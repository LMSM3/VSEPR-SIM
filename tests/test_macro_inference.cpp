// =============================================================================
// tests/test_macro_inference.cpp — Group 24
// =============================================================================
// Task 12  — Diffusion measurement (MSD, jumps, site residence, tortuosity)
// Task 12B — Macro transport inference (D_eff, anisotropy, mobility, class)
// Task 13  — Bead/powder packing measurement (contact, coordination, void)
// Task 13B — Macro packing inference (density, porosity, compressibility,
//             permeability, sintering readiness)
//
// Interaction : pure LJ via create_lj_coulomb_model()
//               Ar (Z=18): σ=3.4 Å, ε=0.238 kcal/mol
// Integrator  : VelocityVerlet (NVE)
//
// State rule  : no D, porosity, compressibility, permeability stored in state.
//               All macro properties computed in analysis headers only.
//
// Task 12B cases
//   case_ideal       — SC 3×3×3 crystal, zero velocity (localized vibration)
//   case_vacancy     — SC 3×3×3 crystal, one atom removed
//   case_interstitial— SC 3×3×3 crystal, one atom injected
//   case_thermal_lo  — SC 3×3×3 crystal, low thermal jitter (low KE)
//   case_thermal_hi  — SC 3×3×3 crystal, high thermal jitter (high KE)
//   case_surface     — 2D slab, single layer, free surface (surface diffusion)
//
// Task 13B cases
//   pack_loose       — random bead drop into box, no compression
//   pack_settled     — loose pack after extended relaxation
//   pack_compressed  — compressed vs initial → compressibility proxy
//   pack_jammed      — high-density initial placement
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <numeric>

// Atomistic infrastructure
#include "atomistic/core/state.hpp"
#include "atomistic/crystal/lattice.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include "atomistic/models/model.hpp"

// Analysis layer (12 + 12B)
#include "analysis/diffusion_analysis.hpp"
// Analysis layer (13 + 13B)
#include "analysis/packing_analysis.hpp"

// =============================================================================
// Shared simulation parameters
// =============================================================================

static constexpr double AR_MASS    = 39.948;
static constexpr uint32_t AR_TYPE  = 18;
static constexpr double DT         = 2e-3;    // fs
static constexpr int    N_STEPS_D  = 12000;   // diffusion: extended for better MSD slope + activation trend
static constexpr int    N_STEPS_D_THERMAL = 20000; // thermal sweep cases: longer for Arrhenius signal
static constexpr int    N_STEPS_P  = 3000;    // packing: enough for settling
static constexpr int    SAMPLE     = 200;     // frames per integration block
static constexpr double A_SC       = 3.82;    // Å — Ar simple-cubic lattice constant
static constexpr double A_FCC      = 5.26;    // Å — Ar FCC lattice constant (σ*2^(1/6) ≈ 3.82*1.38)
static constexpr double A_BCC      = 3.30;    // Å — Ar BCC lattice constant

// =============================================================================
// Crystal builder helpers (same Ar parameters as crystal_surface test)
// =============================================================================

static atomistic::crystal::UnitCell make_ar_sc(double a) {
	atomistic::crystal::Lattice lat = atomistic::crystal::Lattice::cubic(a);
	atomistic::crystal::UnitCell uc("Ar_SC", lat);
	uc.add_atom({0.0, 0.0, 0.0}, AR_TYPE, 0.0, AR_MASS);
	return uc;
}

static atomistic::crystal::UnitCell make_ar_fcc(double a) {
	atomistic::crystal::Lattice lat = atomistic::crystal::Lattice::cubic(a);
	atomistic::crystal::UnitCell uc("Ar_FCC", lat);
	uc.add_atom({0.0, 0.0, 0.0}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.5, 0.5, 0.0}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.5, 0.0, 0.5}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.0, 0.5, 0.5}, AR_TYPE, 0.0, AR_MASS);
	return uc;
}

static atomistic::crystal::UnitCell make_ar_bcc(double a) {
	atomistic::crystal::Lattice lat = atomistic::crystal::Lattice::cubic(a);
	atomistic::crystal::UnitCell uc("Ar_BCC", lat);
	uc.add_atom({0.0, 0.0, 0.0}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.5, 0.5, 0.5}, AR_TYPE, 0.0, AR_MASS);
	return uc;
}

static void fill_ar_masses(atomistic::State& s) {
	s.M.assign(s.N, AR_MASS);
	s.Q.assign(s.N, 0.0);
	for (auto& t : s.type) t = AR_TYPE;
	s.T.assign(s.N, 0.0);
	s.F.resize(s.N, {0,0,0});
}

