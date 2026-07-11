#pragma once
/**
 * x_bundle.hpp
 * ============
 * WO-66M  |  .x Bundle Manifest Descriptor
 *
 * The `.x` format is a bundled suite-execution container.
 *
 * Role separation (bridge paper §18 and .dynx format rules):
 *
 *   Format         Role
 *   ------         ----
 *   .vsim          Source script (human-authored)
 *   .xyz / .xyzFull  Scientific state / replay truth
 *   .dynx          Live visual / session archive (post-compiled)
 *   .x             Bundled suite execution container (this file)
 *
 * An `.x` bundle is an ordered list of execution slots, each referencing:
 *   - A `.vsim` script (or inline script text for small tests)
 *   - Override parameters for that slot
 *   - Expected outputs and validation constraints
 *   - Dependency ordering (slot B may require slot A's outputs)
 *
 * A bundle is also a reproducibility artifact: it carries the run seed,
 * version tag, and hash of each included script so that replaying the
 * bundle produces identical outputs given the same binary.
 *
 * Typical use cases:
 *   - Annihilation test ladders (ANN-01 through ANN-10)
 *   - Formation sweeps with multiple seed values
 *   - Benchmark suites run by CI
 *   - Multi-step validation chains
 *
 * This header defines:
 *   XBundleSlot        — one execution unit inside the bundle
 *   XBundleManifest    — the full bundle descriptor
 *   XBundleStatus      — per-slot execution result
 *   XBundleSummary     — aggregate run summary
 *
 * The runtime that executes `.x` files is defined separately; this header
 * is the data model only.
 *
 * v5.1.4  |  WO-66M  |  v5.0.0-main
 */

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace vsim::bundle {

// ============================================================================
// XBundleSlotKind — what kind of execution unit is this slot?
// ============================================================================

enum class XBundleSlotKind : uint8_t {
	Script       = 0,   // reference to an external .vsim file
	InlineScript = 1,   // script text embedded inline (small tests)
	PyKernel     = 2,   // Python kernel script (.py in pykernel/)
	ValidationCheck = 3, // post-run assertion against an output file
	Benchmark    = 4,   // timed benchmark slot (reports wall time + metrics)
};

inline const char* x_bundle_slot_kind_name(XBundleSlotKind k) noexcept {
	switch (k) {
		case XBundleSlotKind::Script:          return "script";
		case XBundleSlotKind::InlineScript:    return "inline_script";
		case XBundleSlotKind::PyKernel:        return "py_kernel";
		case XBundleSlotKind::ValidationCheck: return "validation_check";
		case XBundleSlotKind::Benchmark:       return "benchmark";
	}
	return "unknown";
}

// ============================================================================
// XBundleOverride — one key=value override applied to a slot's script
// ============================================================================

struct XBundleOverride {
	std::string section;  // e.g. "annihilation", "run", "system"
	std::string key;      // e.g. "radius", "steps", "seed"
	std::string value;    // string form; parser coerces to target type
};

// ============================================================================
// XBundleOutput — expected output file from a slot
// ============================================================================

struct XBundleOutput {
	std::string path;                  // relative output path
	bool        required   {true};     // fail the slot if missing?
	std::string hash_sha1;             // expected SHA-1 (empty = no check)
	std::string validator;             // validator id (empty = no check)
};

// ============================================================================
// XBundleSlot — one execution unit
// ============================================================================

struct XBundleSlot {
	// ---- identity ----------------------------------------------------------
	std::string        id;             // unique slot id within the bundle
	std::string        label;          // human label, e.g. "ANN-01 identity gate"
	XBundleSlotKind    kind    {XBundleSlotKind::Script};

	// ---- source ------------------------------------------------------------
	std::string        script_path;    // for kind=Script / PyKernel
	std::string        inline_text;    // for kind=InlineScript

	// ---- overrides ---------------------------------------------------------
	std::vector<XBundleOverride> overrides;

	// ---- dependency ordering -----------------------------------------------
	// Slot ids that must complete (and pass) before this slot runs.
	std::vector<std::string> depends_on;

	// ---- expected outputs --------------------------------------------------
	std::vector<XBundleOutput> expected_outputs;

	// ---- constraints -------------------------------------------------------
	double max_wall_s     {0.0};   // 0 = no wall limit
	bool   allow_failure  {false}; // if true, slot failure does not abort bundle

	// ---- reproducibility ---------------------------------------------------
	// SHA-1 of the script file at bundle-creation time.
	// Runner verifies before execution when non-empty.
	std::string script_hash_sha1;
};

// ============================================================================
// XBundleManifest — the complete .x bundle descriptor
// ============================================================================

struct XBundleManifest {
	// ---- bundle identity ---------------------------------------------------
	std::string  bundle_id;        // script-defined; set by factory or caller
	std::string  version;          // e.g. "5.0.14"
	std::string  purpose;          // short human description
	std::string  author;           // optional
	std::string  created_utc;      // ISO-8601

	// ---- reproducibility ---------------------------------------------------
	uint64_t     run_seed     {0};   // top-level seed (propagated to all slots)
	std::string  vsim_version;       // e.g. "v5.0.14"
	std::string  git_ref;            // commit / tag at bundle creation

	// ---- execution slots ---------------------------------------------------
	std::vector<XBundleSlot> slots;

	// ---- global outputs ----------------------------------------------------
	std::string  summary_md_path;    // aggregate Markdown summary
	std::string  events_jsonl_path;  // merged event stream
	std::string  bench_tsv_path;     // benchmark timing TSV

	// ---- global constraints ------------------------------------------------
	bool         abort_on_first_failure {false};
	double       global_wall_limit_s    {0.0};    // 0 = no limit

	// ---- helpers -----------------------------------------------------------
	std::size_t slot_count() const noexcept { return slots.size(); }

	const XBundleSlot* find_slot(const std::string& id) const noexcept {
		for (const auto& s : slots) {
			if (s.id == id) return &s;
		}
		return nullptr;
	}
};

// ============================================================================
// XBundleSlotStatus — result of executing one slot
// ============================================================================

enum class XBundleSlotOutcome : uint8_t {
	Pending   = 0,
	Running   = 1,
	Passed    = 2,
	Failed    = 3,
	Skipped   = 4,   // dependency failed; slot skipped
};

inline const char* x_bundle_slot_outcome_name(XBundleSlotOutcome o) noexcept {
	switch (o) {
		case XBundleSlotOutcome::Pending:  return "pending";
		case XBundleSlotOutcome::Running:  return "running";
		case XBundleSlotOutcome::Passed:   return "passed";
		case XBundleSlotOutcome::Failed:   return "failed";
		case XBundleSlotOutcome::Skipped:  return "skipped";
	}
	return "unknown";
}

struct XBundleSlotStatus {
	std::string         slot_id;
	XBundleSlotOutcome  outcome   {XBundleSlotOutcome::Pending};
	double              wall_s    {0.0};    // actual wall time
	std::string         error_msg;          // empty on pass
	std::vector<std::string> missing_outputs;
};

// ============================================================================
// XBundleSummary — aggregate result after all slots complete
// ============================================================================

struct XBundleSummary {
	std::string bundle_id;
	std::size_t total   {0};
	std::size_t passed  {0};
	std::size_t failed  {0};
	std::size_t skipped {0};
	double      total_wall_s {0.0};

	std::vector<XBundleSlotStatus> slot_statuses;

	bool all_passed() const noexcept { return failed == 0 && skipped == 0; }

	// One-line summary for CI / post-install output
	std::string one_line() const {
		char buf[256];
		std::snprintf(buf, sizeof(buf),
			"[BUNDLE] %s  total=%zu pass=%zu fail=%zu skip=%zu  %.2fs  %s",
			bundle_id.c_str(), total, passed, failed, skipped, total_wall_s,
			all_passed() ? "PASSED" : "FAILED");
		return std::string(buf);
	}
};

// ============================================================================
// Factory helpers
// ============================================================================

// Build a generic Default Usage Bundle (DUB) for any single .vsim script.
//
// Slots produced:
//   LOAD-01      load / locate the source script
//   RUN-01       execute the simulation
//   VALIDATE-01  validate required outputs exist
//   REPORT-01    generate summary / report artifacts
//   BENCH-01     optional benchmark / timing record (allow_failure = true)
//
// All paths are relative to the working directory at execution time.
// The caller sets bundle_id to a meaningful name (e.g. the script stem).
inline XBundleManifest make_default_usage_bundle(
	const std::string& script_path,
	const std::string& bundle_id,
	uint64_t           seed        = 0,
	const std::string& vsim_ver    = "v5.1.4",
	const std::string& created_utc = ""
) {
	XBundleManifest m;
	m.bundle_id         = bundle_id;
	m.version           = "5.0.14";
	m.purpose           = "Default single-script execution bundle";
	m.run_seed          = seed;
	m.vsim_version      = vsim_ver;
	m.created_utc       = created_utc;
	m.summary_md_path   = bundle_id + "_summary.md";
	m.events_jsonl_path = bundle_id + "_events.jsonl";
	m.bench_tsv_path    = bundle_id + "_bench.tsv";

	// LOAD-01 — locate/load the source script
	{
		XBundleSlot s;
		s.id          = "LOAD-01";
		s.label       = "Load source script";
		s.kind        = XBundleSlotKind::Script;
		s.script_path = script_path;
		m.slots.push_back(std::move(s));
	}

	// RUN-01 — execute the simulation
	{
		XBundleSlot s;
		s.id         = "RUN-01";
		s.label      = "Execute simulation";
		s.kind       = XBundleSlotKind::Script;
		s.script_path = script_path;
		s.depends_on = {"LOAD-01"};
		m.slots.push_back(std::move(s));
	}

	// VALIDATE-01 — validate required outputs
	{
		XBundleSlot s;
		s.id         = "VALIDATE-01";
		s.label      = "Validate required outputs";
		s.kind       = XBundleSlotKind::ValidationCheck;
		s.depends_on = {"RUN-01"};
		m.slots.push_back(std::move(s));
	}

	// REPORT-01 — generate summary / report artifacts
	{
		XBundleSlot s;
		s.id         = "REPORT-01";
		s.label      = "Generate summary and report artifacts";
		s.kind       = XBundleSlotKind::Script;
		s.depends_on = {"VALIDATE-01"};
		m.slots.push_back(std::move(s));
	}

	// BENCH-01 — optional benchmark/timing (non-fatal)
	{
		XBundleSlot s;
		s.id           = "BENCH-01";
		s.label        = "Benchmark and timing record";
		s.kind         = XBundleSlotKind::Benchmark;
		s.depends_on   = {"RUN-01"};
		s.allow_failure = true;
		m.slots.push_back(std::move(s));
	}

	return m;
}

// Build the ANN-01..10 standard test ladder bundle
inline XBundleManifest make_annihilation_test_bundle(
	uint64_t seed = 6701,
	const std::string& script_dir = "scripts/annihilation/"
) {
	XBundleManifest m;
	m.bundle_id    = "annihilation_test_ladder";
	m.version      = "5.0.14";
	m.purpose      = "Controlled annihilation verification — ANN-01 through ANN-10";
	m.run_seed     = seed;
	m.vsim_version = "v5.0.14";
	m.summary_md_path   = "annihilation_summary.md";
	m.events_jsonl_path = "annihilation_events.jsonl";
	m.bench_tsv_path    = "annihilation_bench.tsv";

	// Helper lambda to push a standard script slot
	auto push = [&](const std::string& id,
					const std::string& label,
					const std::string& script,
					std::vector<std::string> deps = {}) {
		XBundleSlot s;
		s.id          = id;
		s.label       = label;
		s.kind        = XBundleSlotKind::Script;
		s.script_path = script_dir + script;
		s.depends_on  = std::move(deps);
		m.slots.push_back(std::move(s));
	};

	push("ANN-01", "Identity gate — only valid anti-pairs annihilate",
		 "ann_01_identity_gate.vsim");
	push("ANN-02", "Distance gate — r_ij < r_c",
		 "ann_02_distance_gate.vsim", {"ANN-01"});
	push("ANN-03", "Energy gate — E_rel > E_c",
		 "ann_03_energy_gate.vsim", {"ANN-01"});
	push("ANN-04", "Product record creation",
		 "ann_04_products.vsim", {"ANN-02", "ANN-03"});
	push("ANN-05", "Energy / momentum residual logging",
		 "ann_05_residuals.vsim", {"ANN-04"});
	push("ANN-06", "Live print integration ([ANN] stream)",
		 "ann_06_live_print.vsim", {"ANN-04"});
	push("ANN-07", "Glue-field disturbance response",
		 "ann_07_glue_field.vsim", {"ANN-05"});
	push("ANN-08", "Eigen trend capture",
		 "ann_08_eigen_trend.vsim", {"ANN-07"});
	push("ANN-09", "Batch sweep",
		 "ann_09_batch_sweep.vsim", {"ANN-08"});
	push("ANN-10", "Heavy proxy annihilation (alpha + anti-alpha)",
		 "ann_10_heavy_proxy.vsim", {"ANN-09"});

	return m;
}

} // namespace vsim::bundle

