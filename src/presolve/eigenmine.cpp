// =============================================================================
// eigenmine.cpp  —  EigenMining Presolve Engine  (WO-XSUITE-02B)
// =============================================================================

#include "vsim/eigenmine.hpp"
#include "vsim/basis_archive.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// Internal: build K matrix from a SolveVector, return eigenvalues+vectors.
// For the presolve layer a symmetric real matrix is assembled from the outer
// products of the state/force/event subvectors.  Full eigen-decomposition is
// approximated via power-iteration for the top `rank` modes.
// ---------------------------------------------------------------------------

namespace detail {

// Simple symmetric matrix stored row-major
struct SymMat {
	int n = 0;
	std::vector<double> data;
	double& at(int r, int c) { return data[r * n + c]; }
	const double& at(int r, int c) const { return data[r * n + c]; }
};

// Assemble K = Φ(x,v,f,p,e) as a normalised outer-product sum
static SymMat build_K(const SolveVector& sv) {
	// Concatenate all sub-vectors
	std::vector<double> all;
	all.insert(all.end(), sv.x.begin(), sv.x.end());
	all.insert(all.end(), sv.v.begin(), sv.v.end());
	all.insert(all.end(), sv.f.begin(), sv.f.end());
	all.insert(all.end(), sv.p.begin(), sv.p.end());
	all.insert(all.end(), sv.e.begin(), sv.e.end());

	int n = static_cast<int>(all.size());
	if (n == 0) n = 1;

	// Normalise
	double norm = 0.0;
	for (double v : all) norm += v * v;
	norm = std::sqrt(norm);
	if (norm > 0.0) for (double& v : all) v /= norm;

	SymMat K;
	K.n = n;
	K.data.assign(static_cast<size_t>(n * n), 0.0);
	// K = diag(all) + outer(all, all)  — captures both diagonal energy and coupling
	for (int i = 0; i < n; ++i) {
		K.at(i, i) = all[i] * all[i] + 1.0;
		for (int j = i + 1; j < n; ++j) {
			double v = all[i] * all[j];
			K.at(i, j) = v;
			K.at(j, i) = v;
		}
	}
	return K;
}

// One step of power iteration: q_new = K·q / ‖K·q‖
static double power_step(const SymMat& K, std::vector<double>& q) {
	int n = K.n;
	std::vector<double> kq(n, 0.0);
	for (int i = 0; i < n; ++i)
		for (int j = 0; j < n; ++j)
			kq[i] += K.at(i, j) * q[j];
	double norm = 0.0;
	for (double v : kq) norm += v * v;
	norm = std::sqrt(norm);
	if (norm > 0.0) for (int i = 0; i < n; ++i) q[i] = kq[i] / norm;
	return norm; // Rayleigh quotient approximation
}

// Extract top `rank` eigen candidates via deflated power iteration
static void extract_top_modes(
	const SymMat& K,
	int           rank,
	std::vector<double>& eigenvalues_out,
	std::vector<std::vector<double>>& eigenvectors_out,
	uint64_t seed_val)
{
	int n = K.n;
	rank = std::min(rank, n);
	eigenvalues_out.clear();
	eigenvectors_out.clear();

	// Working copy for deflation
	SymMat M = K;

	// Simple LCG seeding for deterministic start vector
	uint64_t rng = seed_val ^ 0xDEADBEEFCAFEULL;
	auto lcg_next = [&]() -> double {
		rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<double>(rng >> 33) / static_cast<double>(1ULL << 31) - 1.0;
	};

	for (int k = 0; k < rank; ++k) {
		std::vector<double> q(n);
		for (int i = 0; i < n; ++i) q[i] = lcg_next();
		double qnorm = 0.0;
		for (double v : q) qnorm += v * v;
		qnorm = std::sqrt(qnorm);
		if (qnorm > 0.0) for (double& v : q) v /= qnorm;

		double lambda = 0.0;
		for (int iter = 0; iter < 64; ++iter)
			lambda = power_step(M, q);

		// Deflate: M = M - λ·q·qᵀ
		for (int i = 0; i < n; ++i)
			for (int j = 0; j < n; ++j)
				M.at(i, j) -= lambda * q[i] * q[j];

		eigenvalues_out.push_back(lambda);
		eigenvectors_out.push_back(q);
	}
}

} // namespace detail

// ---------------------------------------------------------------------------
// eigenmine_run
// ---------------------------------------------------------------------------