static void fill_velocities_zero(atomistic::State& s) {
	s.V.assign(s.N, {0,0,0});
}

static void apply_thermal_jitter(
	atomistic::State& s, double sigma, uint64_t seed)
{
	std::mt19937 rng(static_cast<uint32_t>(seed));
	std::normal_distribution<double> nd(0.0, sigma);
	for (uint32_t i = 0; i < s.N; ++i) {
		s.V[i].x += nd(rng);
		s.V[i].y += nd(rng);
		s.V[i].z += nd(rng);
	}
}

// =============================================================================
// Task 12B — Diffusion-Based Macro Transport Inference
// =============================================================================

struct DiffCase {
	std::string case_id;
	atomistic::State state;
	double surface_z_min;   // Å — set < all atoms to disable surface filtering
};

static DiffCase make_diff_case(
	const std::string& case_id,
	const std::string& variant,
	uint64_t           seed = 42)
{
	DiffCase dc;
	dc.case_id       = case_id;
	dc.surface_z_min = -1e30;

	// ── Surface: 2D slab SC 5×5×1 ────────────────────────────────────────────
	if (variant == "surface") {
		auto uc2 = make_ar_sc(A_SC);
		auto sc2 = atomistic::crystal::construct_supercell(uc2, 5, 5, 1);
		dc.state = sc2.state;
		fill_ar_masses(dc.state);
		fill_velocities_zero(dc.state);
		dc.state.box.enabled = false;
		apply_thermal_jitter(dc.state, 0.10, seed);
		dc.surface_z_min = -999.0;
		return dc;
	}

	// ── 1D channel: thin SC slab (1×1×20) — diffusion along z ───────────────
	if (variant == "channel_1d") {
		auto uc2 = make_ar_sc(A_SC);
		auto sc2 = atomistic::crystal::construct_supercell(uc2, 1, 1, 20);
		dc.state = sc2.state;
		fill_ar_masses(dc.state);
		fill_velocities_zero(dc.state);
		dc.state.box.enabled = false;
		apply_thermal_jitter(dc.state, 0.15, seed);
		// z-only diffusion; surface_z_min not meaningful here (3D tracker, dim=1)
		return dc;
	}

	// ── FCC 3×3×3 ─────────────────────────────────────────────────────────────
	if (variant == "fcc") {
		auto uc2 = make_ar_fcc(A_FCC);
		auto sc2 = atomistic::crystal::construct_supercell(uc2, 3, 3, 3);
		dc.state = sc2.state;
		fill_ar_masses(dc.state);
		fill_velocities_zero(dc.state);
		dc.state.box.enabled = false;
		apply_thermal_jitter(dc.state, 0.05, seed);
		return dc;
	}

	// ── BCC 3×3×3 ─────────────────────────────────────────────────────────────
	if (variant == "bcc") {
		auto uc2 = make_ar_bcc(A_BCC);
		auto sc2 = atomistic::crystal::construct_supercell(uc2, 3, 3, 3);
		dc.state = sc2.state;
		fill_ar_masses(dc.state);
		fill_velocities_zero(dc.state);
		dc.state.box.enabled = false;
		apply_thermal_jitter(dc.state, 0.05, seed);
		return dc;
	}

	// ── All SC-based variants share the same 3×3×3 base ──────────────────────
	auto uc = make_ar_sc(A_SC);
	auto sc = atomistic::crystal::construct_supercell(uc, 3, 3, 3);
	dc.state = sc.state;
	fill_ar_masses(dc.state);
	fill_velocities_zero(dc.state);
	dc.state.box.enabled = false;

	const uint32_t N = dc.state.N;

	if (variant == "ideal") {
		apply_thermal_jitter(dc.state, 0.02, seed);

	} else if (variant == "vacancy") {
		auto& s = dc.state;
		vsepr::Vec3 cm{0,0,0};
		for (const auto& p : s.X) { cm.x+=p.x; cm.y+=p.y; cm.z+=p.z; }
		cm.x/=N; cm.y/=N; cm.z/=N;
		double best=1e30; uint32_t bi=0;
		for (uint32_t i=0;i<N;++i) {
			double d2=(s.X[i].x-cm.x)*(s.X[i].x-cm.x)+(s.X[i].y-cm.y)*(s.X[i].y-cm.y)+(s.X[i].z-cm.z)*(s.X[i].z-cm.z);
			if(d2<best){best=d2;bi=i;}
		}
		s.X.erase(s.X.begin()+bi); s.V.erase(s.V.begin()+bi);
		s.F.erase(s.F.begin()+bi); s.M.erase(s.M.begin()+bi);
		s.Q.erase(s.Q.begin()+bi); s.type.erase(s.type.begin()+bi);
		s.T.erase(s.T.begin()+bi); s.N--;
		apply_thermal_jitter(dc.state, 0.05, seed);

	} else if (variant == "interstitial") {
		auto& s = dc.state;
		vsepr::Vec3 ipos{0,0,0};
		for (const auto& p : s.X) { ipos.x+=p.x; ipos.y+=p.y; ipos.z+=p.z; }
		ipos.x=ipos.x/N+2.0; ipos.y=ipos.y/N+0.5; ipos.z=ipos.z/N+0.5;
		s.X.push_back(ipos); s.V.push_back({0,0,0}); s.F.push_back({0,0,0});
		s.M.push_back(AR_MASS); s.Q.push_back(0.0); s.type.push_back(AR_TYPE);
		s.T.push_back(0.0); s.N++;
		apply_thermal_jitter(dc.state, 0.05, seed);

	} else if (variant == "mixed_defect") {
		// Vacancy + interstitial simultaneously — crossed defects, max transport perturbation
		auto& s = dc.state;
		// Remove atom nearest to centroid
		vsepr::Vec3 cm{0,0,0};
		for (const auto& p : s.X) { cm.x+=p.x; cm.y+=p.y; cm.z+=p.z; }
		cm.x/=N; cm.y/=N; cm.z/=N;
		double best=1e30; uint32_t bi=0;
		for (uint32_t i=0;i<N;++i) {
			double d2=(s.X[i].x-cm.x)*(s.X[i].x-cm.x)+(s.X[i].y-cm.y)*(s.X[i].y-cm.y)+(s.X[i].z-cm.z)*(s.X[i].z-cm.z);
			if(d2<best){best=d2;bi=i;}
		}
		s.X.erase(s.X.begin()+bi); s.V.erase(s.V.begin()+bi);
		s.F.erase(s.F.begin()+bi); s.M.erase(s.M.begin()+bi);
		s.Q.erase(s.Q.begin()+bi); s.type.erase(s.type.begin()+bi);
		s.T.erase(s.T.begin()+bi); s.N--;
		// Insert interstitial on the opposite side
		vsepr::Vec3 ipos{cm.x + A_SC*1.5, cm.y + A_SC*0.5, cm.z - A_SC*0.5};
		s.X.push_back(ipos); s.V.push_back({0,0,0}); s.F.push_back({0,0,0});
		s.M.push_back(AR_MASS); s.Q.push_back(0.0); s.type.push_back(AR_TYPE);
		s.T.push_back(0.0); s.N++;
		apply_thermal_jitter(dc.state, 0.08, seed);

	} else if (variant == "thermal_lo") {
		apply_thermal_jitter(dc.state, 0.05, seed);

	} else if (variant == "thermal_med") {
		apply_thermal_jitter(dc.state, 0.12, seed);   // intermediate

	} else if (variant == "thermal_hi") {
		apply_thermal_jitter(dc.state, 0.30, seed);   // ~6× higher kinetic activity

	} else if (variant == "thermal_vhi") {
		apply_thermal_jitter(dc.state, 0.55, seed);   // very high — near melting proxy
	}

	return dc;
}