// ============================================================================
// XBundleFolderLayout  —  canonical on-disk folder structure for a .x bundle
// ============================================================================
//
// 72-floating-7  |  V5.1.4
//
// Defines the minimum folder shape expected around an unpacked or executed .x
// bundle.  Every Day-73/74 archive and comparison tool can use this layout as
// a stable reference.
//
//   <root>/
//     manifest.json         — machine-readable bundle manifest
//     inputs/               — source .vsim scripts (+ assets)
//     outputs/              — xyz / xyzFull / xyzc trajectory files
//     dynx/                 — .dynx session archives
//     xyz/                  — alias / symlink target for outputs/ (optional)
//     reports/              — summary.md, analysis CSV/JSON, energy TSV
//     logs/                 — per-slot stdout/stderr, bench.tsv
//
// Usage:
//   XBundleFolderLayout layout = make_bundle_folder_layout("runs/water_demo");
//   fs::create_directories(layout.inputs);
//   ...

namespace vsim::bundle {

struct XBundleFolderLayout {
    std::string root;        // e.g. "runs/water_demo"
    std::string manifest;    // root + "/manifest.json"
    std::string inputs;      // root + "/inputs"
    std::string outputs;     // root + "/outputs"
    std::string dynx;        // root + "/dynx"
    std::string reports;     // root + "/reports"
    std::string logs;        // root + "/logs"
    std::string xyz;         // root + "/xyz"   (alias / optional)

    // Flat list of subdirectories that must exist for a valid layout.
    std::vector<std::string> required_dirs() const {
        return { inputs, outputs, dynx, reports, logs };
    }
};

inline XBundleFolderLayout make_bundle_folder_layout(const std::string& root) {
    const std::string sep = "/";
    XBundleFolderLayout l;
    l.root     = root;
    l.manifest = root + sep + "manifest.json";
    l.inputs   = root + sep + "inputs";
    l.outputs  = root + sep + "outputs";
    l.dynx     = root + sep + "dynx";
    l.reports  = root + sep + "reports";
    l.logs     = root + sep + "logs";
    l.xyz      = root + sep + "xyz";
    return l;
}

} // namespace vsim::bundle