EigenMineResult eigenmine_run(
	const EigenMineConfig& cfg,
	SolveFn                solve_fn,
	std::ostream&          log)
{
	EigenMineResult res;

	if (cfg.batch_count <= 0 || cfg.seeds_per_batch <= 0 || cfg.systems_per_seed <= 0) {
		res.error = "eigenmine: batch_count / seeds_per_batch / systems_per_seed must be > 0";
		return res;
	}

	int64_t total = static_cast<int64_t>(cfg.batch_count)
				  * cfg.seeds_per_batch
				  * cfg.systems_per_seed;

	log << "[eigenmine] target_solves=" << cfg.target_solves
		<< "  planned=" << total
		<< "  batches=" << cfg.batch_count
		<< "  seeds/batch=" << cfg.seeds_per_batch
		<< "  systems/seed=" << cfg.systems_per_seed << "\n";

	std::vector<ModeRecord> all_modes;
	int64_t solves_run = 0;

	for (int batch = 0; batch < cfg.batch_count; ++batch) {
		for (int s = 0; s < cfg.seeds_per_batch; ++s) {
			int64_t seed_val = static_cast<int64_t>(batch) * cfg.seeds_per_batch + s;

			for (int sys = 0; sys < cfg.systems_per_seed; ++sys) {
				SolveVector sv;
				sv.seed       = seed_val;
				sv.batch      = batch;
				sv.system_idx = sys;

				if (!solve_fn(sv)) continue;
				++solves_run;

				// Build K and extract eigen candidates
				auto K = detail::build_K(sv);
				int rank = std::min(cfg.mode_rank_limit, K.n);

				std::vector<double> eigenvalues;
				std::vector<std::vector<double>> eigenvectors;
				detail::extract_top_modes(K, rank, eigenvalues, eigenvectors,
										  static_cast<uint64_t>(seed_val * 1000 + sys));

				for (int k = 0; k < static_cast<int>(eigenvalues.size()); ++k) {
					ModeRecord m;
					m.seed        = seed_val;
					m.batch       = batch;
					m.mode_index  = k;
					m.eigenvalue  = eigenvalues[k];
					m.eigenvector = eigenvectors[k];
					m.basis_hash  = basis_archive_hash_vec(m.eigenvector);

					// Stub scoring: recurrence and stability from eigenvalue magnitude
					double lam_norm = std::abs(m.eigenvalue) / (1.0 + std::abs(m.eigenvalue));
					m.recurrence = lam_norm;
					m.stability  = lam_norm * 0.9;
					m.coupling   = lam_norm * 0.5;
					m.recon_error = 1.0 - lam_norm;
					m.utility    = eigenmine_score_mode(m);

					if (eigenmine_passes_gate(m, cfg))
						all_modes.push_back(std::move(m));
				}
			}
		}
		if ((batch + 1) % std::max(1, cfg.batch_count / 10) == 0) {
			log << "[eigenmine]   batch " << (batch + 1) << "/" << cfg.batch_count
				<< "  solves=" << solves_run
				<< "  modes_retained=" << all_modes.size() << "\n";
		}
	}

	res.solves_run      = solves_run;
	res.modes_extracted = static_cast<int>(
		static_cast<int64_t>(cfg.batch_count) * cfg.seeds_per_batch
		* cfg.systems_per_seed * std::min(cfg.mode_rank_limit, 1));
	res.modes_retained  = static_cast<int>(all_modes.size());
	res.retained_modes  = std::move(all_modes);

	// Write modes ndjson
	if (!cfg.write_modes.empty()) {
		for (const auto& m : res.retained_modes)
			basis_archive_append_mode(m, cfg.write_modes);
		res.modes_path = cfg.write_modes;
		log << "[eigenmine] modes written -> " << cfg.write_modes << "\n";
	}

	// Write binary basis
	if (!cfg.write_basis.empty()) {
		if (basis_archive_write_bin(res.retained_modes, cfg.write_basis))
			res.basis_path = cfg.write_basis;
		log << "[eigenmine] basis written -> " << cfg.write_basis << "\n";
	}

	// Write summary markdown
	if (!cfg.write_summary.empty()) {
		std::ofstream f(cfg.write_summary);
		if (f) {
			f << "# EigenMine Summary\n\n"
			  << "| Field | Value |\n|---|---|\n"
			  << "| solves_run | " << res.solves_run << " |\n"
			  << "| modes_retained | " << res.modes_retained << " |\n"
			  << "| min_recurrence_threshold | " << cfg.min_recurrence << " |\n"
			  << "| max_recon_error_threshold | " << cfg.max_recon_error << " |\n";
			res.summary_path = cfg.write_summary;
			log << "[eigenmine] summary written -> " << cfg.write_summary << "\n";
		}
	}

	res.ok = true;
	log << "[eigenmine] DONE  solves=" << res.solves_run
		<< "  retained=" << res.modes_retained << "\n";
	return res;
}

} // namespace presolve
} // namespace vsepr