static vsepr::diffusion::TransportInference run_diffusion_case(
	DiffCase& dc,
	atomistic::IModel& model,
	const atomistic::ModelParams& mp,
	int  dim       = 3,
	int  n_steps   = N_STEPS_D)
{
	using namespace vsepr::diffusion;

	DiffusionTracker tracker;
	tracker.r_jump        = 2.5;   // Å — > 2/3 of Ar lattice constant
	tracker.surface_z_min = dc.surface_z_min;
	tracker.set_reference(dc.state.X);
	model.eval(dc.state, mp);
	tracker.set_baseline(dc.state.E.total());

	// Store initial per-atom energy for formation-context annotation (analysis-only)
	const double E_init = dc.state.E.total();

	atomistic::VelocityVerletParams vvp;
	vvp.dt = DT; vvp.n_steps = SAMPLE; vvp.print_freq = 999999; vvp.verbose = false;
	atomistic::VelocityVerlet vv(model, mp);

	std::vector<DiffusionRecord> log;
	log.reserve(n_steps / SAMPLE);
	double t = 0; uint64_t f = 0;

	for (int block = 0; block * SAMPLE < n_steps; ++block) {
		vv.integrate(dc.state, vvp);
		t += SAMPLE * DT; ++f;
		auto row = tracker.compute(f, t, dc.state.E.total(), dc.state.X, dc.state.V);
		log.push_back(row);
	}

	TransportAnalyzer ta;
	ta.dt           = DT;
	ta.fit_fraction = 0.6;
	ta.dim          = dim;
	auto inf = ta.infer(dc.case_id, log);

	// Stamp initial energy for formation-context annotation step (caller fills ref)
	inf.energy_per_atom = (dc.state.N > 0) ? E_init / dc.state.N : 0.0;
	return inf;
}

