#pragma once
// =============================================================================
// solve_batch.hpp  —  Live Persistent Instance + Batch Solve Layer
//                     (WO-XSUITE-02B)
// =============================================================================
//
// Defines:
//   LiveXSuiteInstance   — runtime state object that persists across frames
//   BatchSolveConfig     — mirrors [presolve] .X block
//   BatchSolveRecord     — per-seed compact result
//   BatchSolveResult     — aggregate of all seeds
//   batch_solve_run(...)  — top-level executor
//
// Live state equation:
//   L(t) = [S(t), D(t), A(t), R(t), H(t)]
//
// Hard rule: R(t) must NOT mutate S(t).
//
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <ostream>
#include <iostream>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// State sub-objects  (opaque to presolve layer — caller manages internals)
// ---------------------------------------------------------------------------

struct TruthState   { std::vector<double> pos; std::vector<double> vel; };
struct DynState     { std::vector<double> force; double ke = 0.0; double pe = 0.0; };
struct AnalysisState{ std::vector<double> rdf; double msd = 0.0; };
struct RenderState  { bool dirty = false; uint64_t last_frame = 0; };

struct EventEntry   { std::string type; uint64_t step = 0; std::string payload; };
struct MetricEntry  { std::string name; double value = 0.0; uint64_t step = 0; };

struct HashRecord {
	std::string state_hash;
	std::string trajectory_hash;
	std::string suite_hash;
};

// ---------------------------------------------------------------------------
// LiveXSuiteInstance  —  runtime session object
// ---------------------------------------------------------------------------

struct LiveXSuiteInstance {
	std::string suite_path;
	std::string suite_name;

	uint64_t instance_id  = 0;
	uint64_t frame        = 0;
	uint64_t step         = 0;

	double   time  = 0.0;
	double   dt    = 1.0e-15;

	TruthState    truth;
	DynState      dynamics;
	AnalysisState analysis;
	RenderState   render;    // render state is isolated — must not write to truth

	HashRecord             hash;
	std::vector<EventEntry>  events;
	std::vector<MetricEntry> metrics;

	bool running     = false;
	bool paused      = false;
	bool replay_mode = false;
	bool batch_mode  = false;
};

// ---------------------------------------------------------------------------
// BatchSolveConfig  —  mirrors [presolve] .X block
// ---------------------------------------------------------------------------

struct BatchSolveConfig {
	std::string mode          = "batch_eigen"; // "batch_eigen" | "batch_only"
	int         batch_count   = 16;
	int         seed_start    = 1000;
	int         seed_stride   = 1;
	int         max_steps     = 2000;
	bool        extract_matrix = true;
	bool        extract_eigen  = true;
	std::string write_basis;    // archive/eigen_basis.ndjson
	std::string write_summary;  // reports/presolve_summary.md
};

// ---------------------------------------------------------------------------
// BatchSolveRecord  —  one seed's result
// ---------------------------------------------------------------------------

struct BatchSolveRecord {
	int         seed          = 0;
	int         batch         = 0;
	bool        converged     = false;
	int         steps_run     = 0;
	double      final_energy  = 0.0;
	std::string basis_hash;
	// Serialised matrix rows  (row-major, n_atoms×3 for positions etc.)
	std::vector<double> X_final;   // position matrix
	std::vector<double> V_final;   // velocity matrix
	std::vector<double> F_final;   // force matrix
	// Eigen candidates extracted from this seed
	std::vector<double> eigenvalues;
	std::vector<double> eigenvectors; // flattened: [mode0...][mode1...]
};

// ---------------------------------------------------------------------------
// BatchSolveResult
// ---------------------------------------------------------------------------

struct BatchSolveResult {
	bool        ok              = false;
	std::string error;
	int         seeds_run       = 0;
	int         seeds_converged = 0;
	double      mean_energy     = 0.0;
	std::string basis_path;
	std::string summary_path;
	std::vector<BatchSolveRecord> records;
};

// ---------------------------------------------------------------------------
// Callback type: caller provides a short simulation step function.
// Given a seed and max_steps, fills in a BatchSolveRecord.
// ---------------------------------------------------------------------------

using ShortSolveFn = std::function<bool(int seed, int max_steps, BatchSolveRecord&)>;

// ---------------------------------------------------------------------------
// batch_solve_run  —  execute the full presolve batch
// ---------------------------------------------------------------------------

BatchSolveResult batch_solve_run(
	const BatchSolveConfig& cfg,
	ShortSolveFn            solve_fn,
	std::ostream&           log = std::cout);

// ---------------------------------------------------------------------------
// live_instance_flush  —  write state / health snapshots to streams
//   state_stream  — atomic_stream path from [render]
//   health_stream — analysis_stream path from [render]
// ---------------------------------------------------------------------------

void live_instance_flush_state(
	const LiveXSuiteInstance& inst,
	const std::string&        state_stream_path);

void live_instance_flush_health(
	const LiveXSuiteInstance& inst,
	const std::string&        health_stream_path);

} // namespace presolve
} // namespace vsepr
