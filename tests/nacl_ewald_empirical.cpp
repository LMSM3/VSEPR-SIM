/**
 * nacl_ewald_empirical.cpp — Empirical validation: Ewald energy for NaCl crystal
 * ================================================================================
 *
 * Computes the Madelung-derived Coulomb energy for an NaCl unit cell supercell
 * and compares to the known analytic Madelung constant.
 *
 * Reference values:
 *   NaCl Madelung constant (A):  1.7475645946...  (rock-salt, exact)
 *   Lattice constant (a):         5.6402 Å         (experimental, 298 K)
 *   Nearest-neighbour distance:   r0 = a/2 = 2.8201 Å
 *   Madelung energy per ion pair: E_M = -A * k * e² / r0
 *                                 = -1.7476 * 332.0637 / 2.8201
 *                                 ≈ -205.8 kcal/mol per ion pair
 *
 * This test:
 *   1. Builds a 2×2×2 NaCl supercell (16 Na + 16 Cl = 32 ions)
 *   2. Runs EwaldSum with calibrated params
 *   3. Reports E_Ewald / N_pairs vs. Madelung analytic value
 *   4. Checks within 1% tolerance
 *
 * Parameters to hand-check:
 *   Printed as "EMPIRICAL VALIDATION TABLE" below for user review.
 *
 * WO-56C / beta-8 validation anchor  |  include/box/pbc.hpp move verification
 */

#include "box/pbc.hpp"
#include "pot/ewald_sum.hpp"

#include <cmath>
#include <cstdio>
#include <vector>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace vsepr;

// ============================================================================
// NaCl rock-salt structure builder
// ============================================================================

struct NaClSystem {
	std::vector<double> coords;   // flat [x0,y0,z0, ...]
	std::vector<double> charges;  // +1 (Na) or -1 (Cl)
	int N = 0;
	BoxOrtho box;
};

// Build n×n×n NaCl supercell.
// Conventional unit cell: a=5.6402 Å, 4 Na + 4 Cl (8 atoms)
// Rock-salt: Na at (0,0,0)-type, Cl at (0.5,0,0)-type fractional coords
static NaClSystem build_nacl(int nx, int ny, int nz, double a = 5.6402) {
	NaClSystem sys;
	// Rock-salt fractional basis (8 atoms per conventional cell)
	// Na: (0,0,0),(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5)
	// Cl: (0.5,0,0),(0,0.5,0),(0,0,0.5),(0.5,0.5,0.5)
	const double na_basis[4][3] = {{0,0,0},{0.5,0.5,0},{0.5,0,0.5},{0,0.5,0.5}};
	const double cl_basis[4][3] = {{0.5,0,0},{0,0.5,0},{0,0,0.5},{0.5,0.5,0.5}};

	for (int ix = 0; ix < nx; ++ix)
	for (int iy = 0; iy < ny; ++iy)
	for (int iz = 0; iz < nz; ++iz) {
		for (auto& b : na_basis) {
			sys.coords.push_back((ix + b[0]) * a);
			sys.coords.push_back((iy + b[1]) * a);
			sys.coords.push_back((iz + b[2]) * a);
			sys.charges.push_back(+1.0);
		}
		for (auto& b : cl_basis) {
			sys.coords.push_back((ix + b[0]) * a);
			sys.coords.push_back((iy + b[1]) * a);
			sys.coords.push_back((iz + b[2]) * a);
			sys.charges.push_back(-1.0);
		}
	}
	sys.N = static_cast<int>(sys.charges.size());
	double Lx = nx * a, Ly = ny * a, Lz = nz * a;
	sys.box = BoxOrtho(Lx, Ly, Lz);
	return sys;
}

// ============================================================================
// Main
// ============================================================================

