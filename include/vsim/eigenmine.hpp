#pragma once
// =============================================================================
// eigenmine.hpp  —  EigenMining Presolve Engine  (WO-XSUITE-02B)
// =============================================================================
//
// Runs N_solves = batch_count × seeds_per_batch × systems_per_seed short
// simulations to extract and cluster recurrent eigen-modes before production
// runs.
//
// Pipeline:
//   for each solve:
//     build K_i = Φ(x,v,f,p,e)
//     compute eigen candidates  K_i q_ik = λ_ik q_ik
//     score recurrence and stability
//     emit compact ModeRecord
//   merge → cluster → rank → export eigen basis
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
// EigenMineConfig  —  mirrors the [eigenmine] .X block
// ---------------------------------------------------------------------------

struct EigenMineConfig {
	int64_t     target_solves      = 10'000'000;
	int         batch_count        = 1000;
	int         seeds_per_batch    = 100;
	int         systems_per_seed   = 100;
	std::string matrix_source      = "state_force_event"; // "state_force_event" | "state_only"
	int         mode_rank_limit    = 256;
	double      min_recurrence     = 0.70;
	double      max_recon_error    = 0.05;
	std::string write_modes;        // path for eigenmine_modes.ndjson
	std::string write_basis;        // path for eigenmine_basis.bin
	std::string write_summary;      // path for eigenmine_report.md
};

// ---------------------------------------------------------------------------
// SolveVector  —  compact per-solve state snapshot
// ---------------------------------------------------------------------------

struct SolveVector {
	int64_t     seed       = 0;
	int         batch      = 0;
	int         system_idx = 0;
	std::vector<double> x; // state vector
	std::vector<double> v; // velocity vector
	std::vector<double> f; // force vector
	std::vector<double> p; // parameter vector
	std::vector<double> e; // event vector
};

// ---------------------------------------------------------------------------
// ModeRecord  —  one extracted eigen candidate
// ---------------------------------------------------------------------------

struct ModeRecord {
	int64_t     seed           = 0;
	int         batch          = 0;
	int         mode_index     = 0;
	double      eigenvalue     = 0.0;
	double      recurrence     = 0.0;    // R_k  in [0,1]
	double      stability      = 0.0;    // S_k  in [0,1]
	double      coupling       = 0.0;    // C_k  observable coupling
	double      recon_error    = 0.0;    // ε_k  reconstruction error
	double      utility        = 0.0;    // U_k = αR + βS + γC - δε
	std::string basis_hash;              // hex hash of eigenvector
	std::vector<double> eigenvector;
};

// ---------------------------------------------------------------------------
// EigenMineResult
// ---------------------------------------------------------------------------

struct EigenMineResult {
	bool        ok              = false;
	std::string error;
	int64_t     solves_run      = 0;
	int         modes_extracted = 0;
	int         modes_retained  = 0;    // after recurrence / error filter
	std::vector<ModeRecord> retained_modes;
	std::string basis_path;
	std::string summary_path;
	std::string modes_path;
};

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

// Caller provides a short-solve function: given a SolveVector seed, fills in
// x/v/f/p/e and returns true on success.
using SolveFn = std::function<bool(SolveVector&)>;

// ---------------------------------------------------------------------------
// eigenmine_run  —  top-level entry point
// ---------------------------------------------------------------------------

EigenMineResult eigenmine_run(
	const EigenMineConfig& cfg,
	SolveFn                solve_fn,
	std::ostream&          log = std::cout);

// ---------------------------------------------------------------------------
// eigenmine_score_mode  —  compute utility score for a ModeRecord
// Weights: α=0.4, β=0.3, γ=0.2, δ=0.1 (default)
// ---------------------------------------------------------------------------

inline double eigenmine_score_mode(const ModeRecord& m,
	double alpha = 0.4,
	double beta  = 0.3,
	double gamma = 0.2,
	double delta = 0.1)
{
	return alpha * m.recurrence
		 + beta  * m.stability
		 + gamma * m.coupling
		 - delta * m.recon_error;
}

// ---------------------------------------------------------------------------
// eigenmine_passes_gate  —  true when mode meets quality thresholds
// ---------------------------------------------------------------------------

inline bool eigenmine_passes_gate(const ModeRecord& m, const EigenMineConfig& cfg) {
	return m.recurrence >= cfg.min_recurrence
		&& m.recon_error <= cfg.max_recon_error;
}

} // namespace presolve
} // namespace vsepr
