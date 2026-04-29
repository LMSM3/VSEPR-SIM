/**
 * test_pbc_ewald_caf2.cpp — Empirical Ewald validation: CaF2 fluorite 4×4×4 supercell
 * ======================================================================================
 *
 * Group 28 — Beta-8 PBC/Ewald Empirical Validation
 *
 * TEST(PBC_Ewald_CaF2, Fluorite_4x4x4_MadelungValidation)
 *
 * Validates Ewald electrostatics against the analytic Madelung energy for
 * the CaF2 fluorite crystal.  This is a stronger test than NaCl because:
 *
 *   1. Mixed charge magnitudes: Ca²⁺ (+2) and F⁻ (−1)
 *   2. Unequal self-energy contributions: Ca contributes q²=4, F contributes q²=1
 *   3. 3-ion formula unit (Ca + 2F) — formula-unit normalization must be correct
 *   4. Larger basis: 12 ions per conventional cell, 768 ions total in 4×4×4 supercell
 *   5. Harder neutral-cell check: 256 Ca²⁺ + 512 F⁻ must sum to zero charge
 *
 * Reference data:
 *   Structure           Fluorite, cubic Fm-3m (#225)          Materials Project / product datasheet
 *   Lattice constant    a = 5.4620 Å                          CaF2 product datasheet
 *   Madelung constant   A = 2.51939                           Louisiana Tech Madelung constants table
 *   Coordination        Ca CN=8, F CN=4                       same table
 *   Nearest Ca-F        r0 = (√3/4) × a = 2.3651 Å
 *   Charges             Ca +2, F −1
 *   E_Madelung/formula  −A × |z+×z−| × 332.0637 / r0
 *                     = −2.51939 × 2 × 332.0637 / 2.3651
 *                     ≈ −707.4479 kcal/mol per CaF2 formula unit
 *
 * Supercell:
 *   Conventional cell: 4 Ca + 8 F = 12 ions, 4 formula units
 *   4×4×4 supercell:   256 Ca + 512 F = 768 ions, 256 formula units
 *   Box:               4 × 5.4620 = 21.8480 Å on each side
 *
 * PASS thresholds:
 *   Gate PASS     |error| < 1.00 %
 *   Strong PASS   |error| < 0.25 %
 *   Excellent     |error| < 0.10 %
 *
 * Day #57A  |  WO-56C  |  beta-8 gate
 */

#include "box/pbc.hpp"
#include "pot/ewald_sum.hpp"

#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <cassert>

using namespace vsepr;

// ============================================================================
// CaF2 fluorite structure builder
//
// Conventional cell (Fm-3m, Z=4):
//   Ca (Wyckoff 4a):  (0,0,0)-type face-centred positions
//     (0,0,0), (0,½,½), (½,0,½), (½,½,0)            × fractional
//   F  (Wyckoff 8c):  (¼,¼,¼)-type and (¾,¾,¾)-type
//     (¼,¼,¼), (¼,¾,¾), (¾,¼,¾), (¾,¾,¼),
//     (¾,¾,¾), (¾,¼,¼), (¼,¾,¼), (¼,¼,¾)            × fractional
//
// Total: 4 Ca + 8 F = 12 ions per conventional cell, 4 formula units.
// ============================================================================

struct CaF2System {
	std::vector<double> coords;   // flat [x0,y0,z0, ...]
	std::vector<double> charges;  // +2 (Ca) or -1 (F)
	int N        = 0;
	int N_Ca     = 0;
	int N_F      = 0;
	int N_form   = 0;   // formula units
	BoxOrtho box;
};

static CaF2System build_caf2(int nx, int ny, int nz, double a = 5.4620) {
	CaF2System sys;

	// Fractional coordinates in conventional cell
	static const double ca_frac[4][3] = {
		{0.00, 0.00, 0.00},
		{0.00, 0.50, 0.50},
		{0.50, 0.00, 0.50},
		{0.50, 0.50, 0.00}
	};
	static const double f_frac[8][3] = {
		{0.25, 0.25, 0.25},
		{0.25, 0.75, 0.75},
		{0.75, 0.25, 0.75},
		{0.75, 0.75, 0.25},
		{0.75, 0.75, 0.75},
		{0.75, 0.25, 0.25},
		{0.25, 0.75, 0.25},
		{0.25, 0.25, 0.75}
	};

	for (int ix = 0; ix < nx; ++ix)
	for (int iy = 0; iy < ny; ++iy)
	for (int iz = 0; iz < nz; ++iz) {
		for (const auto& b : ca_frac) {
			sys.coords.push_back((ix + b[0]) * a);
			sys.coords.push_back((iy + b[1]) * a);
			sys.coords.push_back((iz + b[2]) * a);
			sys.charges.push_back(+2.0);
			++sys.N_Ca;
		}
		for (const auto& b : f_frac) {
			sys.coords.push_back((ix + b[0]) * a);
			sys.coords.push_back((iy + b[1]) * a);
			sys.coords.push_back((iz + b[2]) * a);
			sys.charges.push_back(-1.0);
			++sys.N_F;
		}
	}

	sys.N      = static_cast<int>(sys.charges.size());
	sys.N_form = nx * ny * nz * 4;   // 4 formula units per conventional cell
	sys.box    = BoxOrtho(nx * a, ny * a, nz * a);
	return sys;
}