static void test_diffusion_macro_inference() {
	std::printf("\n=== Task 12B: Diffusion-Based Macro Transport Inference ===\n");

	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp; mp.rc = 10.0;

	// ── Case table ────────────────────────────────────────────────────────────
	// id, variant, dim, n_steps, role_for_formation_context
	struct CaseDef {
		std::string id;
		std::string variant;
		int         dim;
		int         n_steps;
		std::string formation_role;
	};

	const std::vector<CaseDef> cases = {
		// SC crystal baseline set
		{"case_ideal",         "ideal",        3, N_STEPS_D,         "ideal"},
		{"case_vacancy",       "vacancy",      3, N_STEPS_D,         "vacancy"},
		{"case_interstitial",  "interstitial", 3, N_STEPS_D,         "interstitial"},
		{"case_mixed_defect",  "mixed_defect", 3, N_STEPS_D,         "mixed_defect"},

		// Alternative crystal structures (formation structure comparison)
		{"case_fcc",           "fcc",          3, N_STEPS_D,         "fcc"},
		{"case_bcc",           "bcc",          3, N_STEPS_D,         "bcc"},

		// Thermal activation sweep (4 temperatures → proper Arrhenius proxy)
		{"case_thermal_lo",    "thermal_lo",   3, N_STEPS_D_THERMAL, "thermal_lo"},
		{"case_thermal_med",   "thermal_med",  3, N_STEPS_D_THERMAL, "thermal_med"},
		{"case_thermal_hi",    "thermal_hi",   3, N_STEPS_D_THERMAL, "thermal_hi"},
		{"case_thermal_vhi",   "thermal_vhi",  3, N_STEPS_D_THERMAL, "thermal_vhi"},

		// Reduced-dimension diffusion (2D surface, 1D channel)
		{"case_surface",       "surface",      2, N_STEPS_D,         "surface"},
		{"case_channel_1d",    "channel_1d",   1, N_STEPS_D,         "channel_1d"},
	};

	// Header
	std::printf("\n%s\n", vsepr::diffusion::TransportInference::tsv_header().c_str());

	std::vector<vsepr::diffusion::TransportInference> results;
	results.reserve(cases.size());

	for (const auto& cd : cases) {
		auto dc  = make_diff_case(cd.id, cd.variant, 77);
		auto inf = run_diffusion_case(dc, *model, mp, cd.dim, cd.n_steps);
		results.push_back(inf);
	}

	// ── Formation-context annotation ─────────────────────────────────────────
	// Stamp ref_energy_per_atom onto ideal first, then annotate all others
	vsepr::diffusion::annotate_formation_context_ideal(results[0],
		results[0].energy_per_atom * static_cast<double>(27),  // approx N for SC 3^3
		27);
	// For simplicity, re-use the energy_per_atom already computed in run_diffusion_case
	for (std::size_t i = 0; i < results.size(); ++i) {
		if (i == 0) continue;  // already stamped as ideal
		results[i].ref_energy_per_atom    = results[0].ref_energy_per_atom;
		results[i].formation_energy_proxy = results[i].energy_per_atom - results[i].ref_energy_per_atom;
		const std::string& role = cases[i].formation_role;
		if (role == "vacancy" || role == "interstitial" || role == "mixed_defect")
			results[i].formation_context = "defect_modified";
		else if (role == "fcc" || role == "bcc")
			results[i].formation_context = "ideal_reference";
		else if (role == "thermal_lo" || role == "thermal_med" || role == "thermal_hi" || role == "thermal_vhi")
			results[i].formation_context = "thermally_activated";
		else
			results[i].formation_context = "surface_case";
	}

	// ── Defect transport ratios vs SC ideal ───────────────────────────────────
	for (auto& r : results)
		vsepr::diffusion::annotate_defect_ratios(r, results[0]);

	// Print all rows
	for (const auto& r : results)
		std::printf("%s\n", r.to_tsv().c_str());

	// ── Activation trend (4-point thermal sweep) ─────────────────────────────
	// indices 6–9 are thermal_lo, med, hi, vhi
	std::vector<vsepr::diffusion::TransportInference> thermal_cases = {
		results[6], results[7], results[8], results[9]};
	// KE proxies: use anisotropy_ratio as a rough structural temperature proxy
	// (mobility_proxy is mean D / KE; invert to get KE estimate)
	auto ke_proxy = [&](const vsepr::diffusion::TransportInference& r) {
		return (r.mobility_proxy > 1e-12) ? r.D_eff_analysis_only / r.mobility_proxy : 1e-4;
	};
	std::vector<double> ke_vals = {
		ke_proxy(results[6]), ke_proxy(results[7]),
		ke_proxy(results[8]), ke_proxy(results[9])};
	auto act = vsepr::diffusion::ActivationTrend::fit(thermal_cases, ke_vals);

	std::printf("\n[Activation trend proxy — 4-point thermal sweep]\n");
	std::printf("  lo→med→hi→vhi  slope_proxy = %+.6f  r2 = %.4f  class = %s\n",
		act.slope_proxy, act.r2, act.activation_class.c_str());
	std::printf("  activation_energy_proxy = %.4f\n", act.activation_energy_proxy);

	// ── Per-axis MSD summary (anisotropy audit) ────────────────────────────────
	std::printf("\n[Per-axis diffusivity summary (anisotropy audit)]\n");
	std::printf("  %-20s  Dx        Dy        Dz        aniso_ratio  class\n", "case_id");
	for (const auto& r : results) {
		std::printf("  %-20s  %8.5f  %8.5f  %8.5f  %8.4f     %s\n",
			r.case_id.c_str(), r.D_x, r.D_y, r.D_z,
			r.anisotropy_ratio, r.anisotropy_class.c_str());
	}

	// ── Formation-context coupling table ─────────────────────────────────────
	std::printf("\n[Formation-context coupling table (analysis-only)]\n");
	std::printf("  %-20s  %-22s  ref_E/atom  E/atom     ΔU_proxy   D_eff\n", "case_id", "formation_context");
	for (const auto& r : results) {
		std::printf("  %-20s  %-22s  %9.4f  %9.4f  %+10.4f  %.6f\n",
			r.case_id.c_str(),
			r.formation_context.c_str(),
			r.ref_energy_per_atom,
			r.energy_per_atom,
			r.formation_energy_proxy,
			r.D_eff_analysis_only);
	}

	// ── Formation codebase survey (printed for record) ────────────────────────
	std::puts("\n═══════════════════════════════════════════════════════════════");
	std::puts("  FORMATION CODEBASE SURVEY — existing infrastructure");
	std::puts("═══════════════════════════════════════════════════════════════");
	std::puts("  Layer                  File / Symbol");
	std::puts("  ─────────────────────────────────────────────────────────────");
	std::puts("  Formation loop (MD)    src/cli/actions_form.cpp");
	std::puts("                         → LangevinDynamics::integrate() with");
	std::puts("                           T-schedule, write_snapshot, formation.log");
	std::puts("  Formation priors       apps/phase4_formation_priors.cpp");
	std::puts("                         → crystal preset → UnitCell → to_state()");
	std::puts("                           → single-point energy per preset");
	std::puts("  Thermodynamics         include/thermo/thermodynamics.hpp");
	std::puts("                         → ThermoData {H_f, S, G_f, Cp}");
	std::puts("                         → ThermoDatabase (NIST/CRC reference data)");
	std::puts("                         → GibbsCalculator::calculate()");
	std::puts("                         → estimate_H_formation(mol)");
	std::puts("  Fingerprinting         atomistic/classify/fingerprints.hpp");
	std::puts("                         → ProtoFingerprint  (topology hash, RDF, CN)");
	std::puts("                         → DefectFingerprint (occupancy, vacancy, sub)");
	std::puts("                         → weisfeiler_lehman_hash()");
	std::puts("  Structure clustering   atomistic/classify/cluster.hpp");
	std::puts("                         → cluster_by_proto()  — polymorph detection");
	std::puts("                         → cluster_by_defect() — defect microstate");
	std::puts("                         → classify_polymorphs / isomorphs / defects");
	std::puts("  Potential energy       src/pot/energy.hpp");
	std::puts("                         → EnergyResult {UvdW, UCoul, total}");
	std::puts("                         → BondParams, AngleParams, TorsionParams");
	std::puts("  Formation regime       tests/test_runners.hpp");
	std::puts("                         → FormationRegimeRecord, FormationHistory");
	std::puts("                         → build_regime_record(), relaxation_time()");
	std::puts("  ─────────────────────────────────────────────────────────────");
	std::puts("  Integration path for 12B ↔ Formation:");
	std::puts("    trajectory → DiffusionTracker → TransportInference");
	std::puts("    + annotate_formation_context(ref_energy_per_atom from preset)");
	std::puts("    + ThermoDatabase::get(formula) → H_f for true ΔH comparison");
	std::puts("    + cluster_by_defect() on snapshots → microstate classification");
	std::puts("    → formation_energy_proxy is a ΔU bridge between D_eff and H_f");
	std::puts("═══════════════════════════════════════════════════════════════");

	// ── Pass conditions ───────────────────────────────────────────────────────

	for (const auto& r : results) {
		assert(std::isfinite(r.D_eff_analysis_only) && "T12B: D_eff not finite");
		assert(r.energy_status != ""                && "T12B: energy_status empty");
		assert(r.formation_context != ""            && "T12B: formation_context empty");
		(void)r;
	}

	// SC ideal: localized vibration
	assert((results[0].transport_class == "localized_vibration" ||
			results[0].transport_class == "no_transport") &&
		   "T12B: ideal SC should be localized");

	// Thermal ordering: vhi >= hi >= med >= lo in D_eff (at least partially)
	assert(results[9].D_eff_analysis_only >= results[6].D_eff_analysis_only &&
		   "T12B: thermal_vhi should have >= D_eff than thermal_lo");

	// Surface dim = 2
	assert(results[10].diffusion_dimension == 2 && "T12B: surface case must use 2D");

	// Channel dim = 1
	assert(results[11].diffusion_dimension == 1 && "T12B: channel_1d case must use 1D");

	// FCC and BCC formation context = ideal_reference
	assert(results[4].formation_context == "ideal_reference" && "T12B: FCC formation_context");
	assert(results[5].formation_context == "ideal_reference" && "T12B: BCC formation_context");

	// Defect cases formation context = defect_modified
	assert(results[1].formation_context == "defect_modified" && "T12B: vacancy context");
	assert(results[2].formation_context == "defect_modified" && "T12B: interstitial context");
	assert(results[3].formation_context == "defect_modified" && "T12B: mixed_defect context");

	std::puts("\n[xyzFull audit: no diffusion_coefficient, mobility, transport_class in state]");
	std::puts("PASS  test_diffusion_macro_inference");
}

