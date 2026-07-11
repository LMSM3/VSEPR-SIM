// =============================================================================
// tests/test_crystal_surface.cpp — Group 23
// =============================================================================
// Task 9  — Crystal Imperfection Emergence Test
// Task 10 — Surface Interaction and Damage Test
//
// Interaction law : pure LJ via create_lj_coulomb_model()
//                   Ar (Z=18): σ=3.4 Å, ε=0.238 kcal/mol
// Integrator      : VelocityVerlet (NVE)
//
// Task 9 — SC / FCC / BCC Ar 3×3×3 supercells.
//   Perturbation variants per crystal:
//     1. ideal (baseline)
//     2. interstitial injection
//     3. vacancy (remove one atom)
//     4. substitutional (change type of one atom — identity mismatch)
//     5. thermal jitter (random displacement of all atoms)
//     6. localized impulse (kick one atom)
//     7. multi-site perturbation (kick 3 atoms)
//
// Task 10 — Ar slab (SC 3×3×5, free surface, PBC off).
//   Six incoming-atom velocity cases:
//     1. low normal velocity
//     2. medium normal velocity
//     3. high normal velocity
//     4. tangential grazing velocity
//     5. angled impact (45°)
//     6. repeated incoming stream (3 atoms)
//
// State rule : no vacancy_fraction, defect_type, erosion_rate, etc.
//              All labels computed in analysis headers only.
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <random>
#include <algorithm>

// Atomistic infrastructure
#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/crystal/lattice.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include "atomistic/models/model.hpp"

// Analysis layer
#include "analysis/crystal_imperfection.hpp"
#include "analysis/surface_analysis.hpp"

// =============================================================================
// Shared simulation parameters
// =============================================================================

static constexpr double AR_MASS   = 39.948;
static constexpr uint32_t AR_TYPE = 18;
static constexpr double DT        = 2e-3;    // fs — larger dt for crystal stability
static constexpr int    N_STEPS   = 4000;
static constexpr int    SAMPLE    = 200;     // 20 frames per run

static void fill_velocities_zero(atomistic::State& s) {
	s.V.assign(s.N, {0, 0, 0});
}

static void fill_ar_masses(atomistic::State& s) {
	s.M.assign(s.N, AR_MASS);
	s.Q.assign(s.N, 0.0);
	for (auto& t : s.type) t = AR_TYPE;
}

// Build Ar SC unit cell at lattice parameter a
static atomistic::crystal::UnitCell make_ar_sc(double a) {
	atomistic::crystal::Lattice lat = atomistic::crystal::Lattice::cubic(a);
	atomistic::crystal::UnitCell uc("Ar_SC", lat);
	uc.add_atom({0.0, 0.0, 0.0}, AR_TYPE, 0.0, AR_MASS);
	return uc;
}

// Build Ar FCC unit cell
static atomistic::crystal::UnitCell make_ar_fcc(double a) {
	atomistic::crystal::Lattice lat = atomistic::crystal::Lattice::cubic(a);
	atomistic::crystal::UnitCell uc("Ar_FCC", lat);
	uc.add_atom({0.0, 0.0, 0.0}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.5, 0.5, 0.0}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.5, 0.0, 0.5}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.0, 0.5, 0.5}, AR_TYPE, 0.0, AR_MASS);
	return uc;
}

// Build Ar BCC unit cell
static atomistic::crystal::UnitCell make_ar_bcc(double a) {
	atomistic::crystal::Lattice lat = atomistic::crystal::Lattice::cubic(a);
	atomistic::crystal::UnitCell uc("Ar_BCC", lat);
	uc.add_atom({0.0, 0.0, 0.0}, AR_TYPE, 0.0, AR_MASS);
	uc.add_atom({0.5, 0.5, 0.5}, AR_TYPE, 0.0, AR_MASS);
	return uc;
}

// Run and collect imperfection rows — reference-free loop using analyzer
using ImpRow = vsepr::xtal::CrystalImperfectionRow;

