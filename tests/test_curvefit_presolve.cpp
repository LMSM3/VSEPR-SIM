// =============================================================================
// test_curvefit_presolve.cpp  —  WO-XSUITE-02B acceptance tests  EIG-7..8
// =============================================================================
// EIG-7  curvefit module consumes eigenmine output (via sample construction)
// EIG-8  curvefit exports coefficients and residuals
// =============================================================================

#include "vsim/xsuite.hpp"
#include "vsim/curvefit_presolve.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void check(bool cond, const char* msg) {
	if (!cond) { std::cerr << "[FAIL] " << msg << "\n"; std::exit(1); }
	std::cout << "[OK]   " << msg << "\n";
}

// ---------------------------------------------------------------------------
// Build synthetic samples: y = z0 + 0.5*z1  (linear, should fit near-perfectly)
// ---------------------------------------------------------------------------

static std::vector<vsepr::presolve::CurvefitSample> make_samples(int n) {
	std::vector<vsepr::presolve::CurvefitSample> s;
	s.reserve(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i) {
		double z0 = static_cast<double>(i) * 0.1;
		double z1 = static_cast<double>(i % 7) * 0.2;
		vsepr::presolve::CurvefitSample samp;
		samp.z = {z0, z1, 1.0};                    // 3 features
		samp.y = {z0 + 0.5 * z1, z0 * z0 + 0.1};  // 2 targets
		s.push_back(std::move(samp));
	}
	return s;
}

// ---------------------------------------------------------------------------
// EIG-7: curvefit module accepts samples and returns ok
// ---------------------------------------------------------------------------

static void test_EIG7_consumes_input() {
	vsepr::presolve::CurvefitConfig cfg;
	cfg.model          = "poly_log_eigen_hybrid";
	cfg.max_order      = 2;
	cfg.regularization = 1.0e-4;
	cfg.train_fraction = 0.80;
	cfg.val_fraction   = 0.20;

	auto samples = make_samples(50);
	auto res = vsepr::presolve::curvefit_run(cfg, samples, std::cout);

	check(res.ok, "EIG-7: curvefit_run returns ok");
	check(res.n_train > 0, "EIG-7: n_train > 0");
	check(res.n_val   > 0, "EIG-7: n_val > 0");
	check(res.train_rmse >= 0.0, "EIG-7: train_rmse is non-negative");
	check(res.val_score >= 0.0 && res.val_score <= 1.0,
		  "EIG-7: val_score in [0,1]");
	// Linear relationship should yield a reasonable score
	check(res.val_score > 0.5, "EIG-7: val_score > 0.5 for linear test data");
}

// ---------------------------------------------------------------------------
// EIG-8: curvefit exports coefficients and residuals (validation report)
// ---------------------------------------------------------------------------

static void test_EIG8_exports() {
	fs::path model_path  = fs::temp_directory_path() / "test_eig8_model.json";
	fs::path coeff_path  = fs::temp_directory_path() / "test_eig8_coeffs.tsv";
	fs::path report_path = fs::temp_directory_path() / "test_eig8_report.md";

	// Clean up any prior run
	for (const auto& p : {model_path, coeff_path, report_path})
		if (fs::exists(p)) fs::remove(p);

	vsepr::presolve::CurvefitConfig cfg;
	cfg.model               = "poly_log_eigen_hybrid";
	cfg.max_order           = 1;
	cfg.regularization      = 1.0e-3;
	cfg.train_fraction      = 0.75;
	cfg.val_fraction        = 0.25;
	cfg.write_model         = model_path.string();
	cfg.write_coefficients  = coeff_path.string();
	cfg.write_report        = report_path.string();

	auto samples = make_samples(40);
	auto res = vsepr::presolve::curvefit_run(cfg, samples, std::cout);

	check(res.ok, "EIG-8: curvefit_run ok");
	check(fs::exists(model_path),  "EIG-8: model JSON written");
	check(fs::exists(coeff_path),  "EIG-8: coefficients TSV written");
	check(fs::exists(report_path), "EIG-8: validation report written");

	// Coefficients TSV must have header + at least one data row
	{
		std::ifstream f(coeff_path);
		std::string header, row;
		std::getline(f, header);
		bool has_data = static_cast<bool>(std::getline(f, row));
		check(header.find("target") != std::string::npos, "EIG-8: TSV has header");
		check(has_data, "EIG-8: TSV has data rows");
	}
}

// ---------------------------------------------------------------------------
// EIG-7b: curvefit block parses from .X file
// ---------------------------------------------------------------------------

static void test_EIG7b_xparse() {
	std::string body = R"(
[xsuite]
name = test_curvefit
version = 5.2
mode = presolve_suite
created_by = test

[run]
entry = dummy.vsim
mode = presolve
num_steps = 1
dt = 1e-15

[files]
snapshot = dummy.xyz

[curvefit]
enabled             = true
source              = archive/eigenmine_modes.ndjson
target              = archive/presolve_batch.ndjson
model               = poly_log_eigen_hybrid
max_order           = 3
regularization      = 1.0e-4
train_fraction      = 0.80
validation_fraction = 0.20
write_model         = archive/curvefit_model.json
write_coefficients  = reports/curvefit_coefficients.tsv
write_report        = reports/curvefit_validation.md
)";
	auto tmp = fs::temp_directory_path() / "test_curvefit.X";
	{ std::ofstream f(tmp); f << body; }
	auto res = vsepr::xsuite::xsuite_parse(tmp.string());
	check(res.ok, "EIG-7b: parse ok");
	check(res.suite.curvefit_enabled, "EIG-7b: curvefit.enabled");
	check(res.suite.curvefit_model == "poly_log_eigen_hybrid", "EIG-7b: model name");
	check(res.suite.curvefit_max_order == 3, "EIG-7b: max_order");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== EIG-7..8 tests ===\n";
	test_EIG7b_xparse();
	test_EIG7_consumes_input();
	test_EIG8_exports();
	std::cout << "All EIG-7..8 tests passed.\n";
	return 0;
}
