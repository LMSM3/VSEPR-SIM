/**
 * v4_scores_test.cpp — V4.0.4.03 Score Validation
 * ═══════════════════════════════════════════════
 * Tests all three V4 meta-scores (gamma, data_quality, compactness)
 * plus the Pearson correlation matrix against the Day #47 reference
 * dataset of 12 elements.
 *
 * Build (VSEPR-SIM 4.0.4.03 / GCC 14+):
 *   g++ -std=c++23 -O2 -ftrivial-auto-var-init=pattern \
 *       v4_scores_test.cpp -o v4_scores_test
 *
 * C++26 features used:
 *   - Erroneous-behaviour trapping (-ftrivial-auto-var-init=pattern)
 *   - Contract emulation (V4_CONTRACT_PRE/POST)
 *   - Structured bindings throughout
 *   - Trailing return types
 */

#include "v4/formation_record.hpp"
#include "v4/gamma_score.hpp"
#include "v4/data_quality.hpp"
#include "v4/compactness.hpp"
#include "v4/correlation_matrix.hpp"
#include "v4/corr_heatmap.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ── Test Infrastructure ──────────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;
static int g_total = 0;

static void check(bool cond, const char* desc) {
    ++g_total;
    if (cond) {
        ++g_pass;
        std::cout << "  [PASS] " << desc << "\n";
    } else {
        ++g_fail;
        std::cout << "  [FAIL] " << desc << "\n";
    }
}

static void check_range(double val, double lo, double hi, const char* desc) {
    ++g_total;
    bool ok = std::isfinite(val) && val >= lo && val <= hi;
    if (ok) {
        ++g_pass;
        std::cout << "  [PASS] " << desc
                  << "  (" << std::fixed << std::setprecision(4) << val << ")\n";
    } else {
        ++g_fail;
        std::cout << "  [FAIL] " << desc
                  << "  (got " << val << ", expected [" << lo << ", " << hi << "])\n";
    }
}

// ── Phase 1: Formation Record Validation ─────────────────────────────────────

static void test_formation_record() {
    std::cout << "\n══ Phase 1: Formation Record ══════════════════════════════\n";

    auto dataset = v4::reference_dataset();
    check(dataset.size() == 12, "Reference dataset has 12 elements");

    // Verify Gold
    const auto& au = dataset[0];
    check(au.symbol == "Au",                         "Au symbol");
    check(au.name == "Gold",                         "Au name");
    check(au.structure == v4::LatticeClass::FCC,     "Au structure = FCC");
    check(au.n_beads == 64,                          "Au n_beads = 64");
    check(au.converged == true,                      "Au converged = true");
    check(au.steps == 1698,                          "Au steps = 1698");
    check(au.is_scoreable(),                         "Au is scoreable");

    // Verify Cobalt (unconverged HCP)
    const auto& co = dataset[11];
    check(co.symbol == "Co",                         "Co symbol");
    check(!co.converged,                             "Co converged = false");
    check(co.structure == v4::LatticeClass::HCP,     "Co structure = HCP");
    check(co.steps == 6000,                          "Co steps = 6000 (step limit)");

    // Field count
    int au_pop = au.populated_fields();
    check(au_pop > 15, "Au populated_fields > 15");

    std::cout << "  Au populated: " << au_pop << " / "
              << v4::FormationRecord::FIELD_COUNT << "\n";
}

// ── Phase 2: Gamma Score ─────────────────────────────────────────────────────

static void test_gamma_score() {
    std::cout << "\n══ Phase 2: Gamma Score ═══════════════════════════════════\n";

    // Weight validation
    v4::GammaWeights w{};
    check(w.valid(), "Default gamma weights sum to 1.0");

    auto dataset = v4::reference_dataset();

    // Score all 12 elements
    for (auto& rec : dataset) {
        // Use sigma_eta = 0.01 as default, no reference run
        auto result = v4::score_gamma(rec, v4::NaN, 0.01, rec.macro_rigidity,
                                      0, 4);
        check_range(rec.gamma, 0.0, 1.0,
                    (rec.symbol + " gamma in [0,1]").c_str());
    }

    // Au has higher S_intensity (populated fields) but Ag gets high S_var
    // from near-zero avg_eta (CoV blows up).  This is correct: gamma measures
    // exploration, and Ag's trivial-converge high-CoV case looks "explored"
    // even though it's scientifically empty — the DATA QUALITY score handles
    // that distinction.
    check(dataset[0].gamma < dataset[1].gamma,
          "Au gamma < Ag gamma (Ag S_var dominates from near-zero eta)");

    // Iron (BCC, fully converged, rich fields) should score well
    check(dataset[6].gamma > 0.1,
          "Fe gamma > 0.1 (BCC with rich data)");

    // Sub-score breakdown for Gold
    auto au_result = v4::compute_gamma(dataset[0], v4::NaN, 0.01,
                                       dataset[0].macro_rigidity, 0, 4);
    std::cout << "  Au breakdown:\n"
              << "    S_bead     = " << au_result.components.S_bead << "\n"
              << "    S_scale    = " << au_result.components.S_scale << "\n"
              << "    S_var      = " << au_result.components.S_var << "\n"
              << "    S_intensity= " << au_result.components.S_intensity << "\n"
              << "    S_orbital  = " << au_result.components.S_orbital << "\n"
              << "    gamma      = " << au_result.gamma << "\n";
}