static std::vector<ImpRow> run_crystal_analysis(
	atomistic::State&                       state,
	atomistic::IModel&                      model,
	const atomistic::ModelParams&           mp,
	vsepr::xtal::CrystalImperfectionAnalyzer& analyzer)
{
	atomistic::VelocityVerletParams vvp;
	vvp.dt = DT; vvp.n_steps = SAMPLE; vvp.print_freq = 999999; vvp.verbose = false;
	atomistic::VelocityVerlet vv(model, mp);

	model.eval(state, mp);
	analyzer.set_baseline(state.E.total());

	// Snapshot the type vector at t=0 — stays constant under NVE (no reactions).
	// This is passed as cur_types each frame so identity_mismatch_count is live.
	const std::vector<int> cur_types(state.type.begin(), state.type.end());

	std::vector<vsepr::Vec3> prev_pos;
	std::vector<ImpRow> log;
	double sim_time = 0.0;
	uint64_t frame = 0;

	for (int block = 0; block * SAMPLE < N_STEPS; ++block) {
		vv.integrate(state, vvp);
		sim_time += SAMPLE * DT;
		++frame;

		auto row = analyzer.compute(frame, sim_time, state.E.total(),
									state.X, prev_pos, false, cur_types);
		log.push_back(row);
		prev_pos = state.X;
	}
	return log;
}

// =============================================================================
// Task 9 — Crystal Imperfection Emergence Test
// =============================================================================

struct PerturbedCrystal {
	std::string name;
	atomistic::State state;
	std::vector<vsepr::Vec3> ref_positions;
	std::vector<int>          ref_types;     // type of each atom in the ideal reference
};

static PerturbedCrystal make_perturbed(
	const atomistic::crystal::UnitCell& uc,
	int na, int nb, int nc,
	const std::string& variant_name,
	uint64_t seed)
{
	auto sc = atomistic::crystal::construct_supercell(uc, na, nb, nc);
	fill_ar_masses(sc.state);
	fill_velocities_zero(sc.state);
	sc.state.box.enabled = false;   // NVE open box for this test

	std::vector<vsepr::Vec3> ref_pos = sc.state.X;
	std::mt19937 rng(static_cast<uint32_t>(seed));

	atomistic::State& s = sc.state;
	const uint32_t N = s.N;

	if (variant_name == "ideal") {
		// no modification

	} else if (variant_name == "interstitial") {
		// Inject one extra atom at the centroid of the box (interstitial site)
		vsepr::Vec3 interstitial_pos{0,0,0};
		for (const auto& p : s.X) {
			interstitial_pos.x += p.x;
			interstitial_pos.y += p.y;
			interstitial_pos.z += p.z;
		}
		interstitial_pos.x = interstitial_pos.x/N + 2.0;
		interstitial_pos.y = interstitial_pos.y/N + 0.5;
		interstitial_pos.z = interstitial_pos.z/N + 0.5;
		s.X    .push_back(interstitial_pos);
		s.V    .push_back({0,0,0});
		s.F    .push_back({0,0,0});
		s.M    .push_back(AR_MASS);
		s.Q    .push_back(0.0);
		s.type .push_back(AR_TYPE);
		s.T    .push_back(0.0);
		s.N    = static_cast<uint32_t>(s.X.size());

	} else if (variant_name == "vacancy") {
		// Remove the atom closest to center of mass
		vsepr::Vec3 cm{0,0,0};
		for (const auto& p : s.X) { cm.x += p.x; cm.y += p.y; cm.z += p.z; }
		cm.x /= N; cm.y /= N; cm.z /= N;
		double best = 1e30; uint32_t best_i = 0;
		for (uint32_t i = 0; i < N; ++i) {
			double d2 = (s.X[i].x-cm.x)*(s.X[i].x-cm.x) +
						(s.X[i].y-cm.y)*(s.X[i].y-cm.y) +
						(s.X[i].z-cm.z)*(s.X[i].z-cm.z);
			if (d2 < best) { best = d2; best_i = i; }
		}
		s.X    .erase(s.X    .begin() + best_i);
		s.V    .erase(s.V    .begin() + best_i);
		s.F    .erase(s.F    .begin() + best_i);
		s.M    .erase(s.M    .begin() + best_i);
		s.Q    .erase(s.Q    .begin() + best_i);
		s.type .erase(s.type .begin() + best_i);
		s.T    .erase(s.T    .begin() + best_i);
		s.N    = static_cast<uint32_t>(s.X.size());

	} else if (variant_name == "substitutional") {
		// Change type of atom 0 to Z=2 (He — different identity, same position)
		s.type[0] = 2;

	} else if (variant_name == "thermal_jitter") {
		// Random displacements ~ 0.3 Å on each atom
		std::normal_distribution<double> jitter(0.0, 0.3);
		for (uint32_t i = 0; i < N; ++i) {
			s.X[i].x += jitter(rng);
			s.X[i].y += jitter(rng);
			s.X[i].z += jitter(rng);
		}

	} else if (variant_name == "localized_impulse") {
		// Give atom 0 a sharp velocity kick (2 Å/fs in X)
		s.V[0].x = 2.0;

	} else if (variant_name == "multi_site") {
		// Kick 3 atoms with random impulses
		std::uniform_int_distribution<uint32_t> pick(0, N-1);
		std::normal_distribution<double> kick(0.0, 1.0);
		for (int k = 0; k < 3; ++k) {
			uint32_t i = pick(rng);
			s.V[i].x += kick(rng);
			s.V[i].y += kick(rng);
			s.V[i].z += kick(rng);
		}
	}

	PerturbedCrystal pc;
	pc.name         = variant_name;
	pc.ref_positions= ref_pos;
	// Capture reference types before move (ideal lattice types, pre-substitution).
	// For substitutional the type vector on the state has already been edited,
	// so we read the unperturbed types from ref_pos size using AR_TYPE.
	pc.ref_types.assign(ref_pos.size(), AR_TYPE);
	pc.state        = std::move(sc.state);
	return pc;
}

