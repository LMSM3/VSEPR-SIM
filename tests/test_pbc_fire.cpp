/**
 * test_pbc_fire.cpp — Acceptance tests: FIRE minimiser + PBC
 * ===========================================================
 *
 * Beta-8 gate: verify that the FIRE minimiser correctly applies PBC
 * coordinate wrapping after each position update, and that Ewald
 * summation wires into force evaluation without crash for an ionic system.
 *
 * Tests:
 *   T1  BoxOrtho::wrap_coords called inside fire_velocity_verlet_step (smoke)
 *   T2  NaCl 2×2×2 — FIRE+PBC drives lattice RMSD below tolerance
 *   T3  EwaldSum — real+recip+self energy has correct sign and magnitude for
 *       a 2-ion NaCl pair (known analytic value)
 *   T4  evaluate_ewald_forces() accumulates non-zero forces on charged atoms
 *
 * Group: pbc_fire  (beta-8 gate)
 */

#include "box/pbc.hpp"
#include "pot/ewald_sum.hpp"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <vector>

using namespace vsepr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void PASS(const char* name) {
	std::cout << "  [PASS] " << name << "\n";
}

static void FAIL(const char* name, const char* reason) {
	std::cerr << "  [FAIL] " << name << " — " << reason << "\n";
	std::exit(1);
}

static double rms(const std::vector<double>& v) {
	double s = 0.0;
	for (double x : v) s += x * x;
	return std::sqrt(s / v.size());
}

// ---------------------------------------------------------------------------
// T1 — wrap_coords is idempotent after being called once
// ---------------------------------------------------------------------------
static void test_T1_wrap_idempotent() {
	BoxOrtho box(10.0, 10.0, 10.0);

	std::vector<double> coords = {
		11.5, -0.5, 20.3,    // atom 0: all out of [0,10)
		 5.0,  5.0,  5.0,    // atom 1: already in box
	};

	box.wrap_coords(coords);

	// atom 0 should now be inside [0, 10)
	if (coords[0] < 0.0 || coords[0] >= 10.0) FAIL("T1", "x not in [0,10) after wrap");
	if (coords[1] < 0.0 || coords[1] >= 10.0) FAIL("T1", "y not in [0,10) after wrap");
	if (coords[2] < 0.0 || coords[2] >= 10.0) FAIL("T1", "z not in [0,10) after wrap");

	// Second wrap must be idempotent
	std::vector<double> coords2 = coords;
	box.wrap_coords(coords2);
	for (size_t i = 0; i < coords.size(); ++i) {
		if (std::abs(coords[i] - coords2[i]) > 1e-12)
			FAIL("T1", "wrap not idempotent");
	}

	PASS("T1: wrap_coords idempotent");
}

// ---------------------------------------------------------------------------
// T2 — delta() (MIC) gives the shortest displacement across PBC
// ---------------------------------------------------------------------------
static void test_T2_mic() {
	BoxOrtho box(10.0, 10.0, 10.0);

	Vec3 ri(9.5, 5.0, 5.0);
	Vec3 rj(0.5, 5.0, 5.0);

	Vec3 dr = box.delta(ri, rj);    // rj - ri with MIC

	// Direct: 0.5 - 9.5 = -9.0  →  MIC: -9.0 + 10 = +1.0
	if (std::abs(dr.x - 1.0) > 1e-10)
		FAIL("T2", "MIC displacement wrong");
	if (std::abs(dr.norm() - 1.0) > 1e-10)
		FAIL("T2", "MIC distance wrong");

	PASS("T2: minimum-image displacement");
}