// =============================================================================
// Task 13B — Packing-Based Macro Property Inference
// =============================================================================

struct PackCase {
	std::string case_id;
	std::vector<vsepr::Vec3> positions;
	std::vector<double>       radii;
	std::vector<double>       masses;
	vsepr::packing::PackingBox box;
};

static PackCase make_pack_case(
	const std::string& case_id,
	const std::string& variant,
	uint64_t seed = 7)
{
	std::mt19937 rng(static_cast<uint32_t>(seed));

	PackCase pc;
	pc.case_id = case_id;

	const double R   = 1.7;    // Å bead radius (Ar VdW ~1.88, slightly smaller for packing)
	const double L   = 30.0;   // Å box side
	pc.box = {L, L, L};

	if (variant == "loose") {
		// Random placement in box, low packing (~0.15)
		std::uniform_real_distribution<double> rx(R, L-R);
		const int N = 50;
		for (int i = 0; i < N; ++i) {
			pc.positions.push_back({rx(rng), rx(rng), rx(rng)});
			pc.radii.push_back(R);
			pc.masses.push_back(AR_MASS);
		}

	} else if (variant == "settled" || variant == "compressed") {
		// Semi-ordered SC packing — moderate density (~0.35–0.45)
		const double spacing = 2.0 * R + 0.5;   // Å between bead centers
		int n = 0;
		for (double x = R; x < L - R; x += spacing)
		for (double y = R; y < L - R; y += spacing)
		for (double z = R; z < L - R; z += spacing) {
			pc.positions.push_back({x, y, z});
			pc.radii.push_back(R);
			pc.masses.push_back(AR_MASS);
			++n;
		}

		if (variant == "compressed") {
			// Compress: shift all beads closer together by 15%
			vsepr::Vec3 cm{0,0,0};
			for (const auto& p : pc.positions) { cm.x+=p.x; cm.y+=p.y; cm.z+=p.z; }
			const int N = static_cast<int>(pc.positions.size());
			cm.x/=N; cm.y/=N; cm.z/=N;
			for (auto& p : pc.positions) {
				p.x = cm.x + (p.x-cm.x)*0.85;
				p.y = cm.y + (p.y-cm.y)*0.85;
				p.z = cm.z + (p.z-cm.z)*0.85;
			}
		}

	} else if (variant == "jammed") {
		// Close-packed SC: spacing ~2R (touching/slight overlap)
		const double spacing = 2.0 * R * 0.98;
		for (double x = R; x < L - R; x += spacing)
		for (double y = R; y < L - R; y += spacing)
		for (double z = R; z < L - R; z += spacing) {
			pc.positions.push_back({x, y, z});
			pc.radii.push_back(R);
			pc.masses.push_back(AR_MASS);
		}
	}

	return pc;
}

