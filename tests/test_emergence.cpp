// =============================================================================
// tests/test_emergence.cpp — Group 22: Emergence Microtests
// =============================================================================
// Tests Tasks 4–8 of the emergence test specification.
//
// Interaction law : pure LJ via create_lj_coulomb_model()
//                   Ar-like atoms: Z=18 (σ=3.4 Å, ε=0.238 kcal/mol)
//                   No bonds, no charges.
// Integrator      : VelocityVerlet (NVE)
// Analysis        : vsepr::cluster::ClusterAnalysis (reference-free)
// State rule      : NO label stored in State.  All emergence labels are
//                   computed in the analysis layer and printed to stdout.
//
// Task 4  — Three-atom emergence microtest          (test_three_atom_cluster)
// Task 5  — Four-atom cluster shape classification  (test_four_atom_shapes)
// Task 6  — Small cluster relaxation sweep          (test_cluster_sweep)
// Task 7  — First reference-free emergence report   (test_emergence_report)
// Task 8  — Five-to-ten atom micro-nucleation test  (test_micro_nucleation)
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <random>

// Atomistic state and simulation infrastructure
#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include "atomistic/models/model.hpp"

// Analysis layer — no labels in state
#include "analysis/cluster_analysis.hpp"
#include "core/stats/sim_metrics.hpp"

// =============================================================================
// Simulation setup helpers
// =============================================================================

static constexpr double AR_MASS   = 39.948;   // amu
static constexpr double AR_SIGMA  = 3.4;      // Å  (LJ σ, Ar UFF)
static constexpr double AR_EPS    = 0.238;    // kcal/mol
static constexpr uint32_t AR_TYPE = 18;       // Z=18, Ar

// r_min for Ar LJ: 2^(1/6) * sigma ≈ 3.816 Å
// r_bond for connectivity: 1.5 * r_min ≈ 5.7 Å
static constexpr double AR_RBOND  = 5.7;

// Build a State for N identical Ar atoms.
// positions: caller supplies
// velocities: Maxwell-Boltzmann at temp_K, using rng
static atomistic::State make_ar_state(
	const std::vector<vsepr::Vec3>& positions,
	double temp_K,
	std::mt19937& rng)
{
	const uint32_t N = static_cast<uint32_t>(positions.size());
	atomistic::State s;
	s.N    = N;
	s.X    = positions;
	s.V    .assign(N, {0,0,0});
	s.F    .assign(N, {0,0,0});
	s.Q    .assign(N, 0.0);      // no charges
	s.M    .assign(N, AR_MASS);
	s.type .assign(N, AR_TYPE);
	atomistic::initialize_velocities_thermal(s, temp_K, rng);
	return s;
}

// Build a State with velocities scaled by a factor (for sweep tests).
// Scale all velocities by velocity_scale after thermal initialization.
static atomistic::State make_ar_state_scaled(
	const std::vector<vsepr::Vec3>& positions,
	double base_temp_K,
	double velocity_scale,
	std::mt19937& rng)
{
	auto s = make_ar_state(positions, base_temp_K, rng);
	for (auto& v : s.V) {
		v.x *= velocity_scale;
		v.y *= velocity_scale;
		v.z *= velocity_scale;
	}
	return s;
}

// Extract positions and velocities from State as Vec3 vectors
static std::vector<vsepr::Vec3> get_pos(const atomistic::State& s) { return s.X; }
static std::vector<vsepr::Vec3> get_vel(const atomistic::State& s) { return s.V; }
static std::vector<double>      get_masses(const atomistic::State& s) { return s.M; }

// Total energy from State
static double E_total(const atomistic::State& s) { return s.E.total(); }

