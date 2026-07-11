/**
 * test_code_trail.cpp  —  Code Trail Wind v0.1 — verification tests
 * ==================================================================
 * VSEPR-SIM 3.0.1
 *
 * Tests:
 *   T1 : basic binary recording — step counter, field values
 *   T2 : unary recording — NaN rhs, correct result
 *   T3 : assign recording — formula_notation auto-generated
 *   T4 : compare recording — bool stored as 1.0 / 0.0
 *   T5 : custom recording — lhs/rhs NaN, formula carries meaning
 *   T6 : TrailScope tag push/pop
 *   T7 : stats aggregation — counts per OpKind
 *   T8 : CSV string output — header present, row count correct
 *   T9 : flush_csv — file written, readable back
 *   T10: reset — clears entries, resets step counter
 *   T11: TrailWriter streaming — file opened, rows written
 *   T12: LJ pair walk-through — realistic force calculation trail
 *   T13: accumulator chain — running energy sum across steps
 *   T14: CSV escape — commas/quotes in formula_notation survive round-trip
 *
 * Build:
 *   Registered via CMake target: test_code_trail
 */

#include "core/code_trail.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace vsepr::trail;

// ============================================================================
// Test harness
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    std::cout << "  [TEST] " << #name << "... "; \
    try { test_##name(); ++g_passed; std::cout << "PASS\n"; } \
    catch (const std::exception& ex) { ++g_failed; std::cout << "FAIL: " << ex.what() << "\n"; }

#define ASSERT(expr) \
    do { if (!(expr)) throw std::runtime_error("Assertion failed: " #expr); } while(0)

#define ASSERT_NEAR(a, b, tol) \
    do { if (std::abs((a)-(b)) > (tol)) { \
        std::ostringstream _msg; \
        _msg << #a << " = " << (a) << " not within " << (tol) << " of " << #b << " = " << (b); \
        throw std::runtime_error(_msg.str()); } } while(0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
    do { if ((haystack).find(needle) == std::string::npos) { \
        throw std::runtime_error(std::string("Expected to find \"") + (needle) + "\" in output"); } } while(0)

// ============================================================================
// T1: basic binary recording
// ============================================================================

static void test_basic_binary() {
    CodeTrail t("t1_binary");
    t.record_binary("multiply", 2.0, 3.0, 6.0, 6.0, "result = 2 * 3", "kcal/mol", "test");

    ASSERT(t.step_count() == 1);
    ASSERT(t.entries().size() == 1);

    const TrailEntry& e = t.entries()[0];
    ASSERT(e.step       == 0);
    ASSERT(e.op_label   == "multiply");
    ASSERT(e.kind       == OpKind::BINARY);
    ASSERT_NEAR(e.lhs,         2.0,  1e-15);
    ASSERT_NEAR(e.rhs,         3.0,  1e-15);
    ASSERT_NEAR(e.result,      6.0,  1e-15);
    ASSERT_NEAR(e.accumulator, 6.0,  1e-15);
    ASSERT(e.unit_label      == "kcal/mol");
    ASSERT(e.formula_notation == "result = 2 * 3");
    ASSERT(e.source_tag       == "test");
    ASSERT(!e.is_unary());
}

// ============================================================================
// T2: unary recording
// ============================================================================

static void test_unary_recording() {
    CodeTrail t("t2_unary");
    double input = 4.0;
    double result = std::sqrt(input);
    t.record_unary("sqrt", input, result, result, "r = sqrt(4)", "Angstrom", "bond_length");

    ASSERT(t.entries().size() == 1);
    const TrailEntry& e = t.entries()[0];
    ASSERT(e.kind == OpKind::UNARY);
    ASSERT(e.is_unary());
    ASSERT(std::isnan(e.rhs));
    ASSERT_NEAR(e.lhs,    4.0, 1e-15);
    ASSERT_NEAR(e.result, 2.0, 1e-15);
}

// ============================================================================
// T3: assign recording (formula auto-generated when empty)
// ============================================================================

static void test_assign_recording() {
    CodeTrail t("t3_assign");
    t.record_assign("epsilon", 0.238, 0.238, "", "kcal/mol", "LJ");

    const TrailEntry& e = t.entries()[0];
    ASSERT(e.kind == OpKind::ASSIGN);
    // formula_notation should be auto-generated: "epsilon = 0.238000"
    ASSERT(!e.formula_notation.empty());
    ASSERT_STR_CONTAINS(e.formula_notation, "epsilon");
    ASSERT_NEAR(e.result, 0.238, 1e-15);
}

// ============================================================================
// T4: compare recording
// ============================================================================

static void test_compare_recording() {
    CodeTrail t("t4_compare");
    t.record_compare("less_than", 1.5, 2.0, true,  0.0, "r < r_cutoff", "check");
    t.record_compare("less_than", 3.0, 2.0, false, 0.0, "r < r_cutoff", "check");

    ASSERT(t.entries().size() == 2);
    ASSERT(t.entries()[0].kind   == OpKind::COMPARE);
    ASSERT_NEAR(t.entries()[0].result, 1.0, 1e-15);   // true  → 1.0
    ASSERT_NEAR(t.entries()[1].result, 0.0, 1e-15);   // false → 0.0
}

// ============================================================================
// T5: custom recording
// ============================================================================

static void test_custom_recording() {
    CodeTrail t("t5_custom");
    t.record_custom("LJ_potential", -0.1234, -0.1234,
                    "U = 4*eps*[(sig/r)^12 - (sig/r)^6]",
                    "kcal/mol", "pair_3_7");

    const TrailEntry& e = t.entries()[0];
    ASSERT(e.kind == OpKind::CUSTOM);
    ASSERT(std::isnan(e.lhs));
    ASSERT(std::isnan(e.rhs));
    ASSERT_NEAR(e.result, -0.1234, 1e-14);
    ASSERT(e.op_label == "LJ_potential");
    ASSERT_STR_CONTAINS(e.formula_notation, "sig/r");
}

// ============================================================================
// T6: TrailScope tag push/pop
// ============================================================================

static void test_trail_scope() {
    CodeTrail t("t6_scope");

    // Outside scope — no active tag
    t.record_binary("add", 1.0, 2.0, 3.0, 3.0);
    ASSERT(t.entries()[0].source_tag.empty());

    {
        TrailScope scope(t, "bond_calc");
        t.record_binary("multiply", 3.0, 4.0, 12.0, 12.0);
        ASSERT(t.entries()[1].source_tag == "bond_calc");

        // Nested explicit tag — scope tag wins
        t.record_unary("sqrt", 12.0, std::sqrt(12.0), std::sqrt(12.0));
        ASSERT(t.entries()[2].source_tag == "bond_calc");
    }

    // After scope — tag cleared
    t.record_binary("subtract", 10.0, 3.0, 7.0, 7.0);
    ASSERT(t.entries()[3].source_tag.empty());
}

// ============================================================================
// T7: stats aggregation
// ============================================================================

static void test_stats_aggregation() {
    CodeTrail t("t7_stats");
    t.record_binary("add",      1.0, 2.0, 3.0, 3.0);
    t.record_binary("multiply", 3.0, 2.0, 6.0, 6.0);
    t.record_unary ("sqrt",     4.0, 2.0, 2.0);
    t.record_assign("k_bolt",   1.38e-23, 1.38e-23);
    t.record_compare("gt",      2.0, 1.0, true, 0.0);
    t.record_custom("custom_op", 99.0, 99.0, "x = f(y)");

    TrailStats st = t.stats();
    ASSERT(st.total_steps  == 6);
    ASSERT(st.binary_ops   == 2);
    ASSERT(st.unary_ops    == 1);
    ASSERT(st.assign_ops   == 1);
    ASSERT(st.compare_ops  == 1);
    ASSERT(st.custom_ops   == 1);
    ASSERT_NEAR(st.final_accumulator, 99.0, 1e-12);
}

// ============================================================================
// T8: CSV string output
// ============================================================================

static void test_csv_string_output() {
    CodeTrail t("t8_csv");
    t.record_binary("add",  1.0, 2.0, 3.0, 3.0, "total = a + b", "eV");
    t.record_unary ("sqrt", 9.0, 3.0, 3.0, "r = sqrt(9)", "Angstrom");

    std::string csv = t.to_csv_string();

    // Preamble present
    ASSERT_STR_CONTAINS(csv, "# Code Trail Wind v0.1");
    ASSERT_STR_CONTAINS(csv, "# trail_name: t8_csv");
    ASSERT_STR_CONTAINS(csv, "# total_steps: 2");

    // Header present
    ASSERT_STR_CONTAINS(csv, "step,op_kind,op_label");
    ASSERT_STR_CONTAINS(csv, "formula_notation");

    // Data rows: 2 rows of data + header line + preamble lines
    int data_rows = 0;
    std::istringstream ss(csv);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[0] != '#' &&
            line.find("step,op_kind") == std::string::npos) {
            ++data_rows;
        }
    }
    ASSERT(data_rows == 2);
}

// ============================================================================
// T9: flush_csv — file written and readable back
// ============================================================================

static void test_flush_csv() {
    CodeTrail t("t9_flush");
    t.record_binary("divide", 10.0, 4.0, 2.5, 2.5, "ratio = 10 / 4");

    const std::string path = "test_code_trail_t9.csv";
    bool ok = t.flush_csv(path);
    ASSERT(ok);

    std::ifstream f(path);
    ASSERT(f.is_open());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    ASSERT_STR_CONTAINS(content, "# Code Trail Wind");
    ASSERT_STR_CONTAINS(content, "divide");
    ASSERT_STR_CONTAINS(content, "2.5");

    // Clean up
    f.close();
    std::remove(path.c_str());
}

// ============================================================================
// T10: reset — clears entries, resets step counter
// ============================================================================

static void test_reset() {
    CodeTrail t("t10_reset");
    t.record_binary("add", 1.0, 1.0, 2.0, 2.0);
    t.record_binary("add", 2.0, 1.0, 3.0, 3.0);
    ASSERT(t.step_count() == 2);

    t.reset();
    ASSERT(t.step_count()     == 0);
    ASSERT(t.entries().empty());

    // After reset, step indices restart from 0
    t.record_unary("neg", 5.0, -5.0, -5.0);
    ASSERT(t.entries()[0].step == 0);
}

// ============================================================================
// T11: TrailWriter streaming
// ============================================================================

static void test_trail_writer() {
    const std::string path = "test_code_trail_t11.csv";
    {
        TrailWriter w(path, "t11_writer");
        ASSERT(w.is_open());
        w.record_binary("add",    1.0, 2.0, 3.0, 3.0, "sum = 1 + 2");
        w.record_unary ("abs",   -3.0, 3.0, 3.0, "|x|");
        w.record_assign("sigma",  3.4, 3.4, "", "Angstrom");
        ASSERT(w.step_count() == 3);
    }
    // After destructor, file should be closed and readable
    std::ifstream f(path);
    ASSERT(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    ASSERT_STR_CONTAINS(content, "# Code Trail Wind");
    ASSERT_STR_CONTAINS(content, "t11_writer");
    ASSERT_STR_CONTAINS(content, "total_steps_written: 3");
    ASSERT_STR_CONTAINS(content, "sigma");
    f.close();
    std::remove(path.c_str());
}

// ============================================================================
// T12: LJ pair walk-through  —  realistic force calculation trail
// ============================================================================
//
// Lennard-Jones potential:
//   U = 4 * epsilon * [(sigma/r)^12 - (sigma/r)^6]
//   F = -dU/dr = 24 * epsilon/r * [2*(sigma/r)^12 - (sigma/r)^6]
//
// Walk-through parameters:  epsilon = 0.238 kcal/mol, sigma = 3.4 Å, r = 4.0 Å

static void test_lj_pair_walkthrough() {
    const double eps   = 0.238;
    const double sig   = 3.4;
    const double r     = 4.0;

    CodeTrail t("LJ_pair_3_7");
    double acc = 0.0;

    // Step 0: assign epsilon
    t.record_assign("epsilon", eps, acc, "epsilon = 0.238 kcal/mol", "kcal/mol", "LJ");

    // Step 1: assign sigma
    t.record_assign("sigma", sig, acc, "sigma = 3.4 Angstrom", "Angstrom", "LJ");

    // Step 2: assign r
    t.record_assign("r", r, acc, "r = 4.0 Angstrom", "Angstrom", "LJ");

    // Step 3: sr = sigma / r
    double sr = sig / r;
    t.record_binary("divide", sig, r, sr, acc, "sr = sigma / r", "", "LJ");

    // Step 4: sr6 = sr^6
    double sr6 = sr * sr * sr * sr * sr * sr;
    t.record_unary("pow6", sr, sr6, acc, "sr6 = (sigma/r)^6", "", "LJ");

    // Step 5: sr12 = sr6^2
    double sr12 = sr6 * sr6;
    t.record_unary("pow12", sr6, sr12, acc, "sr12 = (sigma/r)^12", "", "LJ");

    // Step 6: term1 = sr12 - sr6
    double term1 = sr12 - sr6;
    t.record_binary("subtract", sr12, sr6, term1, acc,
                    "term1 = sr12 - sr6", "", "LJ");

    // Step 7: U = 4 * eps * term1
    double U = 4.0 * eps * term1;
    acc = U;
    t.record_binary("multiply", 4.0 * eps, term1, U, acc,
                    "U = 4 * epsilon * (sr12 - sr6)", "kcal/mol", "LJ");

    // Step 8: F = 24 * eps / r * (2*sr12 - sr6)
    double F = 24.0 * eps / r * (2.0 * sr12 - sr6);
    t.record_custom("LJ_force", F, acc,
                    "F = 24*epsilon/r * (2*sr12 - sr6)", "kcal/mol/A", "LJ");

    TrailStats st = t.stats();
    ASSERT(st.total_steps == 9);
    ASSERT(st.assign_ops  == 3);
    ASSERT(st.binary_ops  == 3);
    ASSERT(st.unary_ops   == 2);
    ASSERT(st.custom_ops  == 1);

    // Verify U value
    double expected_U = 4.0 * eps * (std::pow(sig/r, 12) - std::pow(sig/r, 6));
    ASSERT_NEAR(U, expected_U, 1e-12);
}

// ============================================================================
// T13: accumulator chain — running energy sum
// ============================================================================

static void test_accumulator_chain() {
    CodeTrail t("t13_acc_chain");
    double running = 0.0;

    // Simulate accumulating bond energies across 5 bonds
    for (int i = 0; i < 5; ++i) {
        double k  = 300.0;               // kcal/mol/A^2
        double dr = 0.02 * (i + 1);      // deviation from equilibrium
        double E  = 0.5 * k * dr * dr;

        double half_k = 0.5 * k;
        t.record_binary("multiply", 0.5, k, half_k, running,
                        "half_k = 0.5 * k", "kcal/mol/A^2",
                        "bond_" + std::to_string(i));

        double dr2 = dr * dr;
        t.record_unary("square", dr, dr2, running,
                       "dr2 = dr^2", "A^2",
                       "bond_" + std::to_string(i));

        running += E;
        t.record_binary("multiply", half_k, dr2, E, running,
                        "E_bond = 0.5 * k * dr^2", "kcal/mol",
                        "bond_" + std::to_string(i));
    }

    ASSERT(t.step_count() == 15);   // 3 steps per bond × 5 bonds

    // Final accumulator should equal sum of all bond energies
    double expected = 0.0;
    for (int i = 0; i < 5; ++i) {
        double dr = 0.02 * (i + 1);
        expected += 0.5 * 300.0 * dr * dr;
    }
    ASSERT_NEAR(t.entries().back().accumulator, expected, 1e-12);
}

// ============================================================================
// T14: CSV escape — commas/quotes in formula_notation
// ============================================================================

static void test_csv_escape() {
    CodeTrail t("t14_escape");
    // Formula notation with commas and quotes — must survive CSV round-trip
    t.record_custom("test_op", 1.0, 1.0,
                    "f(a, b) = \"a + b\"",
                    "units, mixed", "ctx,tag");

    std::string csv = t.to_csv_string();

    // The formula with commas/quotes must appear correctly in the CSV
    ASSERT_STR_CONTAINS(csv, "f(a, b)");
    ASSERT_STR_CONTAINS(csv, "a + b");

    // Parse back — find the single data row (not a comment or header)
    std::istringstream ss(csv);
    std::string line;
    int data_rows = 0;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[0] != '#' &&
            line.find("step,op_kind") == std::string::npos) {
            ++data_rows;
            // Row must start with "0," (step index 0)
            ASSERT(line.rfind("0,", 0) == 0);
        }
    }
    ASSERT(data_rows == 1);
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════════════════╗\n";
    std::cout << "  ║  Code Trail Wind v0.1  —  Verification Tests ║\n";
    std::cout << "  ╚══════════════════════════════════════════════╝\n\n";

    TEST(basic_binary)
    TEST(unary_recording)
    TEST(assign_recording)
    TEST(compare_recording)
    TEST(custom_recording)
    TEST(trail_scope)
    TEST(stats_aggregation)
    TEST(csv_string_output)
    TEST(flush_csv)
    TEST(reset)
    TEST(trail_writer)
    TEST(lj_pair_walkthrough)
    TEST(accumulator_chain)
    TEST(csv_escape)

    std::cout << "\n  ──────────────────────────────────────────────\n";
    std::cout << "  Results: " << g_passed << " passed, " << g_failed << " failed\n";

    if (g_failed == 0) {
        std::cout << "  Status:  ALL PASS\n\n";
        return 0;
    }
    std::cout << "  Status:  FAILURES DETECTED\n\n";
    return 1;
}