// ── Phase 3: Data Quality Score ──────────────────────────────────────────────

static void test_data_quality() {
    std::cout << "\n══ Phase 3: Data Quality Score ════════════════════════════\n";

    v4::DataQualityWeights w{};
    check(w.valid(), "Default DQ weights valid");

    auto dataset = v4::reference_dataset();

    for (auto& rec : dataset) {
        auto result = v4::score_data_quality(rec);
        check_range(rec.data_quality, 0.0, 1.0,
                    (rec.symbol + " Q_data in [0,1]").c_str());
    }

    // Converged cases should score higher than unconverged
    check(dataset[0].data_quality > dataset[3].data_quality,
          "Au Q > Pt Q (converged vs unconverged)");

    // Gold (converged, rich energy) > Silver (converged but trivial)
    check(dataset[0].data_quality > dataset[1].data_quality,
          "Au Q > Ag Q (rich vs trivial)");

    // Iron (BCC, converged, strong energy) should be high
    check(dataset[6].data_quality > 0.2,
          "Fe Q > 0.2 (converged BCC with energy)");

    // Breakdown for Iron
    auto fe_result = v4::compute_data_quality(dataset[6]);
    std::cout << "  Fe breakdown:\n"
              << "    C_conv         = " << fe_result.components.C_conv << "\n"
              << "    C_energy       = " << fe_result.components.C_energy << "\n"
              << "    C_stability    = " << fe_result.components.C_stability << "\n"
              << "    C_consistency  = " << fe_result.components.C_consistency << "\n"
              << "    C_completeness = " << fe_result.components.C_completeness << "\n"
              << "    P_failure      = " << fe_result.components.P_failure << "\n"
              << "    Q_data         = " << fe_result.Q_data << "\n";
}

// ── Phase 4: Compactness Score ───────────────────────────────────────────────

static void test_compactness() {
    std::cout << "\n══ Phase 4: Compactness Score ═════════════════════════════\n";

    v4::CompactnessWeights w{};
    check(w.valid(), "Default compactness weights valid");

    auto dataset = v4::reference_dataset();

    for (auto& rec : dataset) {
        auto result = v4::score_compactness(rec);
        check_range(rec.compactness, 0.0, 1.0,
                    (rec.symbol + " C_compact in [0,1]").c_str());
    }

    // BCC metals with high avg_rho/avg_C should be more compact
    // Fe: avg_rho=11.8, avg_C=45.3
    // Au: avg_rho=5.2, avg_C=25.6
    check(dataset[6].compactness > dataset[0].compactness,
          "Fe compact > Au compact (higher rho/C)");

    // Silver (all zeros) should be low compactness
    check(dataset[1].compactness < 0.1,
          "Ag compact < 0.1 (trivial/zero fields)");

    // Breakdown for Tungsten
    auto w_result = v4::compute_compactness(dataset[7]);
    std::cout << "  W breakdown:\n"
              << "    rho*      = " << w_result.components.rho_star << "\n"
              << "    eta*      = " << w_result.components.eta_star << "\n"
              << "    R_coord*  = " << w_result.components.R_coord_star << "\n"
              << "    D_macro*  = " << w_result.components.D_macro_star << "\n"
              << "    Phi_void* = " << w_result.components.Phi_void_star << "\n"
              << "    C_compact = " << w_result.C_compact << "\n";
}

// ── Phase 5: Correlation Matrix ──────────────────────────────────────────────