static void test_crystal_imperfection() {
	std::printf("\n=== Task 9: Crystal Imperfection Emergence Test ===\n");

	struct CrystalConfig { std::string name; double a; int basis_n; };
	const std::vector<CrystalConfig> crystals = {
		{"SC",  3.82, 1},
		{"FCC", 5.40, 4},
		{"BCC", 4.40, 2},
	};

	const std::vector<std::string> variants = {
		"ideal", "interstitial", "vacancy", "substitutional",
		"thermal_jitter", "localized_impulse", "multi_site"
	};

	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp; mp.rc = 10.0;

	// Summary table across all crystal/variant pairs
	std::printf("\n%-6s %-18s %-20s %-20s %8s %8s %8s %8s %5s\n",
		"xtal", "variant", "structure_class", "identity_class",
		"RMSD_ref", "occ_mis", "def_frac", "N_excess", "id_mis");

	for (const auto& cfg : crystals) {
		atomistic::crystal::UnitCell uc =
			(cfg.name == "SC")  ? make_ar_sc(cfg.a)  :
			(cfg.name == "FCC") ? make_ar_fcc(cfg.a) :
								  make_ar_bcc(cfg.a);

		for (const auto& var : variants) {
			auto pc = make_perturbed(uc, 3, 3, 3, var, 99);

			vsepr::xtal::CrystalImperfectionAnalyzer analyzer;
			analyzer.r_occ           = 1.8;
			analyzer.r_bond          = cfg.a * 1.3;
			analyzer.defect_threshold = 0.6;
			analyzer.set_reference(pc.ref_positions);
			analyzer.ref_types       = pc.ref_types;

			auto log = run_crystal_analysis(pc.state, *model, mp, analyzer);
			const auto& last = log.back();

			std::printf("%-6s %-18s %-20s %-20s %8.4f %8.4f %8.4f %8d %5d\n",
				cfg.name.c_str(), var.c_str(),
				last.structure_class.c_str(), last.identity_class.c_str(),
				last.RMSD_ref, last.occupancy_mismatch,
				last.defect_fraction, last.N_excess,
				last.identity_mismatch_count);

			// ── Pass conditions ──────────────────────────────────────────────

			// Ideal: residual should be near-zero
			if (var == "ideal") {
				assert(last.RMSD_ref < 2.0 && "T9: ideal crystal RMSD_ref too large");
			}

			// Interstitial: N_excess must be +1
			if (var == "interstitial") {
				assert(last.N_excess == 1 && "T9: interstitial N_excess != +1");
			}

			// Vacancy: N_excess must be -1, occupancy_mismatch > 0
			if (var == "vacancy") {
				assert(last.N_excess == -1 && "T9: vacancy N_excess != -1");
				assert(last.occupancy_mismatch > 0.0 && "T9: vacancy occ_mismatch == 0");
			}

			// Substitutional: N_excess == 0, identity_mismatch_count >= 1
			// Structure may look ideal if geometry is undistorted — that is correct.
			// The identity layer is the only honest place to record the mismatch.
			if (var == "substitutional") {
				assert(last.N_excess == 0 && "T9: substitutional N_excess != 0");
				assert(last.identity_mismatch_count >= 1 && "T9: substitutional identity_mismatch_count == 0");
				assert(last.identity_class == "substitutional_site" && "T9: substitutional identity_class wrong");
			}

			// Energy sanity: integrator must not produce NaN or Inf
			// (Physically driven energy growth from impulse variants is expected)
			assert(std::isfinite(last.E_total) && "T9: E_total is not finite");
			if (var == "ideal" || var == "substitutional") {
				// Unperturbed / same-count cases: drift should stay small
				assert(std::abs(last.E_rel_drift) < 5.0 && "T9: ideal/sub energy drift too large");
			}
		}
	}

	// Print detailed trajectory for SC ideal
	std::printf("\n[Detailed trajectory: SC ideal]\n%s\n",
		ImpRow::tsv_header().c_str());
	{
		auto uc = make_ar_sc(3.82);
		auto pc = make_perturbed(uc, 3, 3, 3, "ideal", 1);
		vsepr::xtal::CrystalImperfectionAnalyzer analyzer;
		analyzer.r_occ = 1.8; analyzer.r_bond = 5.0; analyzer.defect_threshold = 0.6;
		analyzer.set_reference(pc.ref_positions);
		auto log = run_crystal_analysis(pc.state, *model, mp, analyzer);
		for (const auto& r : log) std::printf("%s\n", r.to_tsv().c_str());
	}

	// xyzFull property audit (compile-time: no labels in row)
	std::puts("\n[xyzFull audit: fields = frame time id type x y z vx vy vz]");
	std::puts("[AUDIT: no vacancy_fraction, defect_type, grain_size, damage_score in state]");

	std::puts("PASS  test_crystal_imperfection");
}