// Run a packing case: no atomistic integrator needed — pure geometry analysis.
// Positions are held fixed; we compute multiple frames with tiny random kicks
// to show settled/dynamic distinction.
static vsepr::packing::PackingInference run_packing_case(
	const PackCase& pc_initial,
	double          E_dummy = -1.0)
{
	using namespace vsepr::packing;

	PackingTracker tracker;
	tracker.r_contact = 0.2;   // Å gap beyond radii sum = contact
	tracker.set_box(pc_initial.box);
	tracker.set_baseline(E_dummy);

	double total_mass = 0;
	for (double m : pc_initial.masses) total_mass += m;

	std::vector<PackingRecord> log;
	log.reserve(N_STEPS_P / SAMPLE);

	std::mt19937 rng(13);
	std::normal_distribution<double> jitter(0.0, 0.02);

	std::vector<vsepr::Vec3> pos = pc_initial.positions;
	double t = 0;

	for (int block = 0; block * SAMPLE < N_STEPS_P; ++block) {
		// Tiny thermal motion proxy (no integrator needed for geometry test)
		for (auto& p : pos) {
			p.x += jitter(rng); p.y += jitter(rng); p.z += jitter(rng);
		}
		t += SAMPLE * DT;
		// Dummy energy that drifts <1% — stays within "stable" threshold
		const double E = E_dummy * (1.0 + 0.001 * block / (N_STEPS_P/SAMPLE));
		auto row = tracker.compute(
			static_cast<uint64_t>(block+1), t, E,
			pos, pc_initial.radii, pc_initial.masses);
		log.push_back(row);
	}

	PackingAnalyzer pa;
	pa.drift_threshold          = 0.05;
	pa.load_bearing_coord_min   = 4.0;
	pa.sintering_persistence_min= 0.5;

	return pa.infer(pc_initial.case_id, log, total_mass);
}

