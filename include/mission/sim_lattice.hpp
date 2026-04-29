#pragma once
/**
 * sim_lattice.hpp
 * ===============
 * V3 — Lattice
 * Scale Mission: Particles, Clouds, Lattice, and Pipe Gas 3
 *
 * Periodic and quasi-periodic solid framework simulation.
 * Does not pretend a solid is "just particles in a box."
 *
 * Physical scope:
 *   - Unit cell or supercell generation
 *   - Defect insertion (vacancy, interstitial, substitutional)
 *   - Thermal displacement (Debye model, mean-square displacement)
 *   - Local bond / neighborhood graph
 *   - Stress marker fields
 *   - Optional radiation / decay-response overlays
 *
 * Core lattice state:
 *   L = {cell, basis, defects, thermal_state, response_field}
 *
 * Stress bands (from mission work order):
 *   Instant    : ideal unit cell, static property lookup
 *   Short_50ms : tiny cell, no defects, one-step pass
 *   Medium_5s  : small supercell, neighbor graph, thermal/stress estimate
 *   Long_5min  : larger supercell, defect campaigns, heating cycle,
 *                property export and ranking
 *
 * Deliverables:
 *   - crystal summary
 *   - lattice energy / coherence table
 *   - defect list
 *   - thermal response map
 *
 * Integrates with:
 *   include/mission/mission_profile.hpp  — shared profile + entity layer
 *   include/data/Crystal.hpp            — Crystal, LatticeVectors, Atom
 *   include/physics/particle_id.hpp     — vacancy / interstitial codes
 *   include/identity/provenance_record.hpp — 3-tier hash audit
 */

#include "mission/mission_profile.hpp"
#include "data/Crystal.hpp"
#include "physics/particle_id.hpp"

#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace vsepr {
namespace mission {
namespace lattice {

// ============================================================================
// RuntimeProfile factory for V3 Lattice
// ============================================================================

inline RuntimeProfile profile_for(MissionScale s) {
    RuntimeProfile p{};
    p.scale               = s;
    p.pairwise_interactions = false; // handled by lattice neighbor graph
    p.export_csv          = true;

    switch (s) {
        case MissionScale::Instant:
            p.max_entities       = 1;     // one unit cell
            p.max_steps          = 0;     // static lookup only
            p.dt                 = 0.0;
            p.high_accuracy      = false;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.0;
            break;

        case MissionScale::Short_50ms:
            p.max_entities       = 64;    // 2×2×2 supercell, typical
            p.max_steps          = 1;
            p.dt                 = 0.1e-12; // 0.1 ps
            p.high_accuracy      = false;
            p.neighbor_list      = true;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = true;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.05;
            break;

        case MissionScale::Medium_5s:
            p.max_entities       = 512;   // 4×4×4 or equivalent
            p.max_steps          = 100;
            p.dt                 = 0.5e-12;
            p.high_accuracy      = true;
            p.neighbor_list      = true;
            p.export_full_history = false;
            p.export_markdown    = true;
            p.export_xyz         = true;
            p.approximation_level = 0;
            p.wall_budget_s      = 5.0;
            break;

        case MissionScale::Long_5min:
            p.max_entities       = 8000;  // ~10×10×10 or larger
            p.max_steps          = 1000;
            p.dt                 = 1.0e-12;
            p.high_accuracy      = true;
            p.neighbor_list      = true;
            p.export_full_history = true;
            p.export_markdown    = true;
            p.export_xyz         = true;
            p.approximation_level = 0;
            p.wall_budget_s      = 300.0;
            break;
    }
    return p;
}

// ============================================================================
// Defect record — typed using ParticleID reserved ladder
// ============================================================================

struct Defect {
    // Which site is affected
    std::size_t site_index;

    // Type: vacancy (-15), interstitial (-16), etc. from ParticleID
    physics::ParticleID type;

    // For interstitials / substitutionals: what species is inserted
    std::string element;

    // Dose / damage that caused this defect (eV/atom, 0 = artificial insertion)
    double formation_energy_eV {0.0};
    double dose_eV_atom        {0.0};