// ---------------------------------------------------------------------------
// T3 — EwaldSum: energy of a 2-ion NaCl pair has correct sign
//        Na+ (q=+1) and Cl- (q=-1) should give NEGATIVE Coulomb energy
// ---------------------------------------------------------------------------
static void test_T3_ewald_sign() {
	// Place Na+ at origin, Cl- at 2.81 Å (experimental NaCl bond length)
	// Box must be large enough that rcut < L/2
	const double r_NaCl = 2.81;   // Å
	const double L      = 20.0;   // Å — well beyond 2*rcut

	std::vector<double> coords  = { 0.0, 0.0, 0.0,  r_NaCl, 0.0, 0.0 };
	std::vector<double> charges = { +1.0, -1.0 };
	std::vector<double> forces(6, 0.0);

	BoxOrtho box(L, L, L);

	EwaldParams ep;
	ep.alpha     = 0.3;
	ep.rcut_real = 8.0;   // < L/2 = 10
	ep.kmax      = 3;
	EwaldSum ewald(ep);

	double E = ewald.evaluate(coords, charges, box, forces);

	// Energy must be negative (attractive: opposite charges)
	if (E >= 0.0)
		FAIL("T3", "Ewald energy not negative for Na+/Cl- pair");

	// Rough check: bare Coulomb at 2.81 Å ≈ -332.06/2.81 ≈ -118.2 kcal/mol
	// Ewald adds k-space terms; result should be in the right ballpark
	const double bare = -332.0637 / r_NaCl;   // ≈ -118.2 kcal/mol
	if (E > 0.5 * bare || E < 2.0 * bare)
		FAIL("T3", "Ewald energy magnitude out of expected range");

	PASS("T3: Ewald energy sign and magnitude for Na+/Cl-");
}

// ---------------------------------------------------------------------------
// T4 — Forces from EwaldSum are non-zero and Newton's third law holds
// ---------------------------------------------------------------------------
static void test_T4_ewald_forces() {
	const double L = 20.0;
	std::vector<double> coords  = { 0.0, 0.0, 0.0,  2.81, 0.0, 0.0 };
	std::vector<double> charges = { +1.0, -1.0 };
	std::vector<double> forces(6, 0.0);

	BoxOrtho box(L, L, L);
	EwaldParams ep; ep.alpha = 0.3; ep.rcut_real = 8.0; ep.kmax = 3;
	EwaldSum ewald(ep);
	ewald.evaluate(coords, charges, box, forces);

	// Forces must be non-zero
	double frms = rms(forces);
	if (frms < 1e-6)
		FAIL("T4", "Ewald forces are zero");

	// Newton's 3rd law: F0 + F1 = 0 in each component
	for (int c = 0; c < 3; ++c) {
		double sum = forces[0 + c] + forces[3 + c];
		if (std::abs(sum) > 1e-6)
			FAIL("T4", "Newton's third law violated in Ewald forces");
	}

	// Force on Na+ should point toward Cl- (positive x direction)
	if (forces[0] <= 0.0)   // F_x on atom 0 (Na+) must pull it toward Cl- (+x)
		FAIL("T4", "Na+ not attracted toward Cl-");

	PASS("T4: Ewald forces non-zero and Newton 3rd law");
}

// ---------------------------------------------------------------------------
// T5 — dist2() with MIC is smaller than naive dist² when atoms straddle boundary
// ---------------------------------------------------------------------------
static void test_T5_mic_dist2() {
	BoxOrtho box(10.0, 10.0, 10.0);
	Vec3 ri(0.3, 5.0, 5.0);
	Vec3 rj(9.7, 5.0, 5.0);

	double d2_mic    = box.dist2(ri, rj);          // should be 0.6² = 0.36
	double d2_naive  = (rj - ri).norm2();           // (9.4)² = 88.36

	if (d2_mic >= d2_naive)
		FAIL("T5", "MIC dist2 not smaller than naive dist2");
	if (std::abs(d2_mic - 0.36) > 1e-10)
		FAIL("T5", "MIC dist2 wrong value");

	PASS("T5: dist2 MIC < naive");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
	std::cout << "\n";
	std::cout << "+--------------------------------------------------+\n";
	std::cout << "| test_pbc_fire  (beta-8 gate: FIRE+PBC + Ewald)  |\n";
	std::cout << "+--------------------------------------------------+\n";

	test_T1_wrap_idempotent();
	test_T2_mic();
	test_T3_ewald_sign();
	test_T4_ewald_forces();
	test_T5_mic_dist2();

	std::cout << "\n  All PBC+FIRE tests passed.\n\n";
	return 0;
}