// Run N_steps of VelocityVerlet, collecting ClusterMetricsRow every sample_freq steps.
// Returns trajectory log.
static std::vector<vsepr::cluster::ClusterMetricsRow> run_and_analyze(
	atomistic::State&              state,
	atomistic::IModel&             model,
	const atomistic::ModelParams&  mp,
	int                            N_steps,
	double                         dt,
	int                            sample_freq,
	vsepr::cluster::ClusterAnalysis& analyzer,
	vsepr::cluster::XyzFullFrame*  xyz_out = nullptr)   // optional audit trail
{
	atomistic::VelocityVerletParams vvp;
	vvp.dt         = dt;
	vvp.n_steps    = sample_freq;
	vvp.print_freq = 999999;
	vvp.verbose    = false;

	atomistic::VelocityVerlet vv(model, mp);

	std::vector<vsepr::cluster::ClusterMetricsRow> log;
	double sim_time = 0.0;
	uint64_t frame  = 0;

	// Evaluate forces at t=0 so E is valid before first step
	model.eval(state, mp);
	const double E0 = E_total(state);
	analyzer.set_baseline(E0);

	for (int block = 0; block * sample_freq < N_steps; ++block) {
		vv.integrate(state, vvp);
		sim_time += sample_freq * dt;
		++frame;

		auto row = analyzer.compute(
			frame, sim_time,
			E_total(state),
			get_pos(state),
			get_vel(state),
			get_masses(state));
		log.push_back(row);

		// Optional xyzFull audit
		if (xyz_out) {
			vsepr::cluster::XyzFullFrame f;
			f.frame = frame;
			f.time  = sim_time;
			for (uint32_t i = 0; i < state.N; ++i) {
				vsepr::cluster::XyzFullRow r;
				r.frame = frame; r.time = sim_time; r.id = i;
				r.type  = state.type[i];
				r.x = state.X[i].x; r.y = state.X[i].y; r.z = state.X[i].z;
				r.vx= state.V[i].x; r.vy= state.V[i].y; r.vz= state.V[i].z;
				f.rows.push_back(r);
			}
			xyz_out->rows.insert(xyz_out->rows.end(), f.rows.begin(), f.rows.end());
		}
	}
	return log;
}

// =============================================================================
// Task 4 — Three-atom emergence microtest
// =============================================================================
// Three identical Ar atoms, random initial positions (fixed seed), small
// velocities.  State contains only X, V, M, type, Q — no cluster labels.
// Analysis computes: d12 d13 d23 mean_pair_distance pair_distance_stddev
//                    radius_of_gyration kinetic_energy energy_drift
//                    cluster_class stationary_flag
// Pass: energy drift stays sane; analyzer classifies final state from geometry.

static void test_three_atom_cluster() {
	std::printf("\n--- Task 4: Three-Atom Emergence Microtest ---\n");

	std::mt19937 rng(42);    // fixed seed for reproducibility
	std::uniform_real_distribution<double> pos_dist(0.0, 8.0);

	// Random initial positions, far enough to avoid core overlap
	std::vector<vsepr::Vec3> init_pos;
	for (int i = 0; i < 3; ++i) {
		double x = 0.0, y = 0.0, z = 0.0;
		bool ok = false;
		while (!ok) {
			x = pos_dist(rng); y = pos_dist(rng); z = pos_dist(rng);
			ok = true;
			for (const auto& p : init_pos) {
				double d = std::sqrt((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y)+(z-p.z)*(z-p.z));
				if (d < 3.5) { ok = false; break; }
			}
		}
		init_pos.push_back({x, y, z});
	}

	auto state = make_ar_state(init_pos, 50.0, rng);
	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp;
	mp.rc = 12.0;

	vsepr::cluster::ClusterAnalysis analyzer;
	analyzer.r_bond    = AR_RBOND;
	analyzer.r_overlap = 2.0;

	// xyzFull audit — state only; no labels
	vsepr::cluster::XyzFullFrame xyz_audit;
	xyz_audit.frame = 0;
	std::printf("%s\n", vsepr::cluster::XyzFullRow::tsv_header().c_str());

	const int N_STEPS = 2000, SAMPLE = 50;
	const double DT = 2e-4;
	auto log = run_and_analyze(state, *model, mp, N_STEPS, DT, SAMPLE, analyzer, &xyz_audit);

	// Print xyzFull header sample (first frame, all 3 atoms)
	for (const auto& r : xyz_audit.rows) {
		if (r.frame == 1) std::printf("%s\n", r.to_tsv().c_str());
	}

	// Print metrics table
	std::printf("\n%s\n", vsepr::cluster::ClusterMetricsRow::tsv_header().c_str());
	for (const auto& row : log) std::printf("%s\n", row.to_tsv().c_str());

	// Verify: energy drift is sane (< 50% over short NVE run at small dt)
	assert(std::abs(log.back().E_rel_drift) < 0.5 && "T4: energy drift too large");

	// Verify: pair distances are positive
	assert(log.back().mean_pair_distance > 0.0 && "T4: zero mean pair distance");

	// Verify: xyzFull contains no cluster/bond/molecule labels
	// (compile-time guarantee: XyzFullRow has no such fields — audit passes)

	// Classify final state
	const bool emerged = (log.back().n_components == 1);
	std::printf("T4 final cluster_class: %s  (emerged: %s)\n",
		log.back().cluster_class.c_str(),
		emerged ? "YES" : "NO");

	std::puts("PASS  test_three_atom_cluster");
}