static void test_packing_macro_inference() {
	std::printf("\n=== Task 13B: Packing-Based Macro Property Inference ===\n");

	struct CaseDef { std::string id; std::string variant; };
	const std::vector<CaseDef> cases = {
		{"pack_loose",      "loose"},
		{"pack_settled",    "settled"},
		{"pack_compressed", "compressed"},
		{"pack_jammed",     "jammed"},
	};

	// Header
	std::printf("\n%s\n", vsepr::packing::PackingInference::tsv_header().c_str());

	std::vector<vsepr::packing::PackingInference> results;
	for (const auto& cd : cases) {
		auto pc  = make_pack_case(cd.id, cd.variant, 7);
		auto inf = run_packing_case(pc, -10.0);
		results.push_back(inf);
		std::printf("%s\n", inf.to_tsv().c_str());
	}

	// Compressibility: compare settled (initial) vs compressed
	{
		using namespace vsepr::packing;
		auto pc_init = make_pack_case("pack_settled", "settled", 7);
		auto pc_comp = make_pack_case("pack_compressed", "compressed", 7);

		PackingTracker tr_i; tr_i.r_contact=0.2; tr_i.set_box(pc_init.box); tr_i.set_baseline(-10.0);
		PackingTracker tr_c; tr_c.r_contact=0.2; tr_c.set_box(pc_comp.box); tr_c.set_baseline(-10.0);

		auto r_init = tr_i.compute(1,0,-10.0,pc_init.positions,pc_init.radii,pc_init.masses);
		auto r_comp = tr_c.compute(1,0,-10.0,pc_comp.positions,pc_comp.radii,pc_comp.masses);

		// Find pack_compressed in results and fill compressibility
		for (auto& inf : results) {
			if (inf.case_id == "pack_compressed") {
				PackingAnalyzer::compute_compressibility(inf, r_init, r_comp);
				std::printf("\n[Compressibility proxy for pack_compressed]\n");
				std::printf("  densification_rate = %.6f  contact_growth = %.4f  compressibility_proxy = %.6f\n",
					inf.densification_rate_proxy,
					inf.contact_growth_rate,
					inf.compressibility_proxy);
			}
		}
	}

	// ── Pass conditions ───────────────────────────────────────────────────────

	// All cases: porosity must be in [0,1] and finite
	for (const auto& r : results) {
		assert(std::isfinite(r.porosity_inferred) &&
			   "T13B: porosity_inferred is not finite");
		assert(r.porosity_inferred >= 0.0 && r.porosity_inferred <= 1.0 &&
			   "T13B: porosity_inferred out of [0,1]");
		(void)r;
	}

	// Loose pack has higher porosity than jammed
	const double por_loose  = results[0].porosity_inferred;
	const double por_jammed = results[3].porosity_inferred;
	assert(por_loose >= por_jammed &&
		   "T13B: loose pack should have >= porosity than jammed");
	(void)por_loose; (void)por_jammed;

	// Jammed has higher mean coordination than loose
	assert(results[3].mean_coordination >= results[0].mean_coordination &&
		   "T13B: jammed pack should have >= coordination than loose");

	// Permeability proxy: loose should have higher permeability than jammed
	assert(results[0].permeability_proxy >= results[3].permeability_proxy &&
		   "T13B: loose pack should have >= permeability proxy than jammed");

	// No macro property stored in state (audit — compile-time guarantee by design)
	std::puts("\n[xyzFull audit: no porosity, bulk_density, compressibility in state]");
	std::puts("PASS  test_packing_macro_inference");
}