// ============================================================================
// Run one validation pass and print a results row
// Returns error % for the given parameter set.
// ============================================================================

static double run_ewald(const CaF2System& sys,
						 double alpha, double rcut, int kmax,
						 double E_mad_per_form,
						 bool print_row = true)
{
	EwaldParams ep;
	ep.alpha     = alpha;
	ep.rcut_real = rcut;
	ep.kmax      = kmax;
	ep.coulomb_k = 332.0637133;

	EwaldSum ewald(ep);
	std::vector<double> forces(sys.coords.size(), 0.0);
	double E_total = ewald.evaluate(sys.coords, sys.charges, sys.box, forces);

	double E_per_form  = E_total / sys.N_form;
	double err_kcal    = E_per_form - E_mad_per_form;
	double err_pct     = 100.0 * std::abs(err_kcal) / std::abs(E_mad_per_form);

	if (print_row) {
		std::printf("║ %.2f   ║ %4.1f Å      ║ %4d ║ %14.4f ║ %11.3f %% ║\n",
					alpha, rcut, kmax, E_per_form, err_pct);
	}
	return err_pct;
}

// ============================================================================
// Main
// ============================================================================

int main() {
	constexpr double a     = 5.4620;          // Å
	constexpr double Ck    = 332.0637133;      // kcal·Å/(mol·e²)
	constexpr double A_mad = 2.51939;          // Madelung constant, fluorite
	constexpr double z_ca  = +2.0;
	constexpr double z_f   = -1.0;

	// Nearest Ca-F distance in fluorite: r0 = (√3/4) × a
	const double r0 = (std::sqrt(3.0) / 4.0) * a;

	// Madelung energy per formula unit
	// E = −A × |z+ × z−| × Ck / r0
	// |z+ × z−| = 2 × 1 = 2
	const double E_mad_per_form = -A_mad * std::abs(z_ca * z_f) * Ck / r0;

	// Build 4×4×4 CaF2 supercell
	const int nx = 4, ny = 4, nz = 4;
	auto sys = build_caf2(nx, ny, nz, a);

	// ─── Basic sanity assertions ────────────────────────────────────────────
	assert(sys.N_Ca == 256   && "Expected 256 Ca ions");
	assert(sys.N_F  == 512   && "Expected 512 F ions");
	assert(sys.N    == 768   && "Expected 768 total ions");
	assert(sys.N_form == 256 && "Expected 256 formula units");

	// Neutral cell
	double Q_total = 0.0;
	for (double q : sys.charges) Q_total += q;
	assert(std::abs(Q_total) < 1e-12 && "Cell must be electrically neutral");

	// ─── Header ─────────────────────────────────────────────────────────────
	std::printf("\n");
	std::printf("╔══════════════════════════════════════════════════════════════╗\n");
	std::printf("║   CaF2 Fluorite Ewald Empirical Validation                  ║\n");
	std::printf("║   include/box/pbc.hpp  |  Group 28  |  beta-8 gate          ║\n");
	std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

	std::printf("Reference:\n");
	std::printf("  Structure               = Fluorite CaF2 (Fm-3m, #225)\n");
	std::printf("  Lattice constant a      = %.4f Å\n", a);
	std::printf("  r0 nearest Ca-F         = %.4f Å  [= (√3/4) × a]\n", r0);
	std::printf("  Madelung constant A     = %.5f\n", A_mad);
	std::printf("  Charges                 = Ca +2, F -1\n");
	std::printf("  E_Madelung per formula  = %.4f kcal/mol\n\n", E_mad_per_form);

	std::printf("System: %d×%d×%d CaF2 conventional supercell\n", nx, ny, nz);
	std::printf("  Ca ions     = %d\n", sys.N_Ca);
	std::printf("  F  ions     = %d\n", sys.N_F);
	std::printf("  Total ions  = %d\n", sys.N);
	std::printf("  Formula units = %d\n", sys.N_form);
	std::printf("  Box = %.4f × %.4f × %.4f Å\n", sys.box.L.x, sys.box.L.y, sys.box.L.z);
	std::printf("  Total charge = %.1e (neutral: OK)\n\n", Q_total);

	// ─── Parameter convergence matrix ───────────────────────────────────────
	std::printf("╔════════╦════════════╦══════╦════════════════╦═══════════════╗\n");
	std::printf("║ alpha  ║ rcut_real  ║ kmax ║ E per formula  ║ error %%       ║\n");
	std::printf("║ Å⁻¹   ║            ║      ║ kcal/mol       ║               ║\n");
	std::printf("╠════════╬════════════╬══════╬════════════════╬═══════════════╣\n");

	struct Params { double alpha; double rcut; int kmax; };
	Params matrix[] = {
		{0.25, 8.0,  6},
		{0.30, 8.0,  8},
		{0.35, 8.0, 10},
		{0.40, 7.0, 10},
	};

	double best_err = 1e10;
	for (const auto& p : matrix) {
		double err = run_ewald(sys, p.alpha, p.rcut, p.kmax, E_mad_per_form, true);
		if (err < best_err) best_err = err;
	}

	std::printf("╚════════╩════════════╩══════╩════════════════╩═══════════════╝\n\n");

	// ─── Primary validation — use the canonical β=0.30 parameters ───────────
	EwaldParams ep;
	ep.alpha     = 0.30;
	ep.rcut_real = 8.0;
	ep.kmax      = 8;
	ep.coulomb_k = Ck;

	EwaldSum ewald(ep);
	std::vector<double> forces(sys.coords.size(), 0.0);
	double E_total    = ewald.evaluate(sys.coords, sys.charges, sys.box, forces);
	double E_per_form = E_total / sys.N_form;
	double err_kcal   = E_per_form - E_mad_per_form;
	double err_pct    = 100.0 * std::abs(err_kcal) / std::abs(E_mad_per_form);

	// Force balance
	double fx = 0, fy = 0, fz = 0;
	for (int i = 0; i < sys.N; ++i) {
		fx += forces[3*i+0];
		fy += forces[3*i+1];
		fz += forces[3*i+2];
	}
	double fnet = std::sqrt(fx*fx + fy*fy + fz*fz);

	// ─── Validation table ────────────────────────────────────────────────────
	std::printf("╔══════════════════════════════════════════════════════════════╗\n");
	std::printf("║   EMPIRICAL VALIDATION TABLE  (alpha=0.30, rcut=8, kmax=8)  ║\n");
	std::printf("╠═══════════════════════════════╦═══════════════╦══════════════╣\n");
	std::printf("║  Quantity                     ║  Value        ║  Units       ║\n");
	std::printf("╠═══════════════════════════════╬═══════════════╬══════════════╣\n");
	std::printf("║  E_Ewald total                ║ %13.4f ║  kcal/mol    ║\n", E_total);
	std::printf("║  N formula units              ║ %13d ║  —           ║\n", sys.N_form);
	std::printf("║  E_Ewald per formula          ║ %13.4f ║  kcal/mol    ║\n", E_per_form);
	std::printf("║  E_Madelung per formula       ║ %13.4f ║  kcal/mol    ║\n", E_mad_per_form);
	std::printf("║  Absolute error               ║ %13.4f ║  kcal/mol    ║\n", err_kcal);
	std::printf("║  Error %%                      ║ %12.3f%% ║              ║\n", err_pct);
	std::printf("║  |F_net|                      ║ %13.2e ║  kcal/mol/Å  ║\n", fnet);
	std::printf("╠═══════════════════════════════╩═══════════════╩══════════════╣\n");

	const bool gate_pass     = err_pct < 1.00;
	const bool strong_pass   = err_pct < 0.25;
	const bool excellent     = err_pct < 0.10;
	const bool fbalance_pass = fnet < 1e-4;

	const char* grade =
		excellent   ? "EXCELLENT (< 0.10%)" :
		strong_pass ? "STRONG PASS (< 0.25%)" :
		gate_pass   ? "GATE PASS (< 1.00%)" : "FAIL";

	std::printf("║  E grade:  %-48s║\n", grade);
	std::printf("║  F-balance: %-47s║\n", fbalance_pass ? "PASS (|F_net| < 1e-4)" : "FAIL");
	std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

	// ─── Assertions (hard gate) ──────────────────────────────────────────────
	// Sanity counts
	assert(sys.N_Ca == 256);
	assert(sys.N_F  == 512);
	assert(sys.N    == 768);
	assert(sys.N_form == 256);

	// Gate pass
	if (!gate_pass) {
		std::fprintf(stderr, "ASSERTION FAILED: CaF2 Ewald error %.3f%% exceeds 1.00%% gate\n", err_pct);
		return 1;
	}

	// Best across convergence matrix
	if (best_err >= 1.0) {
		std::fprintf(stderr, "ASSERTION FAILED: best convergence-matrix error %.3f%% exceeds 1.00%%\n", best_err);
		return 1;
	}

	std::printf("All assertions passed.\n");
	return 0;
}