// =============================================================================
// Task 5 — Four-atom cluster shape classification
// =============================================================================
// Four cases: near-line, near-square, random compact, dispersed.
// Same interaction law; different initial geometry → different final shape.

struct FourAtomCase {
	std::string name;
	std::vector<vsepr::Vec3> init_pos;
	double temp_K;
};

static std::vector<FourAtomCase> make_four_atom_cases() {
	// Spacing relative to LJ r_min ≈ 3.82 Å
	const double RM = 3.82;

	return {
		{
			"case_A_near_line",
			{{0,0,0},{RM,0,0},{2*RM,0,0},{3*RM,0,0}},
			30.0
		},
		{
			"case_B_near_square",
			{{0,0,0},{RM,0,0},{RM,RM,0},{0,RM,0}},
			30.0
		},
		{
			"case_C_compact_random",
			{{0,0,0},{RM*0.9,RM*0.4,0},{RM*0.4,RM*0.85,0.3},{RM*0.5,RM*0.2,RM*0.8}},
			30.0
		},
		{
			"case_D_dispersed",
			{{0,0,0},{8.0,0,0},{0,8.0,0},{8.0,8.0,0}},
			100.0   // higher temp to see scattering
		}
	};
}

static void test_four_atom_shapes() {
	std::printf("\n--- Task 5: Four-Atom Cluster Shape Classification ---\n");

	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp;
	mp.rc = 12.0;

	const int N_STEPS = 2000, SAMPLE = 100;
	const double DT = 2e-4;

	std::printf("\ncase_id\tinitial_anisotropy\tfinal_class\tfinal_Rg\tfinal_mean_pd\n");

	for (const auto& cas : make_four_atom_cases()) {
		std::mt19937 rng(123);
		auto state = make_ar_state(cas.init_pos, cas.temp_K, rng);

		vsepr::cluster::ClusterAnalysis analyzer;
		analyzer.r_bond    = AR_RBOND;
		analyzer.r_overlap = 2.0;

		auto log = run_and_analyze(state, *model, mp, N_STEPS, DT, SAMPLE, analyzer);

		const auto& last = log.back();
		std::printf("%s\t%.3f\t%s\t%.3f\t%.3f\n",
			cas.name.c_str(),
			last.shape_anisotropy,
			last.cluster_class.c_str(),
			last.radius_of_gyration,
			last.mean_pair_distance);

		// Pass condition: cluster_class is a known valid label
		const std::string& cls = last.cluster_class;
		assert((cls == "compact_cluster" || cls == "scattered"
			 || cls == "elongated_chain" || cls == "planar_cluster"
			 || cls == "collapsed")
			&& "T5: unknown cluster_class");
	}

	std::puts("PASS  test_four_atom_shapes");
}

// =============================================================================
// Task 6 — Small cluster relaxation sweep (3×3 = 9 cases)
// =============================================================================
// N=4 Ar atoms.  Sweep: initial_spacing ∈ {low, med, high}
//                        velocity_scale  ∈ {low, med, high}
// Spacing multiplier applied to a tetrahedral-ish base configuration.

