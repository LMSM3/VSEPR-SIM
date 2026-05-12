#pragma once
/**
 * vsim_document.hpp — Parsed representation of a .vsim run script
 * ================================================================
 *
 * A .vsim file is a TOML-subset declarative script that describes a
 * complete VSEPR-SIM run: project identity, simulation parameters,
 * and export targets.
 *
 * Canonical section order:
 *
 *   [project]          — name, version, seed, determinism
 *   [simulation]       — molecules, formation schedule, temperature
 *   [export]           — data file outputs (xyz, json, tsv, md, ...)
 *   [export.visual]    — rendered artifact outputs (svg, png, html, gif, webgl, ...)
 *   [visual]           — interactive display modes (terminal, GL, web)
 *   [defaults.run]     — default run settings merged into every [test.*]
 *   [defaults.analysis]— default analysis flags merged into every [test.*]
 *   [test.<name>]      — golden test entry (structure, group, formula, geometry)
 *   [test.<name>.run]  — per-test run overrides
 *   [test.<name>.analysis] — per-test analysis flags
 *   [suite]            — group-based test selection, run modes, ordering
 *   [suite.limits]     — per-test timeout, fail_fast, continue_on_warning
 *   [suite.smoke]      — fast smoke subset (groups + purpose)
 *   [report]           — report content flags (manifest, hashes, mismatches)
 *
 * The document model is a plain aggregate. No hidden state, no
 * virtual dispatch. Every field has a clear default and is
 * individually inspectable.
 *
 * Design rule: VsimDocument stores WHAT the script says.
 * The runner decides WHAT TO DO with it.
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include "vsim_value.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// [cell] section — simulation box geometry
//
// Defines the orthorhombic periodic cell used for PBC runs.
// Only type = "orthorhombic" is supported in beta-8.
//
// WO-VSEPR-SIM-57B
// ============================================================================

struct CellSection {
	std::string type    = "orthorhombic"; // "orthorhombic" only; "triclinic" reserved
	double      lx      = 0.0;            // Box length X (Å)
	double      ly      = 0.0;            // Box length Y (Å)
	double      lz      = 0.0;            // Box length Z (Å)
	std::string units   = "angstrom";     // "angstrom" supported; "nm" reserved

	bool has_cell() const { return lx > 0.0 && ly > 0.0 && lz > 0.0; }
};

// ============================================================================
// [boundary] section — per-axis boundary mode
//
// Values: "periodic" | "open" | "reflective" (reserved) | "absorbing" (reserved)
// Compact form: mode = "periodic", axes = "x,y,z"
//
// WO-VSEPR-SIM-57B
// ============================================================================

struct BoundarySection {
	std::string x = "open";   // axis boundary mode string
	std::string y = "open";
	std::string z = "open";

	bool any_periodic() const {
		return x == "periodic" || y == "periodic" || z == "periodic";
	}

	bool all_periodic() const {
		return x == "periodic" && y == "periodic" && z == "periodic";
	}
};

// ============================================================================
// [pbc] section — PBC runtime options
//
// WO-VSEPR-SIM-57B
// ============================================================================

// When to remap particle positions into [0, L)
enum class WrapMode {
	Never,
	AfterStep,    // default — wrap after every integration step
	AfterForce,
	OnExport
};

struct PBCSection {
	bool     minimum_image         = true;
	WrapMode wrap_positions        = WrapMode::AfterStep;
	bool     track_images          = true;
	bool     unwrap_for_diffusion  = true;

	// Parsed string for wrap_positions (before enum conversion)
	std::string wrap_positions_str = "after_step";
};

// ============================================================================
// [project] section
// ============================================================================

struct ProjectSection {
	std::string name;                    // e.g. "demo_03_graphite_stack"
	std::string version;                 // e.g. "v5.0.0-beta.8"
	uint64_t    seed_base    = 0;        // Base RNG seed for reproducibility
	bool        determinism  = true;     // Always true — field kept for schema completeness
	std::string description;             // Optional human note

	bool has_name()    const { return !name.empty(); }
	bool has_version() const { return !version.empty(); }
};

// ============================================================================
// [simulation] section — molecule / system spec
// ============================================================================

struct MoleculeEntry {
	std::string formula;        // e.g. "C6H12", "C", "Li2BeF4"
	int         count   = 1;    // Number of copies
	double      temperature_K = 300.0;  // Formation temperature
	std::string lattice;        // Optional lattice hint: "hexagonal", "BCC", "FCC", "none"
	std::string layer_mode;     // Optional stacking mode: "AB", "AA", "turbostratic"
	int         n_layers = 1;   // Layer count (graphene/graphite stacks)

	// ── Directed injection ───────────────────────────────────────────────────
	// region: named placement zone within the simulation box.
	//   Supported values: "corner_xpyp" | "corner_xpyn" | "corner_xnyp" | "corner_xnyn"
	//   (x+/x- = positive/negative X half; y+/y- = positive/negative Y half)
	//   Empty string = auto-placement (default, legacy behaviour).
	std::string region;

	// velocity_drift: inward-directed speed (Å/fs) added to each atom's
	//   Maxwell-Boltzmann velocity so the gas jet points toward the box centre.
	//   0.0 = no drift (default).
	double      velocity_drift = 0.0;
};

struct SimulationSection {
	std::vector<MoleculeEntry> molecules;   // Ordered list of species
	int    fire_max_steps   = 500;          // FIRE relaxation step limit
	double fire_dt_fs       = 1.0;          // Initial FIRE timestep (fs)
	double box_size_ang     = 50.0;         // Cubic box edge (Å); 0 = auto
	bool   periodic         = false;        // Periodic boundary conditions

	// ── Ewald summation (ionic PBC systems) ─────────────────────────────────
	bool   use_ewald        = false;        // Enable Ewald long-range Coulomb
	double ewald_alpha      = 0.3;          // Splitting parameter (Å⁻¹)
	double ewald_rcut       = 10.0;         // Real-space cutoff (Å)
	int    ewald_kmax       = 5;            // k-vector range per axis

	std::string formation_preset;           // Named preset: "metal", "polymer", "ceramic", etc.

	// ── UX pacing ────────────────────────────────────────────────────────────
	int    step_delay_ms    = 0;   // Artificial sleep between FIRE steps (ms); 0 = off
	int    resim_delay_ms   = 400; // Pause before a resim (ms) — lets user see the diff
	bool   smooth_resim     = true;// Fade event spine between resims (terminal animation)
};

// ============================================================================
// [visual.external] section — output-side visual requests
//
// Allows scripts to request rendered output artifacts from the current
// simulation state WITHOUT performing any physics. Output only.
//
// render_targets values:
//   "state_current"     — render current particle positions as SVG/PNG
//   "trajectory_last"   — render last N frames of trajectory
//   "energy_trace"      — energy-per-step trace
//   "rdf"               — radial distribution function
//   "defect_map"        — defect site overlay
//   "cluster_scatter"   — cluster assignment scatter
//   "packing_heatmap"   — packing fraction heatmap
//   "overlay_cycle"     — full overlay-cycle figure
//   "dashboard"         — HTML dashboard
//   "report"            — HTML report
//
// export_format values: "svg" | "png" | "html" | "auto"
// ============================================================================

struct VisualExternalSection {
	bool enabled = false;
	std::vector<std::string> render_targets;   // ordered list of render requests
	std::string export_format = "auto";        // "svg" | "png" | "html" | "auto"
	bool export_frame_png     = false;         // shorthand: write PNG snapshot of current frame
	bool export_trajectory    = false;         // shorthand: write trajectory GIF
	bool show_progress        = true;          // print [render] lines to terminal
	int  render_interval      = 1;             // steps; overrides [visual].render_interval for external backends

	bool any_active() const { return enabled || !render_targets.empty() || export_frame_png; }

	bool should_render(int step, int visual_interval = 1) const {
		int ri = render_interval > 0 ? render_interval : visual_interval;
		if (ri <= 0) ri = 1;
		return (step % ri) == 0;
	}
};

// ============================================================================
// [variance] section — statistical spread / instability measurements
//
// Declares variance probes that the runtime evaluates over the event log.
//
// field values:
//   "energy.total"      — total potential energy per frame
//   "position.x/y/z"   — coordinate component across particles
//   "displacement"      — per-particle displacement from initial position
//   "eta"               — order parameter per frame
//   "coordination"      — avg coordination number per frame
//   "result"            — event result_value series
//
// window:
//   "all"               — all recorded frames
//   "last N"            — last N frames
//   "frames M..N"       — frame range
// ============================================================================

struct VarianceProbe {
	std::string name;       // user label, e.g. "energy_var"
	std::string field;      // what to measure: "energy.total", "displacement", …
	std::string window;     // "all" | "last 50" | "frames 10..200"
	double      threshold   = 0.0;    // used by while-loop guard; 0 = no guard
	std::string particle_group;       // optional: filter to named group
};

struct VarianceSection {
	std::vector<VarianceProbe> probes;
	bool print_results = true;   // print computed variance values after eval
};

// ============================================================================
// [N_evolution] section — population growth-rate tracking
//
// Tracks dN/dt (discrete: ΔN/Δt) for named entity populations.
//
// target values:
//   "cluster_count"     — number of clusters
//   "defect_count"      — number of defect sites
//   "particle_count"    — total particle count
//   "event_count"       — total kernel events emitted
//   "vapor"/"solid"     — particles in a given phase label
//
// window: same as variance — "all" | "last N" | "frames M..N"
// ============================================================================

struct NEvolutionProbe {
	std::string name;       // user label, e.g. "cluster_growth"
	std::string target;     // what population: "cluster_count", "defect_count", …
	std::string window;     // frame window
	std::string where_type; // optional filter: "vapor", "solid", …
	double      threshold   = 0.0;   // for while-loop guard
};

struct NEvolutionSection {
	std::vector<NEvolutionProbe> probes;
	bool print_results = true;
};

// ============================================================================
// [while] section — conditional simulation continuation
//
// Declares one or more while-loop guards.  The runtime evaluates the
// condition BEFORE each iteration and continues until it is false or
// max_iterations is reached.
//
// condition syntax (string):
//   "variance <probe_name> > <value>"
//   "N_evolution <probe_name> > <value>"
//   "energy_drift > <value>"
//   "temperature < <value>"
//   "iteration < N"
//
// body_steps: number of simulation steps to run per iteration
// measure:    probe name(s) to re-evaluate after each body execution
// ============================================================================

struct WhileGuard {
	std::string name;              // user label
	std::string condition;         // full condition string
	int         body_steps  = 100; // FIRE steps per loop body
	int         max_iters   = 20;  // safety ceiling
	std::vector<std::string> measure; // probes to re-evaluate per iteration
	int         iter_delay_ms = 200;  // UX pause between iterations
};

struct WhileSection {
	std::vector<WhileGuard> guards;
};

// ============================================================================
// [batch] section — parameter sweeps and queued job sets
//
// Declares batch jobs: named groups of runs with parameter variation.
//
// sweep_params keys: "lattice", "defect", "temperature", "seed", "count", "formula"
// sweep_values:      space-separated list of values per key
//
// per_run actions:   "analyze rmsd", "analyze variance displacement",
//                    "export report", "export svg", "measure N_evolution"
// ============================================================================

struct BatchJob {
	std::string name;                   // job label, e.g. "crystal_imperfection_sweep"
	std::map<std::string,std::vector<std::string>> sweep_params; // key → value list
	std::vector<std::string> per_run_actions;  // actions after each run
	int   seed_count  = 1;             // seeds per parameter combination
	bool  export_each = false;         // export artifacts for every run
	bool  aggregate   = true;          // produce aggregate report
};

struct BatchSection {
	std::vector<BatchJob> jobs;
	bool print_plan   = true;   // print the full batch plan before executing
	bool abort_on_fail = false; // stop entire batch on first invalid run
};

// ============================================================================
// Batch Manifest Runner — WO-B9-001
//
// Drives batch_manifest.json → per-run isolated folders → summary tables.
//
// batch_manifest.json schema:
//
//   {
//     "batch_id":    "B9_REACTOR_CHANNEL_001",
//     "description": "...",
//     "base_vsim":   "scripts/reactor_channel.vsim",   // (optional, informational)
//     "sweep": [
//       { "param": "temperature_K",  "values": ["300", "600", "900"] },
//       { "param": "pressure_GPa",   "values": ["0.0", "1.0"] }
//     ],
//     "seeds":       3,
//     "score_by":    "energy",          // "energy" | "convergence" | "composite"
//     "output_root": "runs"
//   }
//
// Produces:
//   runs/B9_REACTOR_CHANNEL_001/
//     run_0001/ ... run_NNNN/
//       run_meta.json
//       metrics.tsv
//     batch_summary.tsv
//     ranked_candidates.tsv
//     batch_report.md
//
// WO-B9-001  |  beta-9  |  VSIM-WO-B9-001
// ============================================================================

// ============================================================================
// WO-VSEPR-SIM-62B — Batch Verification Aggregation Bridge
//
// New batch schema sections consumed by the aggregation kernel.
// Must be declared before BatchManifestSection which embeds them.
// ============================================================================

// [batch.verify_policy] — run-level failure response
struct BatchVerifyPolicySection {
	std::string on_run_fail   = "continue"; // "continue" | "abort"
	std::string on_check_fail = "record";   // "record" | "reject_candidate" | "abort"
	bool save_resolved_scripts = true;
};

// [batch.override.verify.*] — per-check threshold overrides
struct BatchOverrideVerifySection {
	double tolerance_A           = -1.0; // -1 = inherit from template
	double relative_tolerance    = -1.0;
	double max_msd_A2            = -1.0;
	int    coordination_tolerance = -1;
};

// [batch.aggregate.verify.gates] — batch-level pass-rate thresholds
struct BatchAggregateVerifyGatesSection {
	double min_overall_pass_rate   = 0.0; // 0.0 = disabled
	double min_mass_pass_rate      = 0.0;
	double min_structure_pass_rate = 0.0;
	double min_rdf_pass_rate       = 0.0;
	double min_msd_pass_rate       = 0.0;
};

// [batch.aggregate.verify] — aggregation control
struct BatchAggregateVerifySection {
	bool enabled                    = false;
	std::vector<std::string> group_by;
	std::vector<std::string> statistics = {"pass_rate", "fail_count"};
	bool emit_matrix                = true;
	bool emit_failure_modes         = true;
	BatchAggregateVerifyGatesSection gates;
};

// [[batch.score.metric.*]] — weighted score contributor
struct BatchScoreMetricSection {
	std::string name;
	std::string source;      // dot-path: "verification.overall_pass"
	double      weight = 0.0;
};

// [batch.score] — weighted composite scoring model
struct BatchScoreSection {
	std::string rank_by = "composite"; // "composite"|"energy"|"convergence"|"empirical_composite"
	std::vector<BatchScoreMetricSection> metrics;
};

// [batch.require] extension
struct BatchRequireSection {
	bool fail_if_missing              = true;
	std::vector<std::string> files;
	std::vector<std::string> checks;
};

// ============================================================================
// WO-VSIM-62C — Batch Layer Parser & Static Axis Runtime
//
// New language structs for [study], [batch.base], [batch.design],
// [[batch.axis]], [[batch.case]], [batch.expand], [seed],
// and [formation.library.*].
// Must be declared before BatchManifestSweepAxis.
// ============================================================================

// [study] — top-level study metadata
struct StudySection {
	std::string name;
	std::string type    = "parameter_sweep"; // parameter_sweep | empirical_validation | formation_study | sensitivity_analysis
	std::string goal;
	std::string version;
	bool        populated = false;
};

// [batch.base]
struct BatchBaseSection {
	std::string mode;    // "inline" | "template"
	std::string script;  // path when mode = "template"
	bool        populated = false;
};

// [batch.design]
struct BatchDesignSection {
	std::string type                = "factorial"; // factorial | single | grid | latin_hypercube (planned) | random | formation_ensemble (v5.2.0)
	int         replicates_per_case = 1;
	std::string seed_policy         = "split";  // "split" | "shift"
	bool        abort_on_fail       = false;
	std::string rank_by             = "composite";
	int         max_parallel        = 1;
	bool        checkpoint          = true;
	int         n_samples           = 0;         // latin_hypercube | random only
	bool        populated           = false;
};

// [[batch.axis]]
struct BatchAxisEntry {
	std::string name;
	std::string target;
	std::string kind          = "static"; // "static" | "stochastic" (v5.1.0) | "formation" (v5.2.0)
	std::vector<std::string> values;
	std::string units;

	// Stochastic fields — parsed now, wired in v5.1.0
	std::string seed_source;
	std::string distribution  = "uniform";
	double      mean          = 0.0;
	double      std_dev       = 1.0;
	double      dist_min      = 0.0;
	double      dist_max      = 1.0;
	int         n_samples     = 1;
};

// [[batch.case]]
struct BatchCaseEntry {
	std::string name;
	std::map<std::string, std::string> overrides; // dot-path → value string
};

// [batch.expand]
struct BatchExpandSection {
	bool                     cases = false;
	std::vector<std::string> axes;
	bool                     populated = false;
};

// [seed]
struct SeedSection {
	uint64_t foundation = 0;
	uint64_t defect     = 0;    // 0 = derive: foundation + 3000
	uint64_t formation  = 0;    // 0 = derive: foundation + 6000
	uint64_t thermal    = 0;    // 0 = derive: foundation + 8000
	uint64_t placement  = 0;    // 0 = derive: foundation + 11000
	bool     populated  = false;

	// Resolved values — filled by SeedResolver, not the parser
	uint64_t resolved_defect    = 0;
	uint64_t resolved_formation = 0;
	uint64_t resolved_thermal   = 0;
	uint64_t resolved_placement = 0;
};

// [formation.library.*] — formation stage kinds
enum class FormationStageKind {
	Hold, Ramp, Quench, Anneal, Relax, PressureRamp,
	FieldRamp,   // planned: not yet wired
	Cycle,       // planned: not yet wired
	Unknown
};

inline const char* to_string(FormationStageKind k) {
	switch (k) {
	case FormationStageKind::Hold:         return "hold";
	case FormationStageKind::Ramp:         return "ramp";
	case FormationStageKind::Quench:       return "quench";
	case FormationStageKind::Anneal:       return "anneal";
	case FormationStageKind::Relax:        return "relax";
	case FormationStageKind::PressureRamp: return "pressure_ramp";
	case FormationStageKind::FieldRamp:    return "field_ramp";
	case FormationStageKind::Cycle:        return "cycle";
	default:                               return "unknown";
	}
}

inline FormationStageKind formation_stage_kind_from_string(const std::string& s) {
	if (s == "hold")          return FormationStageKind::Hold;
	if (s == "ramp")          return FormationStageKind::Ramp;
	if (s == "quench")        return FormationStageKind::Quench;
	if (s == "anneal")        return FormationStageKind::Anneal;
	if (s == "relax")         return FormationStageKind::Relax;
	if (s == "pressure_ramp") return FormationStageKind::PressureRamp;
	if (s == "field_ramp")    return FormationStageKind::FieldRamp;
	if (s == "cycle")         return FormationStageKind::Cycle;
	return FormationStageKind::Unknown;
}

// [[formation.library.*.stage]]
struct FormationStage {
	FormationStageKind kind = FormationStageKind::Unknown;

	// hold / anneal / relax
	double temperature_K      = 293.15;
	double duration_ps        = 0.0;
	double pressure_GPa       = -1.0;   // -1 = inherit from library entry default

	// ramp / quench
	double from_temperature_K = 0.0;
	double to_temperature_K   = 0.0;
	std::string profile       = "linear";
	double from_pressure_GPa  = 0.0;
	double to_pressure_GPa    = 0.0;

	// relax
	int  max_steps = 2000;
	bool converge  = true;

	// field_ramp — planned, not yet wired
	double      from_field_V_A = 0.0;
	double      to_field_V_A   = 0.0;
	std::string field_axis;
};

// [formation.library.<name>]
struct FormationLibraryEntry {
	std::string name;
	std::string type        = "custom"; // quench | anneal | growth | custom
	std::string time_units  = "ps";
	double      pressure_GPa = 0.0;
	std::vector<FormationStage> stages;
};

struct BatchManifestSweepAxis {
	std::string              param;   // parameter name (informational)
	std::vector<std::string> values;  // string values for this axis
};


struct BatchManifestSection {
	std::string batch_id;             // e.g. "B9_REACTOR_CHANNEL_001"
	std::string description;
	std::string base_vsim;            // source .vsim path (informational)
	std::string score_by   = "composite"; // "energy" | "convergence" | "composite"
	std::string output_root = "runs"; // root directory for output tree
	int         seeds       = 1;      // seeds per parameter combination
	bool        abort_on_fail = false;
	bool        write_per_run_meta = true;  // write run_meta.json per run folder
	bool        write_per_run_metrics = true; // write metrics.tsv per run folder

	std::vector<BatchManifestSweepAxis> sweep; // parameter axes to expand

	// WO-VSEPR-SIM-62B sub-sections
	BatchVerifyPolicySection       verify_policy;
	BatchOverrideVerifySection     override_verify;
	BatchAggregateVerifySection    aggregate_verify;
	BatchScoreSection              score;
	BatchRequireSection            require;

	bool valid() const { return !batch_id.empty(); }

	// Total number of runs = product of axis lengths * seeds
	int total_runs() const {
		int n = seeds;
		for (const auto& ax : sweep)
			n *= static_cast<int>(ax.values.size());
		return (n > 0) ? n : seeds;
	}
};

// ── BatchRunRecord — result from a single expanded run ───────────────────────
// Accumulated by the manifest runner; used for summary and ranking tables.

struct BatchRunRecord {
	int         run_index    = 0;     // 1-based
	std::string run_id;               // e.g. "run_0001"
	std::string run_dir;              // full path to run folder

	// Sweep parameters active for this run (param → value string)
	std::map<std::string, std::string> params;
	int         seed         = 0;

	// Simulation outcome
	bool        converged    = false;
	double      final_energy = 0.0;   // kcal/mol
	double      rms_force    = 0.0;   // convergence residual
	int         steps_taken  = 0;
	double      wall_ms      = 0.0;   // wall time for this run (ms)

	// Scoring
	double      score_energy      = 0.0;
	double      score_convergence = 0.0;
	double      score_composite   = 0.0;

	// Gate results (WO-B9-002 fields — populated as false until gates are wired)
	bool        steady_pass          = false;
	std::string failure_reason;

	// Pipeline records (set after pipeline pass)
	std::string canonical_id;         // from FingerprintRecord / CanonicalIdentity
	std::string cluster_id;

	// Ranking
	int         rank = 0;             // 1 = best; 0 = unranked

	// WO-VSEPR-SIM-62B — per-run verification outcome
	// Populated by the batch aggregation kernel after consuming verify_report.json
	std::string verify_status;          // "PASS"|"WARN"|"FAIL"|"SKIP"|"MISSING"|"ERROR"
	std::vector<std::string> failure_modes; // BatchFailureMode code strings
	bool        empirical_ready = false;    // true when verify_status is PASS or WARN
};

// ── Convenience: batch run score helpers ─────────────────────────────────────
namespace batch_score {

inline double energy_score(double final_energy, double reference_energy = 0.0) {
	// Lower energy is better; normalise to [0,1] via soft-max at ±500 kcal/mol
	if (!std::isfinite(final_energy)) return 0.0;
	double delta = final_energy - reference_energy;
	return 1.0 / (1.0 + std::exp(delta / 100.0));
}

inline double convergence_score(int steps, int max_steps,
								double rms_force, double tol) {
	if (steps <= 0 || max_steps <= 0) return 0.0;
	if (rms_force > tol * 100.0) return 0.0;
	double budget = std::max(0.0, 1.0 - static_cast<double>(steps) / max_steps);
	double quality = (rms_force < tol)
		? 1.0 : std::max(0.0, 1.0 - std::log10(rms_force / tol) / 3.0);
	return 0.4 * budget + 0.6 * quality;
}

inline double composite(double es, double cs, double steady = 1.0) {
	return 0.4 * es + 0.4 * cs + 0.2 * (steady ? 1.0 : 0.0);
}

} // namespace batch_score


struct ExportSection {
	// ── Atomistic state ─────────────────────────────────────────────────────
	bool write_xyz                 = true;   // Static final particle positions
	bool write_xyzf                = false;  // Multi-frame trajectory (XYZF format)
	bool write_xyzfull             = false;  // Full state history (xyzFull doctrine)
	bool write_pdb                 = false;  // PDB format for external viewers (VESTA, VMD)

	// ── Analysis layer ───────────────────────────────────────────────────────
	bool write_analysis_json       = false;  // Derived metrics (AnalysisRecord)
	bool write_metrics_tsv         = false;  // Tab-separated per-run metric table
	bool write_cluster_json        = false;  // ClusterRecord assignments
	bool write_fingerprint_json    = false;  // FingerprintRecord feature vectors

	// ── Kernel event spine ──────────────────────────────────────────────────
	bool write_events_json         = false;  // KernelEventLog (JSON Lines)
	bool write_symbolic_trace_json = false;  // SymbolicTrace per-event

	// ── Reporting layer ─────────────────────────────────────────────────────
	bool write_report_md           = false;  // Human-readable Markdown summary
	bool write_summary_csv         = false;  // Per-run summary CSV
	bool write_dashboard_json      = false;  // DashboardRecord (beta-7 pipeline)
	bool write_manifest_json       = false;  // Run manifest with artifact registry
	bool write_dashboard_svg       = false;  // beta-7 pipeline dashboard (SVG — text, diffable)
	bool write_pipeline_audit_jsonl = false; // Stage-by-stage audit JSONL (2C gate)
	bool write_actual_hashes_tsv   = false;  // Golden suite: captured actual hashes TSV

	// ── Engineering geometry ────────────────────────────────────────────────
	bool write_step_file           = false;  // STEP geometry (engineering truth sidecar)
	bool write_vtp_mesh            = false;  // VTK PolyData mesh for ParaView

	std::string output_dir;                  // Output directory (default: out/<name>/)
};

// ============================================================================
// [export.visual] section — rendered visual artifact outputs
//
// These are files rendered FROM the simulation data.
// They are sidecar artifacts — not ground-truth state.
//
// output_format values per flag (used by renderer dispatch):
//   svg    — scalable vector, always available
//   png    — raster, requires stb_image_write or Cairo
//   html   — self-contained HTML with embedded JS charts
//   gif    — animated GIF of trajectory (requires gifenc or ffmpeg pipe)
//   webgl  — WebGL viewer bundle (from webgl_streamer)
//   sse    — SSE event stream descriptor (from vsepr_live)
// ============================================================================

struct ExportVisualSection {
	// ── Static figure exports ────────────────────────────────────────────────
	bool write_svg_figures         = false;  // Per-material SVG metric figures
	bool write_png_snapshots       = false;  // PNG molecular snapshot (requires GL or OSMesa)
	bool write_rdf_svg             = false;  // Radial distribution function plot (SVG)
	bool write_energy_trace_svg    = false;  // Energy-per-step trace figure (SVG)
	bool write_packing_heatmap_svg = false;  // 2D packing fraction heatmap (SVG)
	bool write_defect_map_svg      = false;  // Defect site map overlay (SVG)
	bool write_cluster_map_svg     = false;  // Cluster assignment scatter (SVG)

	// ── Animated exports ─────────────────────────────────────────────────────
	bool write_trajectory_gif      = false;  // Animated GIF of trajectory playback
	bool write_overlay_cycle_gif   = false;  // Animated GIF of overlay cycle (density→coord→...)
	int  gif_frame_skip            = 10;     // Emit every Nth frame into GIF
	int  gif_delay_cs              = 8;      // GIF frame delay (centiseconds)

	// ── Web / streaming exports ──────────────────────────────────────────────
	bool write_html_dashboard      = false;  // Self-contained HTML dashboard (JS charts)
	bool write_webgl_bundle        = false;  // WebGL viewer bundle (webgl_streamer path)
	bool write_sse_descriptor      = false;  // SSE stream config (vsepr_live path)
	int  sse_port                  = 99998;  // Port for SSE / HTTP server

	// ── Composite document ───────────────────────────────────────────────────
	bool write_report_pdf          = false;  // PDF report (requires LaTeX / pandoc)
	bool write_report_html         = false;  // HTML report (standalone, no server needed)

	std::string visual_output_dir; // Subdirectory for visual artifacts (default: figures/)

	bool any_active() const {
		return write_svg_figures || write_png_snapshots || write_rdf_svg
			|| write_energy_trace_svg || write_packing_heatmap_svg
			|| write_defect_map_svg   || write_cluster_map_svg
			|| write_trajectory_gif   || write_overlay_cycle_gif
			|| write_html_dashboard   || write_webgl_bundle
			|| write_sse_descriptor   || write_report_pdf
			|| write_report_html;
	}
};

// ============================================================================
// [visual] section — interactive display modes
//
// Declares what the runner should show DURING and AFTER the simulation.
// Separated from [export.visual] which controls rendered FILE artifacts.
//
// output_type catalog:
//
//   Terminal (always available, no GL dependency):
//     "none"                   — silent; no display
//     "terminal_chart"         — live per-step convergence trace + proxy table
//                                Source: cg_anim_demo (Pattern A) + metal_sim
//     "terminal_snapshot"      — post-run energy/eta bar-chart
//                                Source: seed_bead_demo / SnapshotGraphCollector (Pattern B)
//     "terminal_overlay_cycle" — full 6-panel kernel_viz_demo layout
//                                (timeline, bars, symbolic trace, pipeline trace,
//                                 animation cues, audit table)
//     "terminal_rdf"           — ASCII radial distribution function plot
//     "terminal_energy_heatmap"— 2D ASCII energy landscape heatmap
//     "terminal_defect_map"    — ASCII defect site map (grid projection)
//     "terminal_phase_diagram" — ASCII phase field snapshot
//
//   OpenGL (requires BUILD_VISUALIZATION):
//     "gl_overlay_cycle"       — CGVizViewer: density→coord→eta→orient auto-cycle
//                                Source: cg_anim_demo VizConfig command sequence
//     "gl_live_60fps"          — SeedBeadViewer 60fps FIRE live view
//                                Source: seed_bead_demo BUILD_VISUALIZATION path
//     "gl_crystal_grid"        — CrystalGrid viewer (crystal-viewer.cpp)
//     "gl_interactive"         — Full interactive-viewer with ImGui panels
//
//   Web / streaming (no GL; requires network):
//     "web_dashboard"          — HTTP server with auto-updating HTML dashboard
//                                Source: vsepr_live / live_server.hpp
//     "sse_stream"             — SSE event stream to external client
//     "webgl_viewer"           — WebGL streamer bundle (webgl_streamer.cpp)
//
// animation_mode values (terminal paths only):
//   "none"    — static table; print once after run
//   "spark"   — live per-step single-char spark-line during FIRE loop
//   "bar"     — bar chart redrawn at each convergence checkpoint
//   "overlay" — full overlay-cycle reprint at each steady-state event
// ============================================================================

struct VisualSection {
	// ── Primary output type ──────────────────────────────────────────────────
	std::string output_type    = "none";   // See catalog above

	// ── Animation mode (terminal paths) ─────────────────────────────────────
	std::string animation_mode = "none";   // none | spark | bar | overlay

	// ── Terminal display flags ───────────────────────────────────────────────
	// Shared across all terminal_* output types.
	bool show_proxy_table         = true;  // EnsembleProxy summary table (Pattern A)
	bool show_convergence_trace   = true;  // Live per-step trace row (metal_sim pattern)
	bool show_steady_state_marker = true;  // "✓ CONVERGED at step N" banner
	bool show_snapshot_chart      = false; // Post-run energy/eta bar-chart (Pattern B)
	bool show_event_timeline      = false; // ASCII kernel event timeline ruler
	bool show_bar_chart           = false; // Per-kind event count bar chart
	bool show_symbolic_trace      = false; // Symbolic equation trace per event
	bool show_animation_cues      = false; // Declarative animation cue table
	bool show_audit_table         = false; // Full event audit table
	bool show_rdf_plot            = false; // ASCII radial distribution function
	bool show_energy_heatmap      = false; // 2D ASCII energy landscape projection
	bool show_defect_map          = false; // ASCII defect site grid projection
	bool show_phase_field         = false; // ASCII phase field snapshot

	// ── Advanced-renderer overlays ────────────────────────────────────────────
	bool show_bond_events         = false; // Highlight newly formed/broken bonds per frame
	bool show_charge_overlay      = false; // Color atoms by dynamic partial charge
	bool show_velocity_vectors    = false; // Draw velocity vectors for fast particles

	// ── GL options (forwarded to CGVizViewer / SeedBeadViewer) ───────────────
	bool  gl_show_axes        = true;
	bool  gl_show_neighbours  = true;
	float gl_overlay_hold_s   = 2.5f;  // Seconds per overlay pane
	bool  gl_auto_orbit       = true;  // Orbit camera between overlays
	int   gl_window_width     = 1280;
	int   gl_window_height    = 800;

	// Overlay sequence: density, coordination, memory, orient_order
	std::vector<std::string> overlay_sequence = {
		"density", "coordination", "memory", "orient_order"
	};

	// ── Web / streaming options ──────────────────────────────────────────────
	int  web_port             = 99998; // HTTP / SSE server port
	bool web_auto_open        = false; // Open browser tab automatically

	// ── Render cadence ──────────────────────────────────────────────────────
	// How often simulation output / render events are emitted.
	// render_interval = N  →  emit a render frame every N simulation steps.
	// Orthogonal to display_fps (which controls live UI refresh rate).
	int render_interval = 1;   // steps; <= 0 is treated as 1

	bool should_render(int step) const {
		int ri = render_interval > 0 ? render_interval : 1;
		return (step % ri) == 0;
	}

	// ── Classifiers ─────────────────────────────────────────────────────────
	bool is_terminal_mode() const {
		return output_type == "terminal_chart"
			|| output_type == "terminal_snapshot"
			|| output_type == "terminal_overlay_cycle"
			|| output_type == "terminal_rdf"
			|| output_type == "terminal_energy_heatmap"
			|| output_type == "terminal_defect_map"
			|| output_type == "terminal_phase_diagram";
	}

	bool is_gl_mode() const {
		return output_type == "gl_overlay_cycle"
			|| output_type == "gl_live_60fps"
			|| output_type == "gl_crystal_grid"
			|| output_type == "gl_interactive";
	}

	bool is_web_mode() const {
		return output_type == "web_dashboard"
			|| output_type == "sse_stream"
			|| output_type == "webgl_viewer";
	}

	bool is_any_mode() const { return output_type != "none"; }
};

// ============================================================================
// [open.advanced] — advanced-viewer options for post-run launch
// ============================================================================

struct OpenAdvancedSection {
	bool data_inspector      = true;   // Open data/inspection panel
	bool trajectory_controls = true;   // Show frame scrubber + play controls
	bool event_overlay       = false;  // Overlay bond/charge events per frame
	bool bond_overlay        = false;  // Highlight dynamic bonds
	bool charge_overlay      = false;  // Color atoms by partial charge
	bool velocity_overlay    = false;  // Draw velocity vectors
	bool start_paused        = false;  // Start with playback paused
	int  default_frame       = 0;      // Frame index to display on open
};

// ============================================================================
// [open] — post-run viewer launch declaration
// ============================================================================

struct OpenSection {
	bool        enabled  = false;           // Whether to launch viewer after run
	std::string command  = "vsepr-sim open";// CLI command prefix
	std::string file;                       // Path to file to open (resolved at runtime)
	std::string mode     = "advanced";      // "advanced" | "tui" | "none"

	OpenAdvancedSection advanced;

	// Convenience: true when the effective mode requests the OpenGL renderer
	bool wants_advanced() const {
		return enabled && (mode == "advanced" || mode == "3d");
	}
};

// ============================================================================
// [visual.workspace] — workspace host settings (WO-VSIM-VIS-OVERHAUL-01)
// Ignored in headless / pure CLI runs. Parsed unconditionally so scripts are
// portable between workspace and non-workspace environments.
// ============================================================================

struct VisualWorkspaceSection {
	bool        enabled          = false;   // auto-open declared `show` directives on load
	std::string default_layout   = "tabs";  // "tabs" | "detached"
	bool        auto_open_tree   = false;   // expand object tree on load
	bool        live             = false;   // subscribe windows to live kernel updates (stretch)
};

// ============================================================================
// [room] — room heat-field simulation block (WO-VSIM-VIS-OVERHAUL-01)
// Drives the room.solver viewable node. Physics is handled by RoomSimulation
// (ported from pykernel/room_sim.py); this struct carries the parsed config.
// ============================================================================

struct RoomSection {
	std::string preset          = "";    // "ambient"|"cold"|"hot"|"lab"|"reactor"|"large_volume"
	int         n_steps         = 200;   // solver steps before snapshot
	int         record_interval = 50;    // steps between recorded snapshots

	bool is_active() const { return !preset.empty(); }
};

// ============================================================================
// ViewDirective — parsed from `show "<kind>" target = "<path>"` directives
// Stored on VsimDocument::view_directives (WO-VSIM-VIS-OVERHAUL-01)
// ============================================================================

struct ViewDirective {
	std::string kind;      // e.g. "calibration.helix", "data.events.table"
	std::string target;    // dot-path node: "run.history", "kernel.events", ...
	std::string options_raw; // raw JSON string from options = { ... } clause (may be empty)
};

// ============================================================================
// Golden test document types
// Populated by [defaults.*], [test.*], [suite], [report] sections
// ============================================================================

// Per-test run configuration (merges with GoldenDefaultsSection::run)
struct GoldenTestRunConfig {
	std::string mode             = "";      // build_relax_hash | build_hash | build_perturb_relax_hash | generate_hash
	int         temperature      = -1;      // -1 = use default
	int         relax_steps      = -1;      // -1 = use default
	std::string canonical_target = "";      // crystal_lattice | strained_crystal_lattice | topological_invariant | ...
	std::string spin_initialisation = "";
	double      cluster_cutoff_angstrom = 0.0;
	uint64_t    seed             = 0;
};

// Per-test analysis flags (merges with GoldenDefaultsSection::analysis)
struct GoldenTestAnalysis {
	bool coordination_number   = false;
	bool packing_fraction      = false;
	bool lattice_rmsd          = false;
	bool canonical_hash        = false;
	bool bond_angle            = false;
	bool dipole_moment         = false;
	bool strain_tensor_proxy   = false;
	bool pore_volume           = false;
	bool linker_angle_dist     = false;
	bool linking_number        = false;
	bool writhe                = false;
	bool pair_distribution     = false;
	bool fivefold_symmetry     = false;
	bool phason_strain         = false;
	bool tile_frequency_ratio  = false;
	bool local_isomorphism     = false;
	bool frustration_index     = false;
	bool spin_structure_factor = false;
	bool static_susceptibility = false;
};

// One entry in the golden test registry — one [test.<name>] block
struct GoldenTestEntry {
	std::string name;
	std::string group;          // molecule | ionic_crystal | metallic_crystal | ...
	std::string type;           // molecule | crystal | quasicrystal | surface | topology_object | magnetic_lattice
	std::string formula;
	std::string geometry;       // bent | tetrahedral | octahedral | ...
	std::string structure;      // fcc | bcc | hcp | rock_salt | diamond_cubic | ...
	std::string central_atom;
	std::string ligands;        // comma-separated
	int         lone_pairs      = 0;
	std::string basis;
	std::string supercell;      // e.g. "4x4x4"
	std::string topology;
	std::string symmetry;
	std::string periodicity;
	std::string projection_from;
	std::string tiling_type;
	std::string patch_size;
	std::string inflation_ratio;
	bool        long_range_order = false;
	bool        bulk             = true;
	std::string node_species;
	std::string linker;
	std::string linker_role;
	std::string coordination_hint;
	std::string vacancy_model;
	std::string magnetic_sites;
	std::string spin;
	std::string exchange_model;
	std::string frustration;
	std::string frustration_origin;
	std::string ground_state;
	double      strain_magnitude = 0.0;
	std::string strain_mode;
	std::string strain_axis;
	int         component_count  = 0;
	std::string component_type;
	std::string link_invariant;
	bool        pairwise_linked  = false;
	bool        globally_linked  = false;

	GoldenTestRunConfig  run;
	GoldenTestAnalysis   analysis;

	// Expected hash — empty means not yet captured (not PLACEHOLDER, just absent)
	std::string expected_hash;

	// Skip metadata — empty skip_reason means not skipped
	std::string skip_reason;
	std::string skip_target;
	bool        skip_blocks_release = false;

	bool is_skip() const { return !skip_reason.empty(); }
};

// [defaults.run] and [defaults.analysis] merged into every test at runtime
struct GoldenDefaultsSection {
	GoldenTestRunConfig  run;
	GoldenTestAnalysis   analysis;
};

// [suite] + [suite.limits] + [suite.smoke]
struct SuiteSection {
	bool                     enabled     = true;
	std::vector<std::string> groups;
	std::vector<std::string> run_modes;
	bool                     strict_order = true;
	// [suite.limits]
	int  max_runtime_per_test_seconds = 60;
	bool fail_fast                    = true;
	bool continue_on_warning          = false;
	// [suite.smoke]
	std::vector<std::string> smoke_groups;
	std::string              smoke_purpose;
};

// [report]
struct GoldenReportSection {
	std::string title                 = "golden_tests";
	bool include_test_manifest        = true;
	bool include_actual_hashes        = true;
	bool include_hash_mismatches      = true;
	bool include_placeholder_failures = false;
	bool include_smoke_tests          = true;
};

// ============================================================================
// [post_step] section — script block executed after each simulation step
//
// The script_block is a newline-separated sequence of VSIM interpreter
// expressions (assignments, pbc.* calls, particle.* calls).  It is
// executed by VsimRuntime::run_post_step() once particle positions have
// been updated for the current step.
//
// WO-VSEPR-SIM-57E
// ============================================================================

struct PostStepSection {
	bool        enabled      = false;  // True when a [post_step] block is present
	std::string script_block;          // Newline-separated VSIM expressions

	bool has_script() const { return enabled && !script_block.empty(); }
};

// ============================================================================
// WO-VSIM-03B — Intent-based structure authoring (Level 0–4)
// ============================================================================

// Structure alias map — resolves casual hints to deterministic prototype keys.
// "rocksalt" → "B1_NaCl", "diamond" → "A4_Si", etc.
// Only called during parsing; the resolved prototype is stored in MaterialSection.
[[nodiscard]] inline std::string_view resolve_structure_alias(std::string_view hint) noexcept {
    if (hint == "rocksalt"  || hint == "rock_salt")   return "B1_NaCl";
    if (hint == "zincblende"|| hint == "zinc_blende")  return "B3";
    if (hint == "diamond"   || hint == "diamond_cubic") return "A4_Si";
    if (hint == "wurtzite")                             return "B4";
    if (hint == "perovskite")                           return "E21";
    if (hint == "fluorite")                             return "C1";
    if (hint == "cubic")                                return "A1_cubic";
    if (hint == "bcc")                                  return "A2_bcc";
    if (hint == "fcc")                                  return "A1_fcc";
    if (hint == "hcp")                                  return "A3_hcp";
    return hint;   // unknown → pass through verbatim
}

// ── Level 0 / 1: [material] ──────────────────────────────────────────────────
//
// Declares the material identity and structural intent.
// Level 0:  formula + structure (alias resolved to prototype)
// Level 1:  prototype (deterministic key) | space_group + basis (explicit)
//
// Hierarchy (highest wins):
//   explicit basis  >  prototype  >  structure alias
//
struct MaterialSection {
    std::string formula;          // e.g. "NaCl", "Si", "Fe2O3"
    std::string prototype;        // Deterministic generator key: "B1_NaCl", "A4_Si"
    std::string structure;        // Casual hint, resolved via resolve_structure_alias()
    std::string space_group;      // e.g. "Fm-3m", "Fd-3m"
    std::string lattice;          // e.g. "fcc_ionic", "bcc", "hexagonal"
    std::string basis;            // e.g. "Na:0,0,0; Cl:0.5,0.5,0.5"
    std::string cell;             // Supercell spec: "4x4x4", "2x2x1", or scalar Å
    std::string phase;            // "solid", "liquid", "gas", "amorphous"

    bool has_formula()     const { return !formula.empty(); }
    bool has_prototype()   const { return !prototype.empty(); }
    bool has_basis()       const { return !basis.empty(); }
    bool has_space_group() const { return !space_group.empty(); }

    // Returns the resolved generator key: explicit basis > prototype > alias
    std::string resolved_prototype() const {
        if (!basis.empty() && !space_group.empty()) return "__explicit_basis__";
        if (!prototype.empty())                     return prototype;
        if (!structure.empty())
            return std::string(resolve_structure_alias(structure));
        return "";
    }
};

// ── Level 0: [run] ───────────────────────────────────────────────────────────
//
// Declares the run mode and top-level controls.
// mode values: "relax", "md", "npt", "nvt", "nve", "scan", "single_point"
//
struct RunSection {
    std::string mode;              // Required: "relax", "md", "npt", …
    int         max_steps  = 500;  // Step / iteration limit
    double      dt_fs      = 1.0;  // Timestep (fs) — ignored for "relax"
    double      temperature_K = 300.0;
    double      pressure_GPa  = 0.0;   // For NPT
    bool        converge   = true;     // Stop early on convergence
    std::string output_level = "standard"; // "minimal", "standard", "verbose"

    bool has_mode() const { return !mode.empty(); }
};

// ── Level 2: [environment] ───────────────────────────────────────────────────
struct EnvironmentSection {
    bool   periodic     = false;
    double temperature  = 300.0;   // K
    double pressure     = 0.0;     // GPa
    std::string medium;            // "vacuum", "water", "argon_gas", …
    double humidity     = 0.0;     // 0–1 fraction
    double field_x      = 0.0;    // External E-field components (V/Å)
    double field_y      = 0.0;
    double field_z      = 0.0;
};

// ── Level 2: [excite.*] ──────────────────────────────────────────────────────
// Each named excite subsection (e.g. [excite.laser]) becomes one ExciteEntry.
//
struct ExciteEntry {
    std::string type;              // "laser", "xray", "electron_beam", "thermal_spike"
    std::string axis;              // "x", "y", "z"
    std::string polarization;      // "x", "y", "z", "circular"
    double      intensity   = 1.0; // Arbitrary units (type-dependent)
    double      pulse_width_fs = 100.0;
    double      photon_energy_eV = 0.0;  // For xray / electron beam
    double      fluence     = 0.0;       // J/cm²
    std::string profile;           // "gaussian", "flat", "sech2"
};

struct ExciteSection {
    std::map<std::string, ExciteEntry> entries;  // keyed by subtype name

    bool has(const std::string& name) const { return entries.count(name) > 0; }
    const ExciteEntry* get(const std::string& name) const {
        auto it = entries.find(name);
        return it != entries.end() ? &it->second : nullptr;
    }
};

// ── Level 2: [observe] ───────────────────────────────────────────────────────
struct ObserveSection {
    std::vector<std::string> metrics;  // e.g. ["energy_map","interference","spectral_response"]
    std::string output_format = "auto"; // "csv", "json", "svg", "auto"
    int         every_n_steps = 1;     // Observation cadence
};

// ── Level 3: [[override.particle]] ──────────────────────────────────────────
// Array-of-tables: selectively mutate specific particles before or during run.
//
struct ParticleOverrideEntry {
    int    id          = -1;        // 1-indexed particle ID (-1 = unset)
    std::array<double,3> velocity   = {{0.0, 0.0, 0.0}};
    std::array<double,3> position   = {{0.0, 0.0, 0.0}};  // 0,0,0 = unset
    double charge      = 0.0;
    double mass_scale  = 1.0;       // Multiplicative mass override
    bool   fixed       = false;     // Freeze position
    bool   has_velocity = false;    // True when velocity was explicitly set
    bool   has_position = false;    // True when position was explicitly set
    bool   has_charge   = false;
};

// ── Level 4: [[raw.object]] ─────────────────────────────────────────────────
// Explicit particle injection — tests, importers, file bridges, debugging only.
//
struct RawObjectEntry {
    std::string id;                 // Arbitrary label: "debug_particle_001"
    std::string species;            // Element symbol or reserved label: "C", "alpha", "ghost"
    std::array<double,3> position   = {{0.0, 0.0, 0.0}};
    std::array<double,3> velocity   = {{0.0, 0.0, 0.0}};
    double charge      = 0.0;
    double mass        = 0.0;       // 0 = derive from species
    std::string label;              // Optional display label
};

// ============================================================================
// ChemistrySection — [chemistry] / [system]
// ============================================================================
//
// Declares the ambient reaction physics context for a simulation.
// Reactions are NOT an opt-in mode — they are always evaluated whenever
// reactant pairs exist.  This section controls which chemistry rules are
// active, the heat-gate level, scoring thresholds, and logging verbosity.
//
// [chemistry] is the canonical block.  [system] is the legacy surface that
// maps chemistry = "..." onto this same struct.
//
// chemistry registry values (§26 VSIM_LANGUAGE_REFERENCE):
//   "oxidation", "hydration", "corrosion", "pyrolysis", "reduction",
//   "hydrolysis", "polymerization", "salt_exchange", "fluorination",
//   "isomer_scan", "none"
//
struct ChemistrySection {
	// ── Registry alias ───────────────────────────────────────────────────
	std::string chemistry;          // Registry alias — resolves reaction rule set
									// Default "none" = reactions evaluated but no
									// rule family active; heat-gate still applies

	// ── Heat-gate ────────────────────────────────────────────────────────
	// Integer h ∈ [0, 999].  Maps directly to atomistic::reaction::HeatConfig.
	// 0   = cold / no thermal activation
	// 999 = extreme temperature activation
	// -1  = derive from environment.temperature (default, recommended)
	int  heat       = -1;           // -1 → derive from environment.temperature

	// ── Reaction event controls ──────────────────────────────────────────
	bool reaction_events      = true;  // Emit ReactionEvent into KernelEventLog
	bool track_species_state  = true;  // Emit ChemicalStateEvent per species change
	bool event_registry       = true;  // Build per-step event registry (enables
									   //   reaction_scan observe metric)

	// ── Scoring filter ───────────────────────────────────────────────────
	double min_score_threshold  = 0.25; // overall_score must exceed this to emit
	int    max_reactions_per_step = 8;  // Cap per simulation step (0 = unlimited)

	// ── Domain layer (organic formula parser integration) ─────────────────
	// When domain = "peptide" and sequence is non-empty, vsim_parser expands
	// the sequence into a canonical molecular formula and stores it in
	// material.formula.  Other domain values are reserved for future modules.
	//
	// Supported domain values:
	//   "peptide"      — amino acid one-letter sequence (e.g. "ACDEFG")
	//   "small_molecule" — trivial name or condensed formula
	//   ""             — not set; no organic expansion performed
	std::string domain;    // "peptide" | "small_molecule" | "" (not set)
	std::string sequence;  // for domain="peptide": one-letter AA sequence

	// Computed during parse: the canonical formula produced by the organic
	// formula parser.  Empty if domain/sequence were not set.
	std::string expanded_formula;

	// ── Helpers ──────────────────────────────────────────────────────────
	bool is_active()    const { return chemistry != "none"; }
	bool has_chemistry() const { return !chemistry.empty() && chemistry != "none"; }
	bool has_domain()    const { return !domain.empty(); }
	bool has_sequence()  const { return !sequence.empty(); }

	// Derive a 0–999 heat integer from a temperature in Kelvin.
	// Calibration: 300 K → h≈100; 1000 K → h≈333; 3000 K → h≈999
	static int heat_from_temperature(double T_K) {
		if (T_K <= 0.0)     return 0;
		if (T_K >= 3000.0)  return 999;
		return static_cast<int>((T_K / 3000.0) * 999.0);
	}

	// Return the effective heat integer: explicit heat or derived from T.
	int effective_heat(double env_temperature_K = 300.0) const {
		if (heat >= 0 && heat <= 999) return heat;
		return heat_from_temperature(env_temperature_K);
	}
};

// ============================================================================
// IsomerAnalysisSection — [analysis.isomers]
// ============================================================================
//
// Controls the static isomer analysis pipeline:
//   formula → graph → valence → connectivity → charge/radical gate →
//   canonical hash → geometry → relaxation → validity class → report
//
// WO-VSIM-04A
//
struct IsomerAnalysisSection {
	bool enabled = false;
	std::string mode = "graph_geometry";  // "graph_only" | "graph_geometry"

	// ── Validation gates ──────────────────────────────────────────────────
	bool formula_guard        = true;
	bool graph_validation     = true;
	bool valence_check        = true;
	bool connectivity_check   = true;
	bool formal_charge_check  = true;
	bool radical_check        = false;

	// ── Identity and geometry ─────────────────────────────────────────────
	bool canonical_hash       = true;
	bool geometry_rmsd        = true;
	bool relaxation_check     = true;
	bool known_database_check = false;

	// ── Permissive flags ──────────────────────────────────────────────────
	bool allow_fragments      = false;
	bool allow_charged        = false;
	bool allow_radicals       = false;
	bool allow_strained       = true;

	int  max_bond_order       = 3;

	// ── Tolerances ────────────────────────────────────────────────────────
	double bond_tolerance_scale = 1.20;  // scale × covalent radius sum
	double rmsd_tolerance       = 0.05;
	double angle_tolerance_deg  = 3.0;

	// ── Strategy ─────────────────────────────────────────────────────────
	std::string bond_source    = "hybrid";   // "explicit" | "infer" | "hybrid"
	std::string geometry_build = "vsepr";
	std::string report_level   = "detailed"; // "minimal" | "standard" | "detailed"
};

// ============================================================================
// IsomerGeneratorSection — [generator.isomers]
// ============================================================================
//
// Controls constitutional isomer generation from a molecular formula.
// WO-VSIM-04A
//
struct IsomerGeneratorSection {
	bool enabled = false;
	std::string formula;

	bool allow_fragments = false;
	bool allow_charged   = false;
	bool allow_radicals  = false;
	bool allow_strained  = true;

	int  max_bond_order  = 3;

	std::string deduplicate = "canonical_graph_hash";
};

// ============================================================================
// IsomerTrackingSection — [analysis.isomer_tracking]
// ============================================================================
//
// Per-frame isomer identity tracking over a trajectory (.xyzf / .xyzFull).
// Bond changes, hash changes, and validity transitions are logged as
// KernelEvents — never written back into the raw trajectory.
// WO-VSIM-04A
//
struct IsomerTrackingSection {
	bool enabled = false;
	std::string source;              // path to .xyzf / .xyzFull

	int  sample_every         = 10;
	bool detect_bond_changes  = true;
	bool detect_hash_changes  = true;
	bool write_transition_log = true;
};

// ============================================================================
// WO-VSIM-61C — analysis pipeline section structs
//
// These sections are parsed from a .vsim analysis script and consumed by
// the runtime pipeline orchestrator (src/runtime/vsim_analysis_pipeline.hpp).
// The kernels (structure_inference, property_sampling, property_inference)
// never see these structs — they receive only their typed *Config structs.
// ============================================================================

struct VsimSystemSection {
	int         schema_version = 1;         // 1 = WO-61C grammar; 2 = WO-61D grammar
	std::string name           = "unnamed";
	std::string source;
	std::string source_format  = "auto";  // "auto" | "xyz" | "xyzf" | "xyzFull" | "synthetic"
	int         seed           = 0;         // 0 = unseeded
};

struct VsimStructureAnalysisSection {
	bool   enabled              = false;
	bool   compute_coordination = true;
	bool   compute_packing      = true;
	bool   compute_displacement = true;
	bool   compute_anisotropy   = true;
	double neighbor_cutoff_A    = 3.0;
	double contact_cutoff_A     = 2.2;
};

struct VsimSamplingSection {
	bool enabled               = false;
	bool compute_rdf           = true;
	bool compute_msd           = true;
	int  min_frames_for_motion = 2;
	int  min_frames_for_msd    = 10;
	bool unwrap_pbc            = false;
	// NOTE: field_grid and min_particles_for_fields have moved to VsimScaleSamplingSection
};

// ============================================================================
// [analysis.scale_sampling] — WO-VSIM-61D
// ============================================================================
// Owns L1 → L2 field projection and RVE window sampling.
// Strictly separated from [analysis.sampling] (scalar RDF/MSD only).
// ============================================================================

struct VsimScaleSamplingSection {
	bool enabled                          = false;

	// Sub-operations
	bool compute_field_projection         = true;
	bool compute_rve_sampling             = false;
	bool compute_emergence_metrics        = false;

	// Field projection
	std::array<int, 3> field_grid         = {8, 8, 8};
	int  min_particles_for_scale_sampling = 64;

	// RVE window settings
	std::vector<double> rve_window_lengths_A;          // strictly increasing, all > 0
	int                 rve_windows_per_level  = 16;
	std::string         rve_window_placement   = "stratified"; // "grid"|"random"|"stratified"

	// Drift metric selectors
	std::string temporal_drift_metric  = "block_difference";          // known enum
	std::string scale_drift_metric     = "successive_window_difference"; // known enum

	// Global thresholds (per-metric deferred)
	double spatial_cv_threshold      = 0.20;
	double temporal_drift_threshold  = 0.10;
	double scale_drift_threshold     = 0.15;
};

// WO-61D: was VsimInferenceSection; now VsimAnalysisInferenceSection under [analysis.inference].
// Old [inference] (schema_version=1) is accepted as a deprecated alias.
struct VsimAnalysisInferenceSection {
	bool        enabled                               = false;
	std::string mode                                  = "rule_based_61b"; // also accepts "rule_based_61d"
	bool        infer_packing_regime                  = true;
	bool        infer_mobility_regime                 = true;
	bool        infer_anisotropy_regime               = true;
	bool        infer_structural_stability            = true;
	bool        infer_macro_readiness                 = true;
	int         min_particles_macro_candidate         = 128;
	int         min_frames_macro_candidate            = 20;
	int         minimum_available_metrics_for_macro_ready = 4;
};

// Backward-compat alias so existing WO-61C code still compiles unchanged.
using VsimInferenceSection = VsimAnalysisInferenceSection;

struct VsimOutputSection {
	std::string output_dir    = ".";   // directory for all output files
	std::string output_prefix = "sample"; // filename prefix: {dir}/{prefix}.{type}.json

	bool write_structure_json       = false;
	bool write_sampling_json        = false;
	bool write_scale_sampling_json  = false; // WO-61D
	bool write_inference_json       = false;
	bool write_sampling_manifest    = false;
};

// ============================================================================
// WO-62A — Empirical Verification sections
// ============================================================================

struct VsimVerifyStructureSection {
	bool   enabled                    = false;
	std::string expected_prototype    = "";      // e.g. "B1_NaCl"
	int    expected_coordination      = -1;      // -1 = not checked
	int    coordination_tolerance     = 0;
	double expected_nearest_neighbor_A      = -1.0;  // -1 = not checked
	double nearest_neighbor_tolerance_A     = 0.15;
	std::string expected_density_relation   = "";    // informational label
};

struct VsimVerifyRdfSection {
	bool   enabled                        = false;
	double expected_first_peak_A          = -1.0;
	double first_peak_tolerance_A         = 0.20;
	double expected_second_peak_A         = -1.0;
	double second_peak_tolerance_A        = 0.25;
	std::vector<double> expected_peaks_A; // alternative: ordered list
	double peak_tolerance_A               = 0.25;  // used with expected_peaks_A
	bool   require_peak_order             = true;
};

struct VsimVerifyMsdSection {
	bool   enabled               = false;
	bool   expect_bounded_solid  = false;
	double max_msd_A2            = 5.0;
	double max_slope_late        = 0.01;   // diffusion_proxy_A2_per_frame limit
	std::string expect_regime    = "";     // informational: "solid_bounded"
};

struct VsimVerifyMassSection {
	bool   enabled               = false;
	double relative_tolerance    = 1e-10;
};

struct VsimVerifySection {
	bool   enabled               = false;
	std::string profile          = "";   // informational label, e.g. "nacl_rocksalt_short_md"
	bool   write_verify_report   = true;
	bool   write_verify_tsv      = true;

	VsimVerifyStructureSection structure;
	VsimVerifyRdfSection       rdf;
	VsimVerifyMsdSection       msd;
	VsimVerifyMassSection      mass;
};

// Aggregate parsed from an analysis-oriented .vsim script.
// Consumed by run_vsim_analysis_pipeline(); kernels never see this type.
struct VsimAnalysisPipelineConfig {
	VsimSystemSection              system;
	VsimStructureAnalysisSection   structure;
	VsimSamplingSection            sampling;
	VsimScaleSamplingSection       scale_sampling; // WO-61D
	VsimAnalysisInferenceSection   inference;
	VsimOutputSection              output;
	VsimVerifySection              verify;         // WO-62A
};

// ============================================================================
// VsimDocument — full parsed .vsim file
// ============================================================================

struct VsimDocument {
	std::string         source_path;
	ProjectSection      project;
	SimulationSection   simulation;
	CellSection         cell;         // [cell]     — WO-57B
	BoundarySection     boundary;     // [boundary] — WO-57B
	PBCSection          pbc;          // [pbc]      — WO-57B

	// WO-VSIM-03B — intent-based authoring
	MaterialSection     material;                       // [material]
	RunSection          run;                            // [run]
	EnvironmentSection  environment;                    // [environment]
	ChemistrySection    chemistry;                      // [chemistry] / [system]
	ExciteSection       excite;                         // [excite.*]
	ObserveSection      observe;                        // [observe]
	std::vector<ParticleOverrideEntry> overrides;       // [[override.particle]]
	std::vector<RawObjectEntry>        raw_objects;     // [[raw.object]]

	// WO-VSIM-04A — isomer analysis pipeline
	IsomerAnalysisSection   isomer_analysis;    // [analysis.isomers]
	IsomerGeneratorSection  isomer_generator;   // [generator.isomers]
	IsomerTrackingSection   isomer_tracking;    // [analysis.isomer_tracking]
	ExportSection       exports;
	ExportVisualSection export_visual;
	VisualSection       visual;
	VisualExternalSection visual_external;
	VisualWorkspaceSection visual_workspace;        // [visual.workspace]
	RoomSection            room;                    // [room]
	std::vector<ViewDirective> view_directives;     // `show` top-level directives
	OpenSection         open;                 // [open] / [open.advanced]
	VarianceSection     variance_cfg;
	NEvolutionSection   n_evolution_cfg;
	WhileSection        while_cfg;
	BatchSection        batch_cfg;
	PostStepSection     post_step;    // [post_step] — WO-57E

	// WO-VSIM-61C/61D — analysis pipeline sections
	VsimSystemSection              pipeline_system;         // [system]
	VsimStructureAnalysisSection   pipeline_structure;      // [analysis.structure]
	VsimSamplingSection            pipeline_sampling;       // [analysis.sampling]
	VsimScaleSamplingSection       pipeline_scale_sampling; // [analysis.scale_sampling] WO-61D
	VsimAnalysisInferenceSection   pipeline_inference;      // [analysis.inference] (v2) / [inference] (v1)
	VsimOutputSection              pipeline_output;         // [output]
	VsimVerifySection              pipeline_verify;         // [verify] WO-62A

	// Golden test suite — populated by [defaults.*], [test.*], [suite], [report]
	GoldenDefaultsSection               golden_defaults;
	std::vector<GoldenTestEntry>        golden_tests;
	SuiteSection                        suite;
	GoldenReportSection                 golden_report;

	// Raw key-value store for unknown/extension sections (forward-compatible)
	std::map<std::string, std::map<std::string, Value>> raw_sections;

	// Validation state (populated by validate())
	struct ValidationResult {
		bool        ok        = true;
		std::vector<std::string> errors;
		std::vector<std::string> warnings;

		void error(const std::string& msg)   { ok = false; errors.push_back(msg); }
		void warn(const std::string& msg)    { warnings.push_back(msg); }
	};

	// -----------------------------------------------------------------------
	// Validate the document after parsing
	// -----------------------------------------------------------------------
	ValidationResult validate() const {
		ValidationResult r;

		if (project.name.empty())
			r.error("[project] name is required");

		if (simulation.molecules.empty())
			r.warn("[simulation] no molecules specified — empty run");

		for (const auto& mol : simulation.molecules) {
			if (mol.formula.empty())
				r.error("[simulation] molecule entry has empty formula");
			if (mol.count < 1)
				r.error("[simulation] molecule '" + mol.formula + "' count < 1");
			if (mol.temperature_K < 0.0)
				r.error("[simulation] molecule '" + mol.formula + "' temperature < 0 K");
		}

		if (simulation.fire_max_steps < 1)
			r.error("[simulation] fire_max_steps must be >= 1");

		if (simulation.box_size_ang < 0.0)
			r.error("[simulation] box_size_ang must be >= 0 (0 = auto)");

		// WO-VSIM-03B validations
		if (material.has_formula() && simulation.molecules.empty()) {
			// [material] used without [simulation] — that is fine, no error
		}
		if (run.has_mode()) {
			const std::string& m = run.mode;
			if (m != "relax" && m != "md" && m != "npt" && m != "nvt" &&
				m != "nve"   && m != "scan" && m != "single_point")
				r.warn("[run] mode '" + m + "' is unrecognized — will pass through to runtime");
			if (run.max_steps < 1)
				r.error("[run] max_steps must be >= 1");
		}
		for (const auto& ov : overrides) {
			if (ov.id < 1)
				r.error("[[override.particle]] id must be >= 1");
		}

		if (!exports.output_dir.empty()) {
			// output_dir is informational — no filesystem check at parse time
		}

		return r;
	}

	// -----------------------------------------------------------------------
	// Human-readable summary (for validate command output)
	// -----------------------------------------------------------------------
	std::string summary() const {
		std::string s;
		s += "vsim document: " + source_path + "\n";
		s += "  [project]\n";
		s += "    name        = " + project.name + "\n";
		s += "    version     = " + project.version + "\n";
		s += "    seed_base   = " + std::to_string(project.seed_base) + "\n";
		s += "    determinism = " + std::string(project.determinism ? "true" : "false") + "\n";
		if (!project.description.empty())
			s += "    description = " + project.description + "\n";
		s += "  [simulation]\n";
		s += "    molecules   = " + std::to_string(simulation.molecules.size()) + " species\n";
		for (const auto& m : simulation.molecules) {
			s += "      " + m.formula + "  x" + std::to_string(m.count);
			s += "  T=" + std::to_string((int)m.temperature_K) + " K";
			if (!m.lattice.empty())    s += "  lattice=" + m.lattice;
			if (!m.layer_mode.empty()) s += "  stack=" + m.layer_mode;
			if (m.n_layers > 1)        s += "  layers=" + std::to_string(m.n_layers);
			s += "\n";
		}
		s += "    fire_max_steps = " + std::to_string(simulation.fire_max_steps) + "\n";
		s += "    box_size_ang   = " + std::to_string((int)simulation.box_size_ang) + "\n";
		s += "    periodic       = " + std::string(simulation.periodic ? "true" : "false") + "\n";
		auto flag = [](bool b) -> const char* { return b ? "yes" : "no"; };
		s += "  [export]\n";
		s += "    xyz                    = " + std::string(flag(exports.write_xyz))                 + "\n";
		s += "    xyzf                   = " + std::string(flag(exports.write_xyzf))                + "\n";
		s += "    xyzfull                = " + std::string(flag(exports.write_xyzfull))              + "\n";
		s += "    pdb                    = " + std::string(flag(exports.write_pdb))                  + "\n";
		s += "    analysis_json          = " + std::string(flag(exports.write_analysis_json))        + "\n";
		s += "    metrics_tsv            = " + std::string(flag(exports.write_metrics_tsv))          + "\n";
		s += "    cluster_json           = " + std::string(flag(exports.write_cluster_json))         + "\n";
		s += "    fingerprint_json       = " + std::string(flag(exports.write_fingerprint_json))     + "\n";
		s += "    events_json            = " + std::string(flag(exports.write_events_json))          + "\n";
		s += "    symbolic_trace_json    = " + std::string(flag(exports.write_symbolic_trace_json))  + "\n";
		s += "    report_md              = " + std::string(flag(exports.write_report_md))            + "\n";
		s += "    summary_csv            = " + std::string(flag(exports.write_summary_csv))          + "\n";
		s += "    dashboard_json         = " + std::string(flag(exports.write_dashboard_json))       + "\n";
		s += "    manifest_json          = " + std::string(flag(exports.write_manifest_json))        + "\n";
		s += "    step_file              = " + std::string(flag(exports.write_step_file))            + "\n";
		s += "    vtp_mesh               = " + std::string(flag(exports.write_vtp_mesh))             + "\n";
		s += "    actual_hashes_tsv      = " + std::string(flag(exports.write_actual_hashes_tsv))    + "\n";
		if (!exports.output_dir.empty())
			s += "    output_dir             = " + exports.output_dir + "\n";
		if (export_visual.any_active()) {
			s += "  [export.visual]\n";
			s += "    svg_figures            = " + std::string(flag(export_visual.write_svg_figures))          + "\n";
			s += "    png_snapshots          = " + std::string(flag(export_visual.write_png_snapshots))        + "\n";
			s += "    rdf_svg                = " + std::string(flag(export_visual.write_rdf_svg))              + "\n";
			s += "    energy_trace_svg       = " + std::string(flag(export_visual.write_energy_trace_svg))     + "\n";
			s += "    packing_heatmap_svg    = " + std::string(flag(export_visual.write_packing_heatmap_svg))  + "\n";
			s += "    defect_map_svg         = " + std::string(flag(export_visual.write_defect_map_svg))       + "\n";
			s += "    cluster_map_svg        = " + std::string(flag(export_visual.write_cluster_map_svg))      + "\n";
			s += "    trajectory_gif         = " + std::string(flag(export_visual.write_trajectory_gif))       + "\n";
			s += "    overlay_cycle_gif      = " + std::string(flag(export_visual.write_overlay_cycle_gif))    + "\n";
			s += "    html_dashboard         = " + std::string(flag(export_visual.write_html_dashboard))       + "\n";
			s += "    webgl_bundle           = " + std::string(flag(export_visual.write_webgl_bundle))         + "\n";
			s += "    report_pdf             = " + std::string(flag(export_visual.write_report_pdf))           + "\n";
			s += "    report_html            = " + std::string(flag(export_visual.write_report_html))          + "\n";
			if (!export_visual.visual_output_dir.empty())
				s += "    visual_output_dir      = " + export_visual.visual_output_dir + "\n";
		}
		if (visual.is_any_mode()) {
			s += "  [visual]\n";
			s += "    output_type            = " + visual.output_type    + "\n";
			s += "    animation_mode         = " + visual.animation_mode + "\n";
			s += "    show_proxy_table       = " + std::string(flag(visual.show_proxy_table))          + "\n";
			s += "    show_convergence_trace = " + std::string(flag(visual.show_convergence_trace))    + "\n";
			s += "    show_steady_state      = " + std::string(flag(visual.show_steady_state_marker))  + "\n";
			s += "    show_event_timeline    = " + std::string(flag(visual.show_event_timeline))       + "\n";
			s += "    show_bar_chart         = " + std::string(flag(visual.show_bar_chart))            + "\n";
			s += "    show_symbolic_trace    = " + std::string(flag(visual.show_symbolic_trace))       + "\n";
			s += "    show_snapshot_chart    = " + std::string(flag(visual.show_snapshot_chart))       + "\n";
			if (visual.is_gl_mode()) {
				s += "    gl_window              = " + std::to_string(visual.gl_window_width) + "x"
					+ std::to_string(visual.gl_window_height) + "\n";
				s += "    gl_overlay_hold_s      = " + std::to_string(visual.gl_overlay_hold_s) + "\n";
			}
			if (visual.is_web_mode())
				s += "    web_port               = " + std::to_string(visual.web_port) + "\n";
		}
		if (simulation.step_delay_ms > 0)
			s += "    step_delay_ms  = " + std::to_string(simulation.step_delay_ms) + "\n";
		if (simulation.smooth_resim)
			s += "    smooth_resim   = true\n";
		if (visual_external.any_active()) {
			s += "  [visual.external]\n";
			s += "    enabled        = " + std::string(flag(visual_external.enabled)) + "\n";
			for (const auto& t : visual_external.render_targets)
				s += "    render         = " + t + "\n";
			s += "    format         = " + visual_external.export_format + "\n";
		}
		if (!variance_cfg.probes.empty()) {
			s += "  [variance]\n";
			for (const auto& p : variance_cfg.probes)
				s += "    " + p.name + "  field=" + p.field + "  window=" + p.window + "\n";
		}
		if (!n_evolution_cfg.probes.empty()) {
			s += "  [N_evolution]\n";
			for (const auto& p : n_evolution_cfg.probes)
				s += "    " + p.name + "  target=" + p.target + "  window=" + p.window + "\n";
		}
		if (!while_cfg.guards.empty()) {
			s += "  [while]\n";
			for (const auto& g : while_cfg.guards)
				s += "    " + g.name + "  cond=\"" + g.condition
					+ "\"  steps=" + std::to_string(g.body_steps)
					+ "  max=" + std::to_string(g.max_iters) + "\n";
		}
		if (!batch_cfg.jobs.empty()) {
			s += "  [batch]\n";
			for (const auto& j : batch_cfg.jobs)
				s += "    job=" + j.name + "  seeds=" + std::to_string(j.seed_count) + "\n";
		}
		return s;
	}
};

} // namespace vsim