static void test_correlation_matrix() {
    std::cout << "\n══ Phase 5: Correlation Matrix ════════════════════════════\n";

    // Score all records first
    auto dataset = v4::reference_dataset();
    for (auto& rec : dataset) {
        v4::score_gamma(rec, v4::NaN, 0.01, rec.macro_rigidity, 0, 4);
        v4::score_data_quality(rec);
        v4::score_compactness(rec);
    }

    // Compute correlation matrix
    auto mat = v4::compute_correlation_matrix(dataset);
    check(mat.sample_count == 12, "Correlation matrix sample_count = 12");

    // Diagonal should be 1.0
    for (int i = 0; i < v4::CORR_VAR_COUNT; ++i) {
        check(std::abs(mat(i, i) - 1.0) < 1e-10,
              (std::string("Diagonal R(") +
               v4::corr_var_name(static_cast<v4::CorrVar>(i)) +
               "," +
               v4::corr_var_name(static_cast<v4::CorrVar>(i)) +
               ") = 1.0").c_str());
    }

    // Symmetry check
    bool symmetric = true;
    for (int i = 0; i < v4::CORR_VAR_COUNT; ++i) {
        for (int j = i + 1; j < v4::CORR_VAR_COUNT; ++j) {
            double rij = mat(i, j);
            double rji = mat(j, i);
            if (std::isfinite(rij) && std::isfinite(rji)) {
                if (std::abs(rij - rji) > 1e-12) symmetric = false;
            }
        }
    }
    check(symmetric, "Correlation matrix is symmetric");

    // All correlations should be in [-1, 1]
    bool bounded = true;
    for (int i = 0; i < v4::CORR_VAR_COUNT; ++i) {
        for (int j = 0; j < v4::CORR_VAR_COUNT; ++j) {
            double r = mat(i, j);
            if (std::isfinite(r) && (r < -1.001 || r > 1.001))
                bounded = false;
        }
    }
    check(bounded, "All correlations in [-1,1]");

    // Print ASCII heatmap
    std::cout << "\n── ASCII Heatmap ──────────────────────────────────────────\n";
    v4::print_ascii_heatmap(std::cout, mat);

    // Ranked report
    std::cout << "\n";
    v4::print_ranked_report(std::cout, mat);

    // Export CSV
    {
        std::ofstream csv("v4_correlation.csv");
        if (csv.is_open()) {
            v4::export_csv(csv, mat);
            std::cout << "\n  [INFO] CSV exported to v4_correlation.csv\n";
        }
    }

    // Export HTML heatmap
    {
        std::string html = v4::generate_html_heatmap(mat);
        std::ofstream hf("v4_heatmap.html");
        if (hf.is_open()) {
            hf << html;
            std::cout << "  [INFO] HTML heatmap exported to v4_heatmap.html\n";
        }
    }
}

// ── Phase 6: Score Summary Table ─────────────────────────────────────────────

static void test_score_summary() {
    std::cout << "\n══ Phase 6: Score Summary Table ═══════════════════════════\n";

    auto dataset = v4::reference_dataset();
    for (auto& rec : dataset) {
        v4::score_gamma(rec, v4::NaN, 0.01, rec.macro_rigidity, 0, 4);
        v4::score_data_quality(rec);
        v4::score_compactness(rec);
    }

    std::cout << std::setw(4)  << "Sym"
              << std::setw(12) << "Name"
              << std::setw(5)  << "Lat"
              << std::setw(6)  << "Conv"
              << std::setw(7)  << "Steps"
              << std::setw(8)  << "gamma"
              << std::setw(8)  << "Q_data"
              << std::setw(10) << "C_compact"
              << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& rec : dataset) {
        std::cout << std::setw(4)  << rec.symbol
                  << std::setw(12) << rec.name
                  << std::setw(5)  << v4::lattice_name(rec.structure)
                  << std::setw(6)  << (rec.converged ? "Y" : "N")
                  << std::setw(7)  << rec.steps
                  << std::setw(8)  << std::fixed << std::setprecision(4) << rec.gamma
                  << std::setw(8)  << std::fixed << std::setprecision(4) << rec.data_quality
                  << std::setw(10) << std::fixed << std::setprecision(4) << rec.compactness
                  << "\n";
    }
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  VSEPR-SIM Version 4 Beta — Score Validation Suite\n";
    std::cout << "  Development Day #47 | O(xN) Transition Architecture\n";
    std::cout << "  C++26: erroneous-behaviour | contracts | flat matrix\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    test_formation_record();
    test_gamma_score();
    test_data_quality();
    test_compactness();
    test_correlation_matrix();
    test_score_summary();

    std::cout << "\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "  RESULTS: " << g_pass << " / " << g_total << " PASSED";
    if (g_fail > 0)
        std::cout << "  (" << g_fail << " FAILED)";
    std::cout << "\n═══════════════════════════════════════════════════════════════\n";

    return g_fail > 0 ? 1 : 0;
}