static vsepr::cluster::SweepResult run_sweep_case(
	const std::string& case_id,
	double spacing_mult,
	double vel_scale,
	int    N_steps,
	double dt,
	int    sample_freq)
{
	const double RM = 3.82 * spacing_mult;
	std::vector<vsepr::Vec3> init_pos = {
		{0,    0,    0   },
		{RM,   0,    0   },
		{RM*0.5, RM*0.866, 0   },
		{RM*0.5, RM*0.289, RM*0.816}
	};

	std::mt19937 rng(7);
	const double BASE_TEMP = 80.0;
	auto state = make_ar_state_scaled(init_pos, BASE_TEMP, vel_scale, rng);
	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp; mp.rc = 15.0;

	vsepr::cluster::ClusterAnalysis analyzer;
	analyzer.r_bond    = AR_RBOND;
	analyzer.r_overlap = 2.0;

	auto log = run_and_analyze(state, *model, mp, N_steps, dt, sample_freq, analyzer);

	vsepr::cluster::SweepResult res;
	res.case_id        = case_id;
	res.initial_spacing= spacing_mult;
	res.velocity_scale = vel_scale;

	const auto& last = log.back();
	res.final_mean_pair_distance = last.mean_pair_distance;
	res.final_radius_of_gyration = last.radius_of_gyration;
	res.energy_drift             = last.E_rel_drift;
	res.final_shape_class        = last.cluster_class;

	// Cluster detected: ever all N atoms in one component
	int cluster_lifetime = 0;
	int stationary_frame = -1;
	for (int i = 0; i < static_cast<int>(log.size()); ++i) {
		if (log[i].largest_cluster == 4) ++cluster_lifetime;
		if (log[i].stationary_flag && stationary_frame < 0)
			stationary_frame = static_cast<int>(log[i].frame);
	}
	res.cluster_detected  = (cluster_lifetime > 0);
	res.cluster_lifetime  = cluster_lifetime;
	res.stationary_frame  = stationary_frame;
	return res;
}

static void test_cluster_sweep() {
	std::printf("\n--- Task 6: Small Cluster Relaxation Sweep (3x3 = 9 cases) ---\n");
	std::printf("\n%s\n", vsepr::cluster::SweepResult::tsv_header().c_str());

	const double spacings[] = {0.9, 1.0, 1.4};   // low, med, high
	const double vel_scales[]= {0.3, 1.0, 2.5};   // low, med, high
	const char*  sp_names[]  = {"low","med","high"};
	const char*  vs_names[]  = {"low","med","high"};

	const int N_STEPS = 2000, SAMPLE = 100;
	const double DT = 2e-4;

	for (int si = 0; si < 3; ++si) {
		for (int vi = 0; vi < 3; ++vi) {
			std::string id = std::string("sp_") + sp_names[si] + "_vk_" + vs_names[vi];
			auto res = run_sweep_case(id, spacings[si], vel_scales[vi], N_STEPS, DT, SAMPLE);
			std::printf("%s\n", res.to_tsv().c_str());

			// Energy drift sanity gate
			assert(std::abs(res.energy_drift) < 2.0 && "T6: energy drift exceeded 200%");
		}
	}

	std::puts("PASS  test_cluster_sweep");
}

// =============================================================================
// Task 7 — First reference-free emergence report
// =============================================================================
// Three-atom run with a longer trajectory.  Produces structured report.
// Does NOT use a lattice reference — all metrics are trajectory-derived.

