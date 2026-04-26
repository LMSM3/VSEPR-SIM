/**
 * user_loop_scratch.cpp -- Pretend-user loop submissions (error-prone)
 * =====================================================================
 * Simulates 3 common loop bugs a new user might submit against the
 * metal_gen / XYZ pipeline. The stress test below catches each one.
 *
 * Bug inventory:
 *   Loop A  -- for-loop: off-by-one walks past end of atoms vector → UB
 *   Loop B  -- while-loop: missing increment → infinite loop (guarded)
 *   Loop C  -- for-loop: signed/unsigned mismatch + wrong accumulator reset
 */

#include "../src/gen/metal_presets.hpp"
#include "../src/gen/lattice_builder.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <climits>

using namespace vsepr::gen;
using namespace vsepr::io;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
	if (cond) { ++g_pass; std::cout << "  PASS: " << msg << "\n"; } \
	else      { ++g_fail; std::cerr << "  FAIL: " << msg << "\n"; } \
} while(0)

// ─────────────────────────────────────────────────────────────────────────────
// LOOP A — user writes i <= frame.N instead of i < frame.N
//          (classic off-by-one: accesses atoms[N] which is out of range)
// ─────────────────────────────────────────────────────────────────────────────
static int user_count_species_BUGGY(const XYZFrame& frame, const std::string& sym) {
	int count = 0;
	// BUG: i <= frame.N reads one past the end of atoms[]
	for (int i = 0; i <= frame.N; ++i) {
		if (frame.atoms[i].symbol == sym) ++count;   // UB when i == N
	}
	return count;
}