int main() {
	const double a      = 5.6402;   // Å — experimental NaCl lattice constant
	const double r0     = a / 2.0;  // nearest-neighbour distance = 2.8201 Å
	const double A_mad  = 1.7475645946; // Madelung constant (rock-salt)
	const double Ck     = 332.0637;     // kcal·Å/(mol·e²)

	// Analytic Madelung energy per ion pair (kcal/mol)
	const double E_mad_per_pair = -A_mad * Ck / r0;

	std::printf("\n");
	std::printf("╔══════════════════════════════════════════════════════════════╗\n");
	std::printf("║   NaCl Ewald Empirical Validation — include/box/pbc.hpp     ║\n");
	std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

	std::printf("Reference (Madelung, analytic):\n");
	std::printf("  Lattice constant a         = %.4f Å\n", a);
	std::printf("  r0 (nearest-neighbour)     = %.4f Å\n", r0);
	std::printf("  Madelung constant A        = %.10f\n", A_mad);
	std::printf("  E_Madelung per ion pair    = %.4f kcal/mol\n\n", E_mad_per_pair);

	// Build 2×2×2 supercell (32 ions = 16 pairs)
	int nx = 2, ny = 2, nz = 2;
	auto sys = build_nacl(nx, ny, nz, a);
	int N_pairs = sys.N / 2;

	std::printf("System: %d×%d×%d NaCl supercell\n", nx, ny, nz);
	std::printf("  Ions total: %d  (Na: %d, Cl: %d)\n", sys.N, sys.N/2, sys.N/2);
	std::printf("  Box: %.4f × %.4f × %.4f Å\n\n",
				sys.box.L.x, sys.box.L.y, sys.box.L.z);

	// Ewald parameters calibrated for 2×2×2 NaCl (L=11.28 Å)
	EwaldParams ep;
	ep.alpha     = 0.35;    // Splitting — α ≈ 0.3–0.35 is standard for this box size
	ep.rcut_real = 5.0;     // Real-space cutoff < L/2 = 5.64 Å
	ep.kmax      = 6;       // k-space convergence (higher = more accurate, slower)
	ep.coulomb_k = Ck;

	std::printf("Ewald parameters:\n");
	std::printf("  alpha     = %.3f Å⁻¹\n", ep.alpha);
	std::printf("  rcut_real = %.3f Å\n", ep.rcut_real);
	std::printf("  kmax      = %d\n\n", ep.kmax);

	EwaldSum ewald(ep);
	std::vector<double> forces(sys.coords.size(), 0.0);
	double E_total = ewald.evaluate(sys.coords, sys.charges, sys.box, forces);
	double E_per_pair = E_total / N_pairs;

	double err_pct = 100.0 * std::abs(E_per_pair - E_mad_per_pair) / std::abs(E_mad_per_pair);

	std::printf("╔══════════════════════════════════════════════════════════════╗\n");
	std::printf("║   EMPIRICAL VALIDATION TABLE                                 ║\n");
	std::printf("╠══════════════════════════════════════════════════════════════╣\n");
	std::printf("║  Quantity                 │  Value           │  Units        ║\n");
	std::printf("╠══════════════════════════════════════════════════════════════╣\n");
	std::printf("║  E_Ewald total            │ %16.4f  │  kcal/mol     ║\n", E_total);
	std::printf("║  N ion pairs              │ %16d  │  —            ║\n", N_pairs);
	std::printf("║  E_Ewald per pair         │ %16.4f  │  kcal/mol     ║\n", E_per_pair);
	std::printf("║  E_Madelung per pair      │ %16.4f  │  kcal/mol     ║\n", E_mad_per_pair);
	std::printf("║  Absolute error           │ %16.4f  │  kcal/mol     ║\n", E_per_pair - E_mad_per_pair);
	std::printf("║  Error %%                  │ %15.3f%%  │               ║\n", err_pct);
	std::printf("╠══════════════════════════════════════════════════════════════╣\n");

	bool pass = err_pct < 1.0;
	if (pass)
		std::printf("║  RESULT: ✓ PASS — Ewald energy within 1%% of Madelung       ║\n");
	else
		std::printf("║  RESULT: ✗ FAIL — Error exceeds 1%% tolerance               ║\n");
	std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

	// Force balance check (net force on system should be ~0)
	double fx_tot = 0, fy_tot = 0, fz_tot = 0;
	for (int i = 0; i < sys.N; ++i) {
		fx_tot += forces[3*i+0];
		fy_tot += forces[3*i+1];
		fz_tot += forces[3*i+2];
	}
	double fnet = std::sqrt(fx_tot*fx_tot + fy_tot*fy_tot + fz_tot*fz_tot);
	std::printf("Force balance (net force on system):\n");
	std::printf("  |F_net| = %.2e kcal/mol/Å  (should be ~0)\n\n", fnet);
	bool fpass = fnet < 1e-6;
	std::printf("  Force balance: %s\n\n", fpass ? "✓ PASS" : "✗ FAIL");

	return (pass && fpass) ? 0 : 1;
}