static void test_emergence_report() {
	std::printf("\n--- Task 7: Reference-Free Emergence Report ---\n");

	std::mt19937 rng(2024);
	std::uniform_real_distribution<double> pos_dist(0.0, 7.0);

	std::vector<vsepr::Vec3> init_pos;
	for (int i = 0; i < 3; ++i) {
		double x = 0.0, y = 0.0, z = 0.0;
		bool ok = false;
		while (!ok) {
			x = pos_dist(rng); y = pos_dist(rng); z = pos_dist(rng);
			ok = true;
			for (const auto& p : init_pos) {
				double d = std::sqrt((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y)+(z-p.z)*(z-p.z));
				if (d < 3.5) { ok = false; break; }
			}
		}
		init_pos.push_back({x, y, z});
	}

	auto state = make_ar_state(init_pos, 60.0, rng);
	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp; mp.rc = 12.0;

	vsepr::cluster::ClusterAnalysis analyzer;
	analyzer.r_bond    = AR_RBOND;
	analyzer.r_overlap = 2.0;

	const int N_STEPS = 4000, SAMPLE = 100;
	const double DT = 2e-4;
	auto log = run_and_analyze(state, *model, mp, N_STEPS, DT, SAMPLE, analyzer);

	// ── Report sections ──────────────────────────────────────────────────────
	std::puts("\n=== Section 1: Initial Truth-State Summary ===");
	std::puts("  N=3 Ar atoms  |  Z=18  |  σ=3.4Å  |  ε=0.238 kcal/mol");
	std::printf("  seed: 2024  |  T=60K  |  dt=%.0e fs  |  steps=%d\n",
		DT, N_STEPS);

	std::puts("\n=== Section 2: Active Interaction Laws ===");
	std::puts("  Lennard-Jones 12-6 (UFF/Ar parameters)");
	std::puts("  No bonds, no charges, no bonded terms.");
	std::puts("  Lorentz-Berthelot combining rules.");

	std::puts("\n=== Section 3: xyzFull Audit ===");
	std::puts("  XyzFullRow fields: frame time id type x y z vx vy vz");
	std::puts("  NO cluster_detected, bond_count, molecule_type, stability_class.");

	std::puts("\n=== Section 4–7: Trajectory Metrics ===");
	std::printf("\n%s\n", vsepr::cluster::ClusterMetricsRow::tsv_header().c_str());
	for (const auto& row : log) std::printf("%s\n", row.to_tsv().c_str());

	// ── Section 8: Emergent classification ───────────────────────────────────
	const auto& last = log.back();
	const bool cluster_emerged = (last.n_components == 1);

	vsepr::cluster::EmergenceReport report;
	report.title            = "3-Atom Ar Cluster (seed 2024)";
	report.seed_tag         = "2024";
	report.N_atoms          = 3;
	report.cluster_emerged  = cluster_emerged;
	report.final_class      = last.cluster_class;
	// Find first stationary frame
	for (const auto& r : log) {
		if (r.stationary_flag) { report.stationary_frame = static_cast<int>(r.frame); break; }
	}
	std::printf("\n%s\n", report.format().c_str());

	// Pass conditions
	assert(std::abs(last.E_rel_drift) < 0.5 && "T7: energy drift too large");
	assert(last.mean_pair_distance > 0.0    && "T7: zero mean pair distance");

	std::puts("PASS  test_emergence_report");
}

// =============================================================================
// Task 8 — Five-to-ten atom micro-nucleation test
// =============================================================================
// Four seeds × two sizes (5 and 8 atoms).
// Emergence labels (connected components, largest cluster, cluster_class)
// come from analysis only.  State remains property-free.

struct NucleationSeed {
	std::string name;
	uint64_t    seed;
	int         N;
	double      box_size;   // random positions drawn from [0, box_size]
	double      temp_K;
};