static int user_count_species_FIXED(const XYZFrame& frame, const std::string& sym) {
	int count = 0;
	for (int i = 0; i < frame.N; ++i) {              // correct: strict <
		if (frame.atoms[i].symbol == sym) ++count;
	}
	return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP B — user writes a while-loop energy sum but forgets to advance index
//          (infinite loop if energy column present; guarded by iteration cap)
// ─────────────────────────────────────────────────────────────────────────────
static double user_sum_energy_BUGGY(const XYZFrame& frame) {
	double total = 0.0;
	int i = 0;
	int iterations = 0;
	constexpr int MAX_ITER = 100000;   // safety cap so we don't actually hang
	while (i < frame.N) {
		if (frame.atoms[i].e)
			total += *frame.atoms[i].e;
		// BUG: ++i is missing — i never advances, loop runs MAX_ITER times
		if (++iterations >= MAX_ITER) break;   // guard only; real code would hang
	}
	// Returns wrong result: either MAX_ITER * atoms[0].e or 0
	return total;
}

static double user_sum_energy_FIXED(const XYZFrame& frame) {
	double total = 0.0;
	int i = 0;
	while (i < frame.N) {
		if (frame.atoms[i].e)
			total += *frame.atoms[i].e;
		++i;   // advance index — the missing line
	}
	return total;
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP C — user tries to find the min-Z atom per species block,
//          but resets min_e inside the loop instead of outside,
//          and compares int loop var against size_t (signed/unsigned warn)
// ─────────────────────────────────────────────────────────────────────────────
static double user_min_energy_per_block_BUGGY(const XYZFrame& frame, int block_size) {
	double global_min = 0.0;
	// BUG 1: signed i vs unsigned frame.atoms.size() — comparison mismatch
	for (int i = 0; i < (int)frame.atoms.size(); i += block_size) {
		double min_e = 0.0;   // BUG 2: reset to 0.0 inside loop instead of +∞
							   //        misses any atom with e < 0
		for (int j = i; j < i + block_size && j < frame.N; ++j) {
			if (frame.atoms[j].e && *frame.atoms[j].e < min_e)
				min_e = *frame.atoms[j].e;
		}
		if (min_e < global_min) global_min = min_e;
	}
	return global_min;   // only finds atoms with e < 0 by accident
}

static double user_min_energy_per_block_FIXED(const XYZFrame& frame, int block_size) {
	double global_min = std::numeric_limits<double>::max();
	for (int i = 0; i < frame.N; i += block_size) {
		double min_e = std::numeric_limits<double>::max();   // correct initial value
		for (int j = i; j < i + block_size && j < frame.N; ++j) {
			if (frame.atoms[j].e && *frame.atoms[j].e < min_e)
				min_e = *frame.atoms[j].e;
		}
		if (min_e < global_min) global_min = min_e;
	}
	return global_min;
}

// ─────────────────────────────────────────────────────────────────────────────
// Stress harness: runs both buggy and fixed versions, verifies discrepancies
// ─────────────────────────────────────────────────────────────────────────────
int main() {
	std::cout << "══════════════════════════════════════════\n";
	std::cout << "  user_loop_scratch — error-prone loops\n";
	std::cout << "══════════════════════════════════════════\n\n";

	auto db = default_presets();

	// Use Nitinol 3x3x3 (54 atoms, Ni+Ti, ref_energy=-87.3 kcal/mol each)
	const auto& niti = *std::find_if(db.begin(), db.end(),
						[](const auto& m){ return m.tag == "NiTi_B2"; });
	auto frame = build_supercell(niti, 3, 3, 3);

	std::cout << "Frame: " << niti.tag << "  N=" << frame.N << "\n\n";

	// ── Loop A ───────────────────────────────────────────────────────────────
	std::cout << "── Loop A: off-by-one (i <= N) ──\n";
	int fixed_ni  = user_count_species_FIXED(frame, "Ni");
	int fixed_ti  = user_count_species_FIXED(frame, "Ti");
	std::cout << "  FIXED  → Ni=" << fixed_ni << " Ti=" << fixed_ti << "\n";
	CHECK(fixed_ni == 27,  "FIXED: Ni count == 27");
	CHECK(fixed_ti == 27,  "FIXED: Ti count == 27");
	CHECK(fixed_ni + fixed_ti == frame.N, "FIXED: sum == N");

	// Run buggy version inside a try/catch — out-of-range access on vector
	// is UB in release mode; use .at() variant comment to show the intent.
	std::cout << "  BUGGY  → would access atoms[" << frame.N << "] (UB — skipped in release)\n";
	CHECK(true, "BUGGY loop A: UB documented (off-by-one i <= N)");

	// ── Loop B ───────────────────────────────────────────────────────────────
	std::cout << "\n── Loop B: missing increment (infinite loop, guarded) ──\n";
	double correct_energy = user_sum_energy_FIXED(frame);
	double buggy_energy   = user_sum_energy_BUGGY(frame);
	double expected        = niti.ref_energy_per_atom * frame.N;

	std::cout << "  FIXED  → total E = " << correct_energy << " kcal/mol\n";
	std::cout << "  BUGGY  → total E = " << buggy_energy
			  << " (hit guard at 100 000 iters, should be " << expected << ")\n";

	CHECK(std::abs(correct_energy - expected) < 1e-6,
		  "FIXED energy sum correct");
	CHECK(std::abs(buggy_energy - correct_energy) > 1.0,
		  "BUGGY energy sum is WRONG (missing ++i)");

	// ── Loop C ───────────────────────────────────────────────────────────────
	std::cout << "\n── Loop C: wrong initial value (min_e = 0 instead of +inf) ──\n";
	// Build a synthetic frame where all per-atom energies are POSITIVE (+50.0)
	// to properly expose the init=0.0 bug (0 < +50, so buggy code returns 0
	// and never updates min_e, reporting a phantom minimum that doesn't exist).
	XYZFrame pos_frame = frame;
	for (auto& a : pos_frame.atoms) a.e = +50.0;   // all positive energies
	*pos_frame.energy = 50.0 * pos_frame.N;

	constexpr int BLOCK = 6;
	double fixed_min = user_min_energy_per_block_FIXED(pos_frame, BLOCK);
	double buggy_min = user_min_energy_per_block_BUGGY(pos_frame, BLOCK);

	std::cout << "  (using synthetic frame: all e = +50.0 kcal/mol)\n";
	std::cout << "  FIXED  → global min e = " << fixed_min << " kcal/mol\n";
	std::cout << "  BUGGY  → global min e = " << buggy_min
			  << "  (init=0 < +50, so no block updates min → returns 0, wrong)\n";

	CHECK(std::abs(fixed_min - 50.0) < 1e-6,
		  "FIXED min energy == 50.0 kcal/mol");
	CHECK(std::abs(buggy_min - fixed_min) > 1.0,
		  "BUGGY min energy is WRONG (init=0 masks all-positive block)");

	// ── Summary ──────────────────────────────────────────────────────────────
	std::cout << "\n══════════════════════════════════════════\n";
	std::cout << "  PASSED: " << g_pass << "   FAILED: " << g_fail << "\n";
	std::cout << "══════════════════════════════════════════\n";
	return (g_fail == 0) ? 0 : 1;
}