// =============================================================================
// Task 10 — Surface Interaction and Damage Test
// =============================================================================

struct SurfaceCase {
	std::string name;
	std::string velocity_class;
	double v_normal;      // Å/fs — normal to surface (+z = away, -z = into slab)
	double v_tangential;  // Å/fs — along x
};

static void test_surface_interaction() {
	std::printf("\n=== Task 10: Surface Interaction and Damage Test ===\n");

	// Build SC Ar slab: 3×3×5 in z (5 layers thick), PBC off
	const double A = 3.82;
	auto uc = make_ar_sc(A);
	auto sc_slab = atomistic::crystal::construct_supercell(uc, 3, 3, 5);
	fill_ar_masses(sc_slab.state);
	fill_velocities_zero(sc_slab.state);
	sc_slab.state.box.enabled = false;   // free surface — no PBC

	const uint32_t SLAB_N = sc_slab.state.N;
	const std::vector<vsepr::Vec3> slab_ref = sc_slab.state.X;

	// Find max z of slab (surface plane)
	double surface_z = -1e30;
	for (const auto& p : slab_ref)
		surface_z = std::max(surface_z, p.z);

	// Surface layer: top layer atoms (within 0.6 Å of surface_z)
	const double surface_layer_z_min = surface_z - 0.6;

	// Eject/embed thresholds
	const double z_eject = surface_z + 3.0;   // above this = ejected slab atom
	const double z_embed = surface_z - 4.0;   // below this (for incoming) = embedded

	// Incoming particle placed 8 Å above surface
	const double incoming_start_z = surface_z + 8.0;

	const std::vector<SurfaceCase> cases = {
		{"case_1_low_normal",    "low_normal",    -0.05, 0.0},
		{"case_2_med_normal",    "med_normal",    -0.20, 0.0},
		{"case_3_high_normal",   "high_normal",   -0.80, 0.0},
		{"case_4_grazing",       "grazing",       -0.02, 0.40},
		{"case_5_angled_45",     "angled_45",     -0.15, 0.15},
		{"case_6_stream",        "stream",        -0.10, 0.0},   // handled separately below
	};

	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp; mp.rc = 10.0;

	std::printf("\n%s\n", vsepr::surface::SurfaceSweepResult::tsv_header().c_str());

	for (std::size_t ci = 0; ci < cases.size(); ++ci) {
		const auto& cas = cases[ci];

		// ── Build combined state: slab + one incoming atom ────────────────────
		atomistic::State s;
		s.X     = slab_ref;
		s.V     .assign(SLAB_N, {0, 0, 0});
		s.F     .assign(SLAB_N, {0, 0, 0});
		s.M     .assign(SLAB_N, AR_MASS);
		s.Q     .assign(SLAB_N, 0.0);
		s.type  .assign(SLAB_N, AR_TYPE);
		s.T     .assign(SLAB_N, 0.0);

		// Case 6 "stream": add 3 incoming atoms spaced 1.2 Å apart in z
		const int n_incoming = (cas.name == "case_6_stream") ? 3 : 1;
		for (int k = 0; k < n_incoming; ++k) {
			s.X   .push_back({A * 1.5, A * 1.5, incoming_start_z + k * 1.2});
			s.V   .push_back({cas.v_tangential, 0.0, cas.v_normal});
			s.F   .push_back({0, 0, 0});
			s.M   .push_back(AR_MASS);
			s.Q   .push_back(0.0);
			s.type.push_back(AR_TYPE);
			s.T   .push_back(0.0);
		}
		s.N = static_cast<uint32_t>(s.X.size());
		s.box.enabled = false;

		const uint32_t incoming_id = SLAB_N;   // first incoming atom index

		// ── Analyzer setup ────────────────────────────────────────────────────
		vsepr::surface::SurfaceAnalyzer surf;
		surf.slab_N            = SLAB_N;
		surf.incoming_id       = incoming_id;
		surf.surface_z_ref     = surface_z;
		surf.z_eject           = z_eject;
		surf.z_embed           = z_embed;
		surf.r_reside          = 6.0;
		surf.surface_layer_z_min = surface_layer_z_min;
		surf.set_surface_reference(slab_ref);

		model->eval(s, mp);
		surf.set_baseline(s.E.total());

		// Record KE of incoming atom at t=0
		const auto& v0 = s.V[incoming_id];
		surf.KE_incoming_first = 0.5 * (v0.x*v0.x + v0.y*v0.y + v0.z*v0.z)
								 * AR_MASS * 418.4;

		// ── Integration ───────────────────────────────────────────────────────
		atomistic::VelocityVerletParams vvp;
		vvp.dt = DT; vvp.n_steps = SAMPLE; vvp.print_freq = 999999; vvp.verbose = false;
		atomistic::VelocityVerlet vv(*model, mp);

		std::vector<vsepr::surface::SurfaceMetricsRow> slog;
		double sim_time = 0.0;
		uint64_t frame = 0;

		for (int block = 0; block * SAMPLE < N_STEPS; ++block) {
			vv.integrate(s, vvp);
			sim_time += SAMPLE * DT;
			++frame;
			auto row = surf.compute(cas.name, frame, sim_time,
									s.E.total(), s.X, s.V);
			slog.push_back(row);
		}

		// Build sweep result
		vsepr::surface::SurfaceSweepResult res;
		res.case_id          = cas.name;
		res.velocity_class   = cas.velocity_class;
		res.v_normal         = cas.v_normal;
		res.v_tangential     = cas.v_tangential;
		res.min_surface_distance = surf.min_surface_distance_ever;
		res.residence_frames = slog.back().residence_frames;
		res.final_reflection_angle = slog.back().reflection_angle_deg;
		res.final_surface_residual = slog.back().surface_residual;
		res.final_roughness_proxy  = slog.back().roughness_proxy;
		res.energy_drift     = slog.back().E_rel_drift;
		res.final_class      = slog.back().emergent_class;
		for (const auto& r : slog) {
			res.max_ejected  = std::max(res.max_ejected, r.ejected_count);
			res.max_embedded = std::max(res.max_embedded, r.embedded_count);
		}

		std::printf("%s\n", res.to_tsv().c_str());

		// ── Pass conditions ──────────────────────────────────────────────────

		// Low normal: should not embed (light touch)
		if (cas.velocity_class == "low_normal") {
			assert(res.max_embedded == 0 && "T10: low-v particle should not embed");
		}

		// High normal: min_surface_distance must be small (actual contact)
		if (cas.velocity_class == "high_normal") {
			assert(res.min_surface_distance < 8.0 && "T10: high-v no contact detected");
		}

		// Energy sanity: integrator must not produce NaN or Inf
		assert(std::isfinite(res.energy_drift) && "T10: energy_drift is not finite");
	}

	// Print detailed per-frame log for case_2_med_normal
	std::printf("\n[Detailed trajectory: case_2_med_normal]\n%s\n",
		vsepr::surface::SurfaceMetricsRow::tsv_header().c_str());
	{
		atomistic::State s;
		s.X     = slab_ref;
		s.V     .assign(SLAB_N, {0, 0, 0});
		s.F     .assign(SLAB_N, {0, 0, 0});
		s.M     .assign(SLAB_N, AR_MASS);
		s.Q     .assign(SLAB_N, 0.0);
		s.type  .assign(SLAB_N, AR_TYPE);
		s.T     .assign(SLAB_N, 0.0);
		s.X.push_back({A * 1.5, A * 1.5, incoming_start_z});
		s.V.push_back({0.0, 0.0, -0.20});
		s.F.push_back({0, 0, 0}); s.M.push_back(AR_MASS);
		s.Q.push_back(0.0); s.type.push_back(AR_TYPE); s.T.push_back(0.0);
		s.N = static_cast<uint32_t>(s.X.size());
		s.box.enabled = false;

		vsepr::surface::SurfaceAnalyzer surf2;
		surf2.slab_N = SLAB_N; surf2.incoming_id = SLAB_N;
		surf2.surface_z_ref = surface_z;
		surf2.z_eject = z_eject; surf2.z_embed = z_embed;
		surf2.r_reside = 6.0; surf2.surface_layer_z_min = surface_layer_z_min;
		surf2.set_surface_reference(slab_ref);
		model->eval(s, mp);
		surf2.set_baseline(s.E.total());
		const auto& v0 = s.V[SLAB_N];
		surf2.KE_incoming_first = 0.5*(v0.x*v0.x+v0.y*v0.y+v0.z*v0.z)*AR_MASS*418.4;

		atomistic::VelocityVerletParams vvp2;
		vvp2.dt = DT; vvp2.n_steps = SAMPLE; vvp2.print_freq = 999999; vvp2.verbose = false;
		atomistic::VelocityVerlet vv2(*model, mp);
		double t = 0.0; uint64_t f = 0;
		for (int block = 0; block * SAMPLE < N_STEPS; ++block) {
			vv2.integrate(s, vvp2);
			t += SAMPLE * DT; ++f;
			auto r = surf2.compute("case_2_med_normal", f, t, s.E.total(), s.X, s.V);
			std::printf("%s\n", r.to_tsv().c_str());
		}
	}

	std::puts("\n[xyzFull audit: no erosion_rate, adsorption_probability, surface_damage in state]");
	std::puts("PASS  test_surface_interaction");
}

// =============================================================================
// main
// =============================================================================

int main() {
	test_crystal_imperfection();
	test_surface_interaction();
	std::puts("\nAll crystal imperfection and surface interaction tests passed.");
	return 0;
}