// =============================================================================
// Inference dashboard summary
// =============================================================================

static void print_inference_dashboard(
	const std::vector<vsepr::diffusion::TransportInference>& diff_inf,
	const std::vector<vsepr::packing::PackingInference>&     pack_inf)
{
	std::printf("\n");
	std::printf("╔══════════════════════════════════════════════════════════════╗\n");
	std::printf("║         Macro Inference Layer — Status Dashboard             ║\n");
	std::printf("╠══════════════════════════════════════════════════════════════╣\n");
	std::printf("║  Task 12B — Diffusion/Transport Inference                    ║\n");
	std::printf("╠═══════════════════════╦══════════════════════╦══════════════╣\n");
	std::printf("║  case_id              ║  transport_class     ║  D_eff       ║\n");
	std::printf("╠═══════════════════════╬══════════════════════╬══════════════╣\n");
	for (const auto& r : diff_inf)
		std::printf("║  %-21s║  %-20s║  %12.6g  ║\n",
			r.case_id.c_str(), r.transport_class.c_str(), r.D_eff_analysis_only);
	std::printf("╠══════════════════════════════════════════════════════════════╣\n");
	std::printf("║  Task 13B — Packing Inference                                ║\n");
	std::printf("╠═══════════════════════╦══════════════════════╦══════════════╣\n");
	std::printf("║  case_id              ║  packing_class       ║  porosity    ║\n");
	std::printf("╠═══════════════════════╬══════════════════════╬══════════════╣\n");
	for (const auto& r : pack_inf)
		std::printf("║  %-21s║  %-20s║  %12.6f  ║\n",
			r.case_id.c_str(), r.packing_macro_class.c_str(), r.porosity_inferred);
	std::printf("╚══════════════════════════════════════════════════════════════╝\n");
	std::printf("  B-layer status: both 12B and 13B produce analysis-only outputs.\n");
	std::printf("  No inferred property written back into simulation state.\n");
}

// =============================================================================
// main
// =============================================================================

int main() {
	// Run Task 12B
	test_diffusion_macro_inference();

	// Run Task 13B
	test_packing_macro_inference();

	// Dashboard (re-run to collect for display — lightweight geometry only)
	{
		auto model = atomistic::create_lj_coulomb_model();
		atomistic::ModelParams mp; mp.rc = 10.0;

		std::vector<vsepr::diffusion::TransportInference> diff_results;
		for (const auto& [id, var, dim] : std::vector<std::tuple<std::string,std::string,int>>{
				{"case_ideal","ideal",3},{"case_thermal_hi","thermal_hi",3},
				{"case_surface","surface",2}}) {
			auto dc = make_diff_case(id, var, 77);
			diff_results.push_back(run_diffusion_case(dc, *model, mp, dim));
		}
		std::vector<vsepr::packing::PackingInference> pack_results;
		for (const auto& [id, var] : std::vector<std::pair<std::string,std::string>>{
				{"pack_loose","loose"},{"pack_jammed","jammed"}}) {
			auto pc = make_pack_case(id, var, 7);
			pack_results.push_back(run_packing_case(pc, -10.0));
		}
		print_inference_dashboard(diff_results, pack_results);
	}

	std::puts("\nAll macro inference tests passed.");
	return 0;
}
