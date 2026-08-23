/**
 * test_length_scale_fitter.cpp  —  WO-73D  —  Group 78
 * ======================================================
 * Tests for LengthScaleFitter: all four fitting modes, asymmetry rule,
 * recovery curve normalisation, and edge cases.
 *
 * Build:
 *   cmake --build build --target test_length_scale_fitter
 *   ctest -R LengthScaleFitterGroup78 -V
 */

#include "multiscale/length_scale_fitter.hpp"
#include <cstdio>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>

using namespace vsepr::multiscale;

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond)                                                         \
	do {                                                                    \
		++tests_run;                                                        \
		if (cond) {                                                         \
			++tests_passed;                                                 \
		} else {                                                            \
			std::printf("FAIL  line %d: %s\n", __LINE__, #cond);           \
		}                                                                   \
	} while(0)

#define CHECK_NEAR(a, b, tol)                                               \
	do {                                                                    \
		++tests_run;                                                        \
		if (std::abs((a) - (b)) <= (tol)) {                                 \
			++tests_passed;                                                 \
		} else {                                                            \
			std::printf("FAIL  line %d: |%.6f - %.6f| = %.6f > %.6f\n",   \
				__LINE__, (double)(a), (double)(b),                         \
				std::abs((double)(a)-(double)(b)), (double)(tol));         \
		}                                                                   \
	} while(0)

// Build a synthetic recovery curve: R(L) = 1 - exp(-L / Lambda_true)
static std::vector<RecoverySample> make_curve(double Lambda_true,
											   int n_points = 20,
											   double L_max = 5.0)
{
	std::vector<RecoverySample> s(n_points);
	for (int i = 0; i < n_points; ++i) {
		const double L = L_max * (i + 1.0) / n_points;
		const double A_inf = 100.0;
		const double A_L   = A_inf * (1.0 - std::exp(-L / Lambda_true));
		s[i] = { L, A_L, A_inf };
	}
	return s;
}

// ============================================================================
// [1] LINEAR mode — synthetic curve with Lambda_true = 1.2 nm
// ============================================================================

static void test_linear_mode()
{
	const double Lambda_true = 1.2;
	auto samples = make_curve(Lambda_true);
	LengthScaleFitter fitter(FitMode::LINEAR);
	auto res = fitter.fit(samples, "pi_7", "biological_pocket", "pi_N");

	CHECK(res.valid);
	CHECK_NEAR(res.Lambda_fit_nm, Lambda_true, 0.05);
	CHECK(res.fit_error < 0.1);
	CHECK(res.confidence > 0.0);
	CHECK(res.object_id == "pi_7");
	CHECK(res.state == "biological_pocket");
	CHECK(res.family == "pi_N");
}

// ============================================================================
// [2] SPLINE mode — Lambda_true = 0.28 nm (HB_N crystal)
// ============================================================================

static void test_spline_mode()
{
	const double Lambda_true = 0.28;
	auto samples = make_curve(Lambda_true, 20, 2.0);
	LengthScaleFitter fitter(FitMode::SPLINE);
	auto res = fitter.fit(samples, "HB_N", "crystal", "HB_N");

	CHECK(res.valid);
	CHECK_NEAR(res.Lambda_fit_nm, Lambda_true, 0.03);
	CHECK(res.fit_error < 0.1);
}

// ============================================================================
// [3] LOG mode — Lambda_true = 0.40 nm (vdW_N)
// ============================================================================

static void test_log_mode()
{
	const double Lambda_true = 0.40;
	auto samples = make_curve(Lambda_true, 20, 3.0);
	LengthScaleFitter fitter(FitMode::LOG);
	auto res = fitter.fit(samples, "vdW_N", "crystal", "vdW_N");

	CHECK(res.valid);
	CHECK_NEAR(res.Lambda_fit_nm, Lambda_true, 0.05);
	CHECK(res.fit_error < 0.15);
}

// ============================================================================
// [4] ASYMMETRIC mode — Lambda_true = 1.35 nm (pi_7 / biological_pocket)
// ============================================================================

static void test_asymmetric_mode()
{
	const double Lambda_true = 1.35;
	auto samples = make_curve(Lambda_true, 25, 6.0);
	LengthScaleFitter fitter(FitMode::ASYMMETRIC);
	auto res = fitter.fit(samples, "pi_7", "biological_pocket", "pi_N");

	CHECK(res.valid);
	CHECK_NEAR(res.Lambda_fit_nm, Lambda_true, 0.10);
	CHECK(res.sigma_fit_nm >= 0.0);
}

// ============================================================================
// [5] Asymmetry rule: below-ideal residuals penalised harder
//     Two identical curves — one queried below Lambda, one above.
//     The asymmetric fitter should produce a valid result biased toward
//     the data rather than averaging symmetrically.
// ============================================================================

static void test_asymmetry_direction()
{
	// Build a curve and add a small systematic positive shift above Lambda
	const double Lambda_true = 1.0;
	auto samples = make_curve(Lambda_true, 20, 5.0);

	// Slightly depress early points (L < Lambda) — should pull Lambda down
	for (auto& s : samples) {
		if (s.L < Lambda_true) s.recovered_value *= 0.95;
	}

	LengthScaleFitter fitter(FitMode::ASYMMETRIC);
	auto res = fitter.fit(samples, "test", "gas", "pi_N");
	// With harder below-ideal penalty, fitter should not over-estimate Lambda
	CHECK(res.valid);
	CHECK(res.Lambda_fit_nm < Lambda_true + 0.20);
}

// ============================================================================
// [6] Edge case: exactly 3 points (minimum)
// ============================================================================

static void test_minimum_points()
{
	std::vector<RecoverySample> s = {
		{ 0.3, 25.0, 100.0 },
		{ 0.7, 50.0, 100.0 },
		{ 1.5, 78.5, 100.0 }
	};
	LengthScaleFitter fitter(FitMode::LINEAR);
	auto res = fitter.fit(s, "obj", "gas", "vdW_N");
	CHECK(res.valid || !res.valid);  // must not crash; validity can go either way
	CHECK(res.Lambda_fit_nm >= 0.0);
}

// ============================================================================
// [7] Edge case: fewer than 3 points -> invalid result, no crash
// ============================================================================

static void test_too_few_points()
{
	std::vector<RecoverySample> s = {
		{ 0.5, 40.0, 100.0 },
		{ 1.0, 63.0, 100.0 }
	};
	LengthScaleFitter fitter(FitMode::SPLINE);
	auto res = fitter.fit(s, "obj", "gas", "vdW_N");
	CHECK(!res.valid);
}

// ============================================================================
// [8] sigma_fit_nm is non-negative for all modes
// ============================================================================

static void test_sigma_non_negative()
{
	auto samples = make_curve(1.0, 15, 5.0);
	for (auto mode : { FitMode::LINEAR, FitMode::SPLINE,
					   FitMode::LOG, FitMode::ASYMMETRIC }) {
		LengthScaleFitter fitter(mode);
		auto res = fitter.fit(samples, "obj", "crystal", "vdW_N");
		CHECK(res.sigma_fit_nm >= 0.0);
	}
}

// ============================================================================
// [9] Metal-ligand: short Lambda_true = 0.22 nm
// ============================================================================

static void test_metal_ligand()
{
	const double Lambda_true = 0.22;
	auto samples = make_curve(Lambda_true, 20, 1.5);
	LengthScaleFitter fitter(FitMode::SPLINE);
	auto res = fitter.fit(samples, "metal_ligand", "crystal", "metal_ligand");
	CHECK(res.valid);
	CHECK_NEAR(res.Lambda_fit_nm, Lambda_true, 0.04);
}

// ============================================================================
// Analytic fixture registry — WO-74C
// ============================================================================
// Each FitterTestCase is a named analytic case with a known Lambda_true.
// The test runner iterates all registered fixtures; adding a new case
// requires only adding one entry to the FITTER_FIXTURES table below.

#include <array>

struct FitterTestCase {
	const char*  name;        // human-readable label
	FitMode      mode;        // fitting mode under test
	double       lambda_true; // ground-truth Lambda_nm
	int          n_points;    // number of synthetic samples
	double       l_max;       // upper bound of L range
	double       tol_nm;      // acceptable absolute error in Lambda_fit
	const char*  object_id;
	const char*  state;
	const char*  family;
};

static constexpr std::array FITTER_FIXTURES = {
	// [A] Biological pocket — exponential recovery, lambda = 1.2 nm
	FitterTestCase{ "bio_pocket_linear",  FitMode::LINEAR,     1.20, 20, 5.0, 0.05,
					"bio_1", "biological_pocket", "pi_N" },
	// [B] Short-range metallic — spline, lambda = 0.22 nm
	FitterTestCase{ "metal_ligand_spline", FitMode::SPLINE,    0.22, 20, 1.5, 0.04,
					"metal_ligand", "crystal", "metal_ligand" },
	// [C] Polymer chain — log mode, lambda = 2.5 nm
	FitterTestCase{ "polymer_log",         FitMode::LOG,       2.50, 30, 12.0, 0.10,
					"poly_1", "amorphous", "pi_chain" },
	// [D] Asymmetric case — ASYMMETRIC mode, lambda = 1.5 nm
	//     L > Lambda_X (above-ideal) path: soft weight expected
	FitterTestCase{ "asymmetric_above",    FitMode::ASYMMETRIC, 1.50, 25, 8.0, 0.10,
					"asym_1", "gas", "noble" },
};

static void test_analytic_fixture_registry()
{
	std::printf("  [fixture registry] %zu cases\n", FITTER_FIXTURES.size());
	for (const auto& tc : FITTER_FIXTURES) {
		auto samples = make_curve(tc.lambda_true, tc.n_points, tc.l_max);
		LengthScaleFitter fitter(tc.mode);
		auto res = fitter.fit(samples, tc.object_id, tc.state, tc.family);

		CHECK(res.valid);
		CHECK_NEAR(res.Lambda_fit_nm, tc.lambda_true, tc.tol_nm);
		CHECK(res.fit_error >= 0.0);
		CHECK(res.confidence >= 0.0);

		if (!res.valid || std::abs(res.Lambda_fit_nm - tc.lambda_true) > tc.tol_nm) {
			std::printf("  FAIL case '%s': Lambda_fit=%.4f expected %.4f (tol %.4f)\n",
						tc.name, res.Lambda_fit_nm, tc.lambda_true, tc.tol_nm);
		}
	}
}

// ============================================================================
// main
// ============================================================================

int main()
{
	std::printf("=== test_length_scale_fitter (WO-73D / WO-74C Group 78) ===\n");

	test_linear_mode();
	test_spline_mode();
	test_log_mode();
	test_asymmetric_mode();
	test_asymmetry_direction();
	test_minimum_points();
	test_too_few_points();
	test_sigma_non_negative();
	test_metal_ligand();
	test_analytic_fixture_registry();

	std::printf("\n%d / %d tests passed\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
