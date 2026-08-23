/**
 * test_gap_classifier.cpp  --  WO-74C  --  Group 80
 * ====================================================
 * Tests for classify_gap() and classify_record_gap() from gap_classifier.cpp.
 */

#include "multiscale/continual_run.hpp"

#include <cstdio>
#include <cmath>
#include <cassert>
#include <string>

namespace vm = vsepr::multiscale;

// Declare out-of-line functions (defined in gap_classifier.cpp)
namespace vsepr { namespace multiscale {
    std::string classify_gap(double lambda_sim, double lambda_ref, double tolerance_nm);
    void classify_record_gap(ContinualRunRecord& rec, double tolerance_nm);
} }

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++tests_run;                                                         \
        if (cond) { ++tests_passed; }                                        \
        else { std::printf("FAIL  line %d: %s\n", __LINE__, #cond); }       \
    } while(0)

// ============================================================================
// [1] good_match: |diff| <= tolerance
// ============================================================================

static void test_good_match()
{
    CHECK(vm::classify_gap(1.00, 1.00, 0.05) == "good_match");
    CHECK(vm::classify_gap(1.04, 1.00, 0.05) == "good_match");
    CHECK(vm::classify_gap(0.96, 1.00, 0.05) == "good_match");
    CHECK(vm::classify_gap(1.05, 1.00, 0.05) == "good_match"); // boundary
    CHECK(vm::classify_gap(0.95, 1.00, 0.05) == "good_match"); // boundary
}

// ============================================================================
// [2] overextended_field: sim > ref + tolerance
// ============================================================================

static void test_overextended_field()
{
    CHECK(vm::classify_gap(1.10, 1.00, 0.05) == "overextended_field");
    CHECK(vm::classify_gap(2.00, 1.00, 0.05) == "overextended_field");
}

// ============================================================================
// [3] underresolved_scale: sim < ref - tolerance
// ============================================================================

static void test_underresolved_scale()
{
    CHECK(vm::classify_gap(0.90, 1.00, 0.05) == "underresolved_scale");
    CHECK(vm::classify_gap(0.50, 1.00, 0.05) == "underresolved_scale");
}

// ============================================================================
// [4] classify_record_gap stamps the record
// ============================================================================

static void test_classify_record_gap()
{
    vm::ContinualRunRecord rec;
    rec.Lambda_nm       = 1.15;
    rec.prior_lambda_nm = 1.00;

    vm::classify_record_gap(rec, 0.05);
    CHECK(rec.gap_label == "overextended_field");

    rec.Lambda_nm = 0.98;
    vm::classify_record_gap(rec, 0.05);
    CHECK(rec.gap_label == "good_match");

    rec.Lambda_nm = 0.80;
    vm::classify_record_gap(rec, 0.05);
    CHECK(rec.gap_label == "underresolved_scale");
}

// ============================================================================
// [5] apply_prior_update sanity (inline in header)
// ============================================================================

static void test_prior_update()
{
    vm::ContinualRunRecord rec;
    rec.prior_lambda_nm    = 1.00;
    rec.prior_sigma_nm     = 0.10;
    rec.Lambda_nm          = 1.20;
    rec.prior_energy       = -10.0;
    rec.prior_energy_sigma = 1.0;
    rec.E_value            = -12.0;

    vm::apply_prior_update(rec, 0.05, 1.0);

    // updated_lambda_nm must be between prior and sim
    CHECK(rec.updated_lambda_nm > 1.00 && rec.updated_lambda_nm < 1.20);
    CHECK(rec.updated_sigma_nm > 0.0 && rec.updated_sigma_nm < 0.10);
    CHECK(rec.updated_energy > -12.0 && rec.updated_energy < -10.0);
}

// ============================================================================
// [6] Registry — both strategies registered and return consistent results
// ============================================================================
// This section iterates ModuleRegistry<IGapClassifier> and verifies that
// every registered classifier agrees on the "good_match" case.
// New classifiers self-register and are covered automatically.

#include "vsim/analysis/i_gap_classifier.hpp"
#include "vsim/module_registry.hpp"

static void test_registry_good_match()
{
    using namespace vsepr::multiscale;
    auto& reg = vsim::ModuleRegistry<IGapClassifier>::get();

    // At least "threshold" and "adaptive" must be registered.
    CHECK(reg.size() >= 2);
    CHECK(reg.has("threshold"));
    CHECK(reg.has("adaptive"));

    // Exact match — every strategy must return "good_match".
    GapInput exact;
    exact.lambda_sim   = 1.0;
    exact.lambda_emp   = 1.0;
    exact.e_sim        = -10.0;
    exact.e_emp        = -10.0;
    exact.ref_count    = 5;
    exact.tolerance_nm = 0.05;

    for (std::string_view name : reg.names()) {
        auto clf = reg.create(name);
        CHECK(clf != nullptr);
        if (!clf) continue;
        auto result = clf->classify(exact);
        CHECK(result.gap_label == "good_match");
    }
}

static void test_registry_descriptions()
{
    using namespace vsepr::multiscale;
    auto& reg = vsim::ModuleRegistry<IGapClassifier>::get();

    // description() must return a non-empty string for every classifier.
    for (std::string_view name : reg.names()) {
        auto clf = reg.create(name);
        CHECK(clf != nullptr);
        if (!clf) continue;
        CHECK(!clf->description().empty());
        CHECK(clf->strategy_name() == name);
    }
}

// ============================================================================

int main()
{
    test_good_match();
    test_overextended_field();
    test_underresolved_scale();
    test_classify_record_gap();
    test_prior_update();
    test_registry_good_match();
    test_registry_descriptions();

    std::printf("[GapClassifierGroup80]  %d / %d tests passed\n",
                tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}