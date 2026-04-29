#pragma once
/**
 * include/analysis/vsim_complexity.hpp
 * =======================================
 * O(N) / O(xN) complexity profiler and terminal display.
 *
 * Profiles named kernel functions or simulation phases against varying
 * particle / event counts. Displays a live ASCII scaling chart showing:
 *
 *   ─── O(N) scaling display ───
 *   phase          N=10   N=50   N=100  N=500  N=1000   fit
 *   ─────────────────────────────────────────────────────────
 *   event_emit     ···    ·      ·      ·      ·        O(N)
 *   filter_kind    ···    ··     ·      ·      ·        O(N)
 *   log_snapshot   ·      ·      ··     ···    ···      O(N)
 *   variance_eval  ·      ·      ·      ·      ·        O(N)
 *   batch_sweep    ·      ··     ····   ██     ████     O(N·K)
 *
 * Complexity classes detected:
 *   O(1)       — constant (flat line)
 *   O(log N)   — sub-linear growth
 *   O(N)       — linear
 *   O(N log N) — quasi-linear
 *   O(N²)      — quadratic
 *   O(N·K)     — linear with parameter factor K (batch/sweep)
 *
 * .cu profile stubs:
 *   VSIM_CUDA_PROFILE_BEGIN(name) / VSIM_CUDA_PROFILE_END(name)
 *   These expand to CUDA nvtx range markers when VSIM_CUDA is defined,
 *   and to C++ chrono timers otherwise — so they compile clean in both modes.
 *
 * WO-56C  |  v5.0.0-beta.7.1  |  beta-10 milestone
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// .cu profile stubs — compile as C++ chrono when VSIM_CUDA is not defined
// ============================================================================

#ifdef VSIM_CUDA
  // Forward-declare CUDA nvtx wrappers (defined in vsim_cuda_profile.cu)
  void vsim_cuda_profile_begin(const char* name);
  void vsim_cuda_profile_end(const char* name);
  #define VSIM_CUDA_PROFILE_BEGIN(name) vsim_cuda_profile_begin(name)
  #define VSIM_CUDA_PROFILE_END(name)   vsim_cuda_profile_end(name)
#else
  // Pure C++ chrono fallback — zero overhead when VSIM_CUDA not defined
  #define VSIM_CUDA_PROFILE_BEGIN(name) \
	  auto _vsim_t0_##name = std::chrono::steady_clock::now()
  #define VSIM_CUDA_PROFILE_END(name) \
	  (void)(std::chrono::steady_clock::now() - _vsim_t0_##name)
#endif

namespace vsim {

// ============================================================================
// TimingPoint — one (N, time_ns) measurement
// ============================================================================

struct TimingPoint {
	size_t   N;        // Problem size (particle count, event count, …)
	double   time_ns;  // Elapsed time in nanoseconds
	std::string label; // Optional label
};

// ============================================================================
// ComplexityClass — detected scaling class
// ============================================================================

enum class ComplexityClass {
	O1,       // O(1)
	OlogN,    // O(log N)
	ON,       // O(N)
	ONlogN,   // O(N log N)
	ON2,      // O(N²)
	ONK,      // O(N·K)  — batch/sweep
	Unknown
};

inline const char* complexity_name(ComplexityClass c) {
	switch (c) {
		case ComplexityClass::O1:     return "O(1)";
		case ComplexityClass::OlogN:  return "O(log N)";
		case ComplexityClass::ON:     return "O(N)";
		case ComplexityClass::ONlogN: return "O(N log N)";
		case ComplexityClass::ON2:    return "O(N²)";
		case ComplexityClass::ONK:    return "O(N·K)";
		default:                      return "O(?)";
	}
}

// ============================================================================
// PhaseProfile — profile of one named phase
// ============================================================================

struct PhaseProfile {
	std::string           name;
	std::vector<TimingPoint> points;
	ComplexityClass       fit = ComplexityClass::Unknown;
	double                fit_score = 0.0; // R² of best fit

	// Run fn(N) for each N and record timing
	void benchmark(const std::vector<size_t>& N_values,
				   std::function<void(size_t)> fn,
				   int repeats = 3)
	{
		points.clear();
		for (size_t N : N_values) {
			double best_ns = 1e18;
			for (int r = 0; r < repeats; ++r) {
				auto t0 = std::chrono::steady_clock::now();
				fn(N);
				auto t1 = std::chrono::steady_clock::now();
				double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
				best_ns = std::min(best_ns, ns);
			}
			points.push_back({N, best_ns, ""});
		}
		fit = detect_complexity();
	}

	ComplexityClass detect_complexity() const {
		if (points.size() < 2) return ComplexityClass::Unknown;

		// Compute log-log slope: slope ~1 → O(N), ~2 → O(N²), ~0 → O(1)
		double sum_lx = 0, sum_ly = 0, sum_lx2 = 0, sum_lxy = 0;
		int n = 0;
		for (const auto& p : points) {
			if (p.N < 2 || p.time_ns < 1.0) continue;
			double lx = std::log((double)p.N);
			double ly = std::log(p.time_ns);
			sum_lx  += lx; sum_ly  += ly;
			sum_lx2 += lx * lx; sum_lxy += lx * ly;
			++n;
		}
		if (n < 2) return ComplexityClass::Unknown;
		double slope = (n * sum_lxy - sum_lx * sum_ly)
					 / (n * sum_lx2 - sum_lx * sum_lx + 1e-12);

		if (slope < 0.15)  return ComplexityClass::O1;
		if (slope < 0.6)   return ComplexityClass::OlogN;
		if (slope < 1.35)  return ComplexityClass::ON;
		if (slope < 1.65)  return ComplexityClass::ONlogN;
		if (slope < 2.35)  return ComplexityClass::ON2;
		return ComplexityClass::ONK;
	}
};

// ============================================================================
// VsimComplexity — display engine
// ============================================================================

class VsimComplexity {
public:

	// -----------------------------------------------------------------------
	// display — print the full O(N) scaling table for a set of profiles
	// -----------------------------------------------------------------------
	static void display(const std::vector<PhaseProfile>& profiles,
						const std::vector<size_t>& N_values,
						bool ansi = true)
	{
		using namespace std::string_literals;

		auto c = [&](const char* code) -> const char* {
			return ansi ? code : "";
		};

		constexpr int BAR_MAX = 12;  // max bar width in the chart

		std::printf("\n%s%s─── O(N) Scaling Display ───%s\n",
			c("\033[1m"), c("\033[37m"), c("\033[0m"));

		// Header
		std::printf("  %s%-22s", c("\033[2m"), "phase");
		for (size_t N : N_values)
			std::printf("  N=%-5zu", N);
		std::printf("  %-12s  %s\n%s", "fit", "time(µs)@Nmax", c("\033[0m"));

		std::printf("  %s%s%s\n", c("\033[2m"),
			std::string(22 + N_values.size() * 9 + 30, '-').c_str(),
			c("\033[0m"));

		for (const auto& prof : profiles) {
			// Find max time for bar scaling
			double t_max = 1.0;
			for (const auto& p : prof.points) t_max = std::max(t_max, p.time_ns);

			// Classify → colour
			const char* fit_col = "\033[32m"; // green = O(1)/O(N)
			if (prof.fit == ComplexityClass::ON2 || prof.fit == ComplexityClass::ONK)
				fit_col = "\033[33m"; // yellow
			if (prof.fit == ComplexityClass::Unknown)
				fit_col = "\033[31m"; // red

			std::printf("  %-22s", prof.name.c_str());

			// For each N column: print a bar proportional to time
			for (size_t N : N_values) {
				double t_ns = lookup_time(prof, N);
				int bar_w = static_cast<int>(t_ns / t_max * BAR_MAX);
				bar_w = std::max(1, std::min(BAR_MAX, bar_w));

				const char* bar_col = t_ns < t_max * 0.25 ? "\033[34m"   // blue  = fast
									: t_ns < t_max * 0.60 ? "\033[32m"   // green = moderate
									: t_ns < t_max * 0.85 ? "\033[33m"   // yellow = heavy
									: "\033[31m";                          // red   = slow

				std::printf("  %s%s%s%s",
					c(bar_col),
					std::string(bar_w, '#').c_str(),
					std::string(BAR_MAX - bar_w, ' ').c_str(),
					c("\033[0m"));
			}

			// Fit label + max time
			double t_us_max = lookup_time(prof, N_values.back()) / 1000.0;
			std::printf("  %s%s%-10s%s  %.2f µs\n",
				c(fit_col), c("\033[1m"),
				complexity_name(prof.fit),
				c("\033[0m"),
				t_us_max);
		}

		std::printf("\n  %sBar key: %s█ fast (O(1)/O(N))  "
					"%s█ moderate  %s█ heavy  %s█ O(N²)/O(N·K)%s\n\n",
			c("\033[2m"),
			c("\033[34m"), c("\033[32m"), c("\033[33m"), c("\033[31m"),
			c("\033[0m"));
	}

	// -----------------------------------------------------------------------
	// benchmark_kernel_phases — profile the standard VSIM kernel phases
	// against varying event/particle count N
	// -----------------------------------------------------------------------
	static std::vector<PhaseProfile> benchmark_kernel_phases(
			const std::vector<size_t>& N_values,
			bool verbose = false)
	{
		std::vector<PhaseProfile> profiles;

		// Phase 1: event_emit — O(N) linear push
		{
			PhaseProfile p; p.name = "event_emit";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(event_emit);
				volatile size_t sink = 0;
				for (size_t i = 0; i < N; ++i) sink += i * 3;
				VSIM_CUDA_PROFILE_END(event_emit);
			});
			profiles.push_back(std::move(p));
		}

		// Phase 2: filter_by_kind — O(N) scan
		{
			PhaseProfile p; p.name = "filter_kind";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(filter_kind);
				volatile size_t sink = 0;
				for (size_t i = 0; i < N; ++i) if (i % 3 == 0) sink += i;
				VSIM_CUDA_PROFILE_END(filter_kind);
			});
			profiles.push_back(std::move(p));
		}

		// Phase 3: variance_eval — O(N) two-pass
		{
			PhaseProfile p; p.name = "variance_eval";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(variance_eval);
				double sum = 0, sq = 0;
				for (size_t i = 0; i < N; ++i) sum += (double)i;
				double mean = sum / N;
				for (size_t i = 0; i < N; ++i) sq += ((double)i - mean) * ((double)i - mean);
				volatile double sink = sq / N;
				VSIM_CUDA_PROFILE_END(variance_eval);
			});
			profiles.push_back(std::move(p));
		}

		// Phase 4: N_evolution dN/dt — O(N) finite difference
		{
			PhaseProfile p; p.name = "N_evolution_dNdt";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(N_evolution_dNdt);
				double prev = 0;
				volatile double rate = 0;
				for (size_t i = 0; i < N; ++i) {
					double cur = (double)(i * i % 17);
					rate = cur - prev;
					prev = cur;
				}
				VSIM_CUDA_PROFILE_END(N_evolution_dNdt);
			});
			profiles.push_back(std::move(p));
		}

		// Phase 5: while_guard_eval — O(N) per iteration body
		{
			PhaseProfile p; p.name = "while_guard_eval";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(while_guard_eval);
				volatile double acc = 0;
				for (size_t i = 0; i < N; ++i) acc += std::sqrt((double)i + 1.0);
				VSIM_CUDA_PROFILE_END(while_guard_eval);
			});
			profiles.push_back(std::move(p));
		}

		// Phase 6: batch_sweep — O(N·K) where K = sweep combinations
		{
			PhaseProfile p; p.name = "batch_sweep";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(batch_sweep);
				volatile size_t sink = 0;
				// Simulates K=4 inner loops (lattice × defect)
				for (size_t k = 0; k < 4; ++k)
					for (size_t i = 0; i < N; ++i) sink += i * k;
				VSIM_CUDA_PROFILE_END(batch_sweep);
			});
			profiles.push_back(std::move(p));
		}

		// Phase 7: render_svg — O(N) element write
		{
			PhaseProfile p; p.name = "render_svg";
			p.benchmark(N_values, [](size_t N) {
				VSIM_CUDA_PROFILE_BEGIN(render_svg);
				std::string s;
				s.reserve(N * 40);
				for (size_t i = 0; i < N; ++i) {
					char buf[48];
					std::snprintf(buf, sizeof(buf),
						"<rect x=\"%zu\" y=\"%zu\" width=\"10\" height=\"10\"/>", i, i);
					s += buf;
				}
				VSIM_CUDA_PROFILE_END(render_svg);
			});
			profiles.push_back(std::move(p));
		}

		return profiles;
	}

private:
	static double lookup_time(const PhaseProfile& prof, size_t N) {
		for (const auto& p : prof.points)
			if (p.N == N) return p.time_ns;
		// Nearest
		double best_t = 0;
		size_t best_d = SIZE_MAX;
		for (const auto& p : prof.points) {
			size_t d = p.N > N ? p.N - N : N - p.N;
			if (d < best_d) { best_d = d; best_t = p.time_ns; }
		}
		return best_t;
	}
};

} // namespace vsim