    // Displacement from ideal site (Å)
    std::array<double,3> displacement {0.0, 0.0, 0.0};
};

// ============================================================================
// Lattice site
//   Each site carries: element, ideal position, displaced position,
//   thermal displacement, local stress, and damage index
// ============================================================================

struct LatticeSite {
    std::size_t           index;
    std::string           element;
    std::array<double,3>  r_ideal;      // ideal position (Å, fractional → Cartesian)
    std::array<double,3>  r_displaced;  // current displaced position (Å)
    double                u_rms;        // thermal root-mean-square displacement (Å)
    double                stress;       // local stress marker (GPa equivalent)
    double                damage;       // damage index [0, 1]
    bool                  occupied;     // false = vacancy
    int                   species_code; // Z for element, negative for defect tokens
};

// ============================================================================
// Lattice state  L = {cell, basis, sites, defects, thermal_state}
// ============================================================================

struct LatticeState {
    // Cell geometry
    data::LatticeVectors cell;
    std::string          spacegroup;
    std::string          formula;

    // Sites
    std::vector<LatticeSite> sites;

    // Defects (indexed into sites)
    std::vector<Defect>      defects;

    // Thermal state
    double T_K           {300.0};  // current temperature (K)
    double T_debye_K     {300.0};  // Debye temperature (K)
    double u_rms_mean    {0.0};    // mean thermal displacement (Å)

    // Response field (per-site stress accumulator)
    std::vector<double>  stress_field;    // size = sites.size()

    // Energy
    double cohesive_energy_eV {0.0};     // total cohesive energy per atom (eV)
    double madelung_estimate  {0.0};     // Madelung energy estimate (if ionic)