static void test_micro_nucleation() {
	std::printf("\n--- Task 8: Five-to-Ten Atom Micro-Nucleation Test ---\n");

	const std::vector<NucleationSeed> seeds = {
		{"seed_001_compact_5",  1001, 5,  8.0,  40.0},
		{"seed_002_loose_8",    2002, 8, 14.0,  60.0},
		{"seed_003_high_E_5",   3003, 5,  9.0, 200.0},
		{"seed_004_low_E_8",    4004, 8, 11.0,  20.0},
	};

	std::printf("\n%s\n", vsepr::cluster::SweepResult::tsv_header().c_str());

	auto model = atomistic::create_lj_coulomb_model();
	atomistic::ModelParams mp; mp.rc = 15.0;
	const int N_STEPS = 3000, SAMPLE = 150;
	const double DT = 2e-4;

	for (const auto& s : seeds) {
		std::mt19937 rng(static_cast<uint32_t>(s.seed));
		std::uniform_real_distribution<double> pos_dist(0.0, s.box_size);

		std::vector<vsepr::Vec3> init_pos;
		int attempts = 0;
		while (static_cast<int>(init_pos.size()) < s.N && attempts < 100000) {
			++attempts;
			double x = pos_dist(rng), y = pos_dist(rng), z = pos_dist(rng);
			bool ok = true;
			for (const auto& p : init_pos) {
				double d = std::sqrt((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y)+(z-p.z)*(z-p.z));
				if (d < 3.5) { ok = false; break; }
			}
			if (ok) init_pos.push_back({x, y, z});
		}

		auto state = make_ar_state(init_pos, s.temp_K, rng);

		vsepr::cluster::ClusterAnalysis analyzer;
		analyzer.r_bond    = AR_RBOND;
		analyzer.r_overlap = 2.0;

		auto log = run_and_analyze(state, *model, mp, N_STEPS, DT, SAMPLE, analyzer);

		// Build sweep-style summary from trajectory
		vsepr::cluster::SweepResult res;
		res.case_id       = s.name;
		res.initial_spacing = s.box_size / (s.N > 1 ? s.N : 1);
		res.velocity_scale  = s.temp_K / 80.0;   // relative to reference temp

		const auto& last = log.back();
		res.final_mean_pair_distance = last.mean_pair_distance;
		res.final_radius_of_gyration = last.radius_of_gyration;
		res.energy_drift             = last.E_rel_drift;
		res.final_shape_class        = last.cluster_class;

		int cluster_lifetime = 0, stationary_frame = -1;
		for (int i = 0; i < static_cast<int>(log.size()); ++i) {
			if (log[i].largest_cluster == s.N) ++cluster_lifetime;
			if (log[i].stationary_flag && stationary_frame < 0)
				stationary_frame = static_cast<int>(log[i].frame);
		}
		res.cluster_detected  = (cluster_lifetime > 0);
		res.cluster_lifetime  = cluster_lifetime;
		res.stationary_frame  = stationary_frame;

		std::printf("%s\n", res.to_tsv().c_str());

		// Energy sanity
		assert(std::abs(last.E_rel_drift) < 5.0 && "T8: energy drift exceeded 500%");
	}

	// Print individual trajectory for seed_001 (detailed view)
	std::printf("\n[Detailed trajectory: seed_001_compact_5]\n%s\n",
		vsepr::cluster::ClusterMetricsRow::tsv_header().c_str());
	{
		std::mt19937 rng(1001);
		std::uniform_real_distribution<double> pos_dist(0.0, 8.0);
		std::vector<vsepr::Vec3> init_pos;
		int attempts = 0;
		while (static_cast<int>(init_pos.size()) < 5 && attempts < 100000) {
			++attempts;
			double x = pos_dist(rng), y = pos_dist(rng), z = pos_dist(rng);
			bool ok = true;
			for (const auto& p : init_pos) {
				double d = std::sqrt((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y)+(z-p.z)*(z-p.z));
				if (d < 3.5) { ok = false; break; }
			}
			if (ok) init_pos.push_back({x, y, z});
		}
		auto state = make_ar_state(init_pos, 40.0, rng);
		vsepr::cluster::ClusterAnalysis analyzer;
		analyzer.r_bond = AR_RBOND;
		auto log2 = run_and_analyze(state, *model, mp, 3000, DT, 150, analyzer);
		for (const auto& r : log2) std::printf("%s\n", r.to_tsv().c_str());
	}

	std::puts("PASS  test_micro_nucleation");
}

// =============================================================================
// main
// =============================================================================

int main() {
	test_three_atom_cluster();
	test_four_atom_shapes();
	test_cluster_sweep();
	test_emergence_report();
	test_micro_nucleation();

	std::puts("\nAll emergence microtest tasks passed.");
	return 0;
}