    // Provenance
    std::size_t          supercell_n {1}; // supercell repetition along each axis
};

// ============================================================================
// Unit cell library — common reference structures
//
// Returns the primitive unit cell for the named crystal.
// Only lattice parameters and basis positions — no simulation logic.
// ============================================================================

struct UnitCellSpec {
    std::string  name;
    std::string  spacegroup;
    std::array<double,3> abc;        // a, b, c in Å
    std::array<double,3> angles_deg; // α, β, γ in degrees
    std::vector<std::pair<std::string,std::array<double,3>>> basis; // {element, {x,y,z} fractional}
    double       cohesive_eV;        // reference cohesive energy per atom (eV)
    double       T_debye_K;          // Debye temperature (K)
};

inline std::vector<UnitCellSpec> unit_cell_library() {
    return {
        // FCC metals
        {"Al-FCC",  "Fm3m",  {4.050,4.050,4.050}, {90,90,90},
            {{"Al",{0.0,0.0,0.0}},{"Al",{0.5,0.5,0.0}},
             {"Al",{0.5,0.0,0.5}},{"Al",{0.0,0.5,0.5}}},
            3.39, 428.0},
        {"Cu-FCC",  "Fm3m",  {3.615,3.615,3.615}, {90,90,90},
            {{"Cu",{0.0,0.0,0.0}},{"Cu",{0.5,0.5,0.0}},
             {"Cu",{0.5,0.0,0.5}},{"Cu",{0.0,0.5,0.5}}},
            3.49, 343.0},
        {"Au-FCC",  "Fm3m",  {4.078,4.078,4.078}, {90,90,90},
            {{"Au",{0.0,0.0,0.0}},{"Au",{0.5,0.5,0.0}},
             {"Au",{0.5,0.0,0.5}},{"Au",{0.0,0.5,0.5}}},
            3.81, 170.0},
        {"Ni-FCC",  "Fm3m",  {3.524,3.524,3.524}, {90,90,90},
            {{"Ni",{0.0,0.0,0.0}},{"Ni",{0.5,0.5,0.0}},
             {"Ni",{0.5,0.0,0.5}},{"Ni",{0.0,0.5,0.5}}},
            4.44, 450.0},
        // BCC metals
        {"Fe-BCC",  "Im3m",  {2.866,2.866,2.866}, {90,90,90},
            {{"Fe",{0.0,0.0,0.0}},{"Fe",{0.5,0.5,0.5}}},
            4.28, 470.0},
        {"W-BCC",   "Im3m",  {3.165,3.165,3.165}, {90,90,90},
            {{"W", {0.0,0.0,0.0}},{"W", {0.5,0.5,0.5}}},
            8.90, 400.0},
        // Ionic / ceramic
        {"NaCl",    "Fm3m",  {5.640,5.640,5.640}, {90,90,90},
            {{"Na",{0.0,0.0,0.0}},{"Cl",{0.5,0.0,0.0}},
             {"Na",{0.0,0.5,0.5}},{"Cl",{0.5,0.5,0.5}},
             {"Na",{0.5,0.0,0.5}},{"Cl",{0.0,0.0,0.5}},
             {"Na",{0.5,0.5,0.0}},{"Cl",{0.0,0.5,0.0}}},
            3.28, 321.0},
        {"MgO",     "Fm3m",  {4.212,4.212,4.212}, {90,90,90},
            {{"Mg",{0.0,0.0,0.0}},{"O", {0.5,0.0,0.0}},
             {"Mg",{0.0,0.5,0.5}},{"O", {0.5,0.5,0.5}},
             {"Mg",{0.5,0.0,0.5}},{"O", {0.0,0.0,0.5}},
             {"Mg",{0.5,0.5,0.0}},{"O", {0.0,0.5,0.0}}},
            5.16, 940.0},
        // Actinide solid (from v4 beta Z=94 nuclear domain)
        {"UO2",     "Fm3m",  {5.470,5.470,5.470}, {90,90,90},
            {{"U", {0.0,0.0,0.0}},{"U", {0.5,0.5,0.0}},
             {"U", {0.5,0.0,0.5}},{"U", {0.0,0.5,0.5}},
             {"O", {0.25,0.25,0.25}},{"O",{0.75,0.75,0.25}},
             {"O", {0.75,0.25,0.75}},{"O",{0.25,0.75,0.75}}},
            8.41, 182.0},
    };
}

// ============================================================================
// Build a LatticeState from a UnitCellSpec + supercell repetition
// ============================================================================

inline LatticeState build_lattice(const UnitCellSpec& spec, std::size_t n = 1) {
    LatticeState ls;
    ls.spacegroup = spec.spacegroup;
    ls.formula    = spec.name;
    ls.T_debye_K  = spec.T_debye_K;
    ls.cohesive_energy_eV = spec.cohesive_eV;
    ls.supercell_n = n;

    // Cell vectors (orthogonal approximation — full triclinic trivially extensible)
    ls.cell.a = {static_cast<float>(spec.abc[0] * n), 0.0f, 0.0f};
    ls.cell.b = {0.0f, static_cast<float>(spec.abc[1] * n), 0.0f};
    ls.cell.c = {0.0f, 0.0f, static_cast<float>(spec.abc[2] * n)};

    std::size_t site_idx = 0;
    for (std::size_t ix = 0; ix < n; ++ix)
    for (std::size_t iy = 0; iy < n; ++iy)
    for (std::size_t iz = 0; iz < n; ++iz) {
        for (auto& [elem, frac] : spec.basis) {
            LatticeSite s;
            s.index   = site_idx++;
            s.element = elem;
            s.r_ideal = {
                (frac[0] + ix) * spec.abc[0],
                (frac[1] + iy) * spec.abc[1],
                (frac[2] + iz) * spec.abc[2],
            };
            s.r_displaced = s.r_ideal;
            s.u_rms       = 0.0;
            s.stress      = 0.0;
            s.damage      = 0.0;
            s.occupied    = true;
            s.species_code = 0; // element Z resolved externally
            ls.sites.push_back(s);
        }
    }
    ls.stress_field.assign(ls.sites.size(), 0.0);
    return ls;
}

// ============================================================================
// Thermal displacement channel — Debye model
//
// u_rms = sqrt( (3 ħ²) / (m k_B θ_D) * D(θ_D/T) )
// Here we use the high-T classical limit: u_rms = sqrt(3 k_B T / (m ω_D²))
// In simplified form: u_rms ≈ A * sqrt(T / T_Debye)  in Å
//
// Contract:
//   Required state: T_K, T_debye_K, sites
//   Writes:         u_rms on each site, u_rms_mean
//   Local only:     yes
//   dt_max:         large (thermodynamic, no instability)
// ============================================================================

inline void apply_thermal_displacement(LatticeState& ls) {
    // A ~ 0.08 Å is typical for metals at T/T_D ~ 1
    const double A = 0.08;
    const double ratio = (ls.T_debye_K > 0.0)
        ? std::sqrt(ls.T_K / ls.T_debye_K)
        : 0.0;
    const double u = A * ratio;
    for (auto& s : ls.sites) s.u_rms = u;
    ls.u_rms_mean = u;
}

// ============================================================================
// Defect insertion channel
//
// Inserts a vacancy or interstitial at a given site index.
// Uses ParticleID::vacancy and ParticleID::interstitial from the v4 beta
// reserved negative ladder.
//
// Contract:
//   Required state: sites (must be non-empty)
//   Writes:         sites[idx].occupied, defects list
//   Topology-mutating: yes — rebuild neighbor graph after calling
// ============================================================================

inline void insert_vacancy(LatticeState& ls, std::size_t site_idx) {
    if (site_idx >= ls.sites.size()) return;
    ls.sites[site_idx].occupied    = false;
    ls.sites[site_idx].species_code =
        static_cast<int>(physics::ParticleID::vacancy); // -15
    Defect d;
    d.site_index  = site_idx;
    d.type        = physics::ParticleID::vacancy;
    d.element     = ls.sites[site_idx].element;
    d.formation_energy_eV = 1.0; // placeholder — proper calculation is material-specific
    ls.defects.push_back(d);
}

inline void insert_interstitial(LatticeState& ls,
                                 std::size_t near_site_idx,
                                 const std::string& element,
                                 double formation_eV = 2.0)
{
    // Add a new site offset from the near site by half a lattice parameter
    LatticeSite s;
    s.index   = ls.sites.size();
    s.element = element;
    const auto& r0 = ls.sites[near_site_idx].r_ideal;
    s.r_ideal = {r0[0] + 0.5, r0[1] + 0.5, r0[2] + 0.5};
    s.r_displaced = s.r_ideal;
    s.u_rms    = ls.u_rms_mean;
    s.occupied = true;
    s.species_code = static_cast<int>(physics::ParticleID::interstitial); // -16

    ls.sites.push_back(s);
    ls.stress_field.push_back(0.0);

    Defect d;
    d.site_index          = s.index;
    d.type                = physics::ParticleID::interstitial;
    d.element             = element;
    d.formation_energy_eV = formation_eV;
    ls.defects.push_back(d);
}

// ============================================================================
// Stress accumulation channel — simple pairwise distance model
//
// For each site, sums signed deviation of neighbour distances from ideal.
// Writes local stress marker to stress_field[i].
//
// Contract:
//   Required state: sites.r_displaced, stress_field sized
//   Writes:         stress_field, sites[i].stress
//   Pairwise:       yes (short-range cutoff)
//   dt_max:         large
// ============================================================================

inline void apply_stress_accumulation(LatticeState& ls, double cutoff_A = 3.5) {
    const std::size_t N = ls.sites.size();
    std::fill(ls.stress_field.begin(), ls.stress_field.end(), 0.0);

    // Compute ideal nearest-neighbour distance as reference
    double d_ref = 0.0;
    if (N >= 2) {
        const auto& a = ls.sites[0].r_ideal;
        const auto& b = ls.sites[1].r_ideal;
        double dx=a[0]-b[0], dy=a[1]-b[1], dz=a[2]-b[2];
        d_ref = std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    if (d_ref < 1e-6) d_ref = 2.5;

    for (std::size_t i = 0; i < N; ++i) {
        if (!ls.sites[i].occupied) continue;
        double sigma_i = 0.0;
        int    n_nb    = 0;
        const auto& ri = ls.sites[i].r_displaced;
        for (std::size_t j = 0; j < N; ++j) {
            if (i == j || !ls.sites[j].occupied) continue;
            const auto& rj = ls.sites[j].r_displaced;
            double dx=ri[0]-rj[0], dy=ri[1]-rj[1], dz=ri[2]-rj[2];
            double d = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (d < cutoff_A) {
                sigma_i += (d - d_ref) / d_ref;
                ++n_nb;
            }
        }
        if (n_nb > 0) sigma_i /= n_nb;
        ls.stress_field[i]  = sigma_i;
        ls.sites[i].stress  = sigma_i;
    }
}

// ============================================================================
// Lattice system
// ============================================================================

struct LatticeSystem {
    LatticeState   state;
    RuntimeProfile profile;
    std::size_t    step     {0};
    double         sim_time {0.0};
};

// ============================================================================
// Main run — deterministic lattice scheduler
// ============================================================================

inline MissionDeliverable run(LatticeSystem& sys) {
    MissionDeliverable d{};
    d.sim_version  = "V3_Lattice";
    d.scale        = sys.profile.scale;
    d.entity_count = sys.state.sites.size();

    auto t_start = std::chrono::steady_clock::now();

    if (sys.profile.max_steps == 0) {
        // Instant: static property evaluation
        apply_thermal_displacement(sys.state);
        apply_stress_accumulation(sys.state);
    } else {
        // Heating cycle: ramp temperature from 300 K to 2×300 K
        const double T_start = sys.state.T_K;
        const double T_end   = T_start * 2.0;
        const double dT      = (T_end - T_start) / sys.profile.max_steps;

        for (std::size_t step = 0; step < sys.profile.max_steps; ++step) {
            sys.state.T_K += dT;
            apply_thermal_displacement(sys.state);
            apply_stress_accumulation(sys.state);

            sys.step     = step + 1;
            sys.sim_time = (step + 1) * sys.profile.dt;

            if (sys.profile.wall_budget_s > 0.0) {
                auto elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - t_start).count();
                if (elapsed > sys.profile.wall_budget_s) break;
            }
        }
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    d.steps_run       = sys.step;
    d.wall_time_s     = elapsed;
    d.energy_total    = sys.state.cohesive_energy_eV *
                        static_cast<double>(sys.state.sites.size());
    d.temperature_out = sys.state.T_K;
    d.defect_count    = static_cast<int>(sys.state.defects.size());
    d.converged       = true;

    // RMS thermal displacement as conformer entropy proxy
    d.conformer_entropy = sys.state.u_rms_mean;

    return d;
}

// ============================================================================
// Console report
// ============================================================================

inline std::string report(const LatticeSystem& sys, const MissionDeliverable& d) {
    const auto& ls = sys.state;
    std::ostringstream o;
    o << "\n  V3 Lattice — " << mission_scale_name(sys.profile.scale) << "\n";
    o << "  " << std::string(60, '-') << "\n";
    o << "  Formula   : " << ls.formula << "  " << ls.spacegroup << "\n";
    o << "  Supercell : " << ls.supercell_n << "×" << ls.supercell_n
      << "×" << ls.supercell_n << "  (" << d.entity_count << " sites)\n";
    o << "  T         : " << std::fixed << std::setprecision(1) << ls.T_K << " K\n";
    o << "  T_Debye   : " << ls.T_debye_K << " K\n";
    o << "  u_rms     : " << std::setprecision(4) << ls.u_rms_mean << " Å\n";
    o << "  E_coh     : " << ls.cohesive_energy_eV << " eV/atom\n";
    o << "  Defects   : " << ls.defects.size() << "\n";
    if (!ls.defects.empty()) {
        o << "  Defect list:\n";
        for (const auto& df : ls.defects) {
            o << "    site " << df.site_index << "  "
              << physics::to_string(df.type)
              << "  " << df.element
              << "  Ef=" << df.formation_energy_eV << " eV\n";
        }
    }
    o << "  Wall time : " << std::setprecision(4) << d.wall_time_s << " s\n";

    // Stress field summary
    if (!ls.stress_field.empty()) {
        double s_max = *std::max_element(ls.stress_field.begin(), ls.stress_field.end());
        double s_min = *std::min_element(ls.stress_field.begin(), ls.stress_field.end());
        double s_sum = 0.0;
        for (double v : ls.stress_field) s_sum += v;
        double s_mean = s_sum / ls.stress_field.size();
        o << "  Stress field: min=" << s_min << "  max=" << s_max
          << "  mean=" << s_mean << "\n";
    }
    return o.str();
}

} // namespace lattice
} // namespace mission
} // namespace vsepr
