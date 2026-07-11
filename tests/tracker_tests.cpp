/**
 * tracker_tests.cpp
 * =================
 * Verification tests for the Element Index Tracker and Random Element Picker.
 *
 * Tests:
 *   1. Stable creation     — 1000 particles, all IDs unique, all indices valid
 *   2. Reorder stability   — random shuffle, identity preserved
 *   3. Deletion/compaction — 10% removed, survivors preserve identity
 *   4. Seed reproducibility — same seed → same sequence
 *   5. Parent-child lineage — transmutation/decay records
 *   6. Verification pass   — internal consistency check
 *   7. CSV export          — readable output
 *   8. Event log           — all transitions logged
 *
 * Build:
 *   g++ -std=c++20 -I include -I src -o tracker_tests \
 *       tests/tracker_tests.cpp \
 *       src/core/element_index_tracker.cpp \
 *       src/core/random_element_picker.cpp
 *
 * Run:
 *   ./tracker_tests
 */

#include "core/element_descriptor.hpp"
#include "core/element_index_tracker.hpp"
#include "core/random_element_picker.hpp"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>

// ============================================================================
// Test harness
// ============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    std::cout << "  [TEST] " << #name << "... "; \
    try { test_##name(); tests_passed++; std::cout << "PASS\n"; } \
    catch (const std::exception& e) { tests_failed++; std::cout << "FAIL: " << e.what() << "\n"; }

#define ASSERT(cond) \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond " at line " + std::to_string(__LINE__))

// ============================================================================
// Test 1: Stable creation
// ============================================================================

void test_stable_creation() {
    vsepr::ElementIndexTracker tracker;
    vsepr::RandomElementPicker picker(42);

    std::vector<int> allowed_Z = {1, 6, 7, 8, 14, 15, 16, 26, 92, 94};
    picker.set_pool(allowed_Z);

    std::unordered_set<uint64_t> all_ids;

    for (size_t i = 0; i < 1000; i++) {
        auto pick = picker.pick_uniform();
        ASSERT(pick.valid);

        vsepr::ElementDescriptor desc;
        desc.atomic_number = pick.atomic_number;
        desc.symbol        = pick.symbol;

        uint64_t pid = tracker.register_particle(desc, i, 0, true);
        ASSERT(pid > 0);
        ASSERT(all_ids.insert(pid).second); // Must be unique
    }

    ASSERT(all_ids.size() == 1000);
    ASSERT(tracker.total_count() == 1000);
    ASSERT(tracker.active_count() == 1000);

    // Verify every record
    for (const auto& rec : tracker.records()) {
        ASSERT(rec.persistent_id > 0);
        ASSERT(rec.atomic_number > 0);
        ASSERT(!rec.symbol.empty());
        ASSERT(rec.active);
        ASSERT(rec.state_tag == "initialized");
    }

    // Verify internal consistency
    auto v = tracker.verify();
    ASSERT(v.ok);
    ASSERT(v.unique_ids == 1000);
    ASSERT(v.index_collisions == 0);
    ASSERT(v.duplicate_id_violations == 0);
}

// ============================================================================
// Test 2: Reorder stability
// ============================================================================

void test_reorder_stability() {
    vsepr::ElementIndexTracker tracker;

    // Create 100 particles
    for (size_t i = 0; i < 100; i++) {
        vsepr::ElementDescriptor desc;
        desc.atomic_number = static_cast<int>((i % 10) + 1);
        desc.symbol = "X" + std::to_string(desc.atomic_number);
        tracker.register_particle(desc, i, 0);
    }

    // Record original state
    struct Snapshot { uint64_t pid; int Z; std::string sym; };
    std::vector<Snapshot> originals;
    for (const auto& rec : tracker.records()) {
        originals.push_back({rec.persistent_id, rec.atomic_number, rec.symbol});
    }

    // Build random permutation
    std::vector<size_t> old_to_new(100);
    std::iota(old_to_new.begin(), old_to_new.end(), 0);
    std::mt19937 rng(12345);
    std::shuffle(old_to_new.begin(), old_to_new.end(), rng);

    // Apply remap
    auto status = tracker.remap_after_reorder(old_to_new, 34);
    ASSERT(status.is_ok());

    // Verify identity preservation
    for (size_t i = 0; i < originals.size(); i++) {
        auto* rec = tracker.find_by_id(originals[i].pid);
        ASSERT(rec != nullptr);
        ASSERT(rec->atomic_number == originals[i].Z);
        ASSERT(rec->symbol == originals[i].sym);
        ASSERT(rec->persistent_id == originals[i].pid);
    }

    // Verify no index collisions
    auto v = tracker.verify();
    ASSERT(v.ok);
    ASSERT(v.index_collisions == 0);
}

// ============================================================================
// Test 3: Deletion and compaction
// ============================================================================

void test_deletion_compaction() {
    vsepr::ElementIndexTracker tracker;

    // Create 100 particles
    std::vector<uint64_t> pids;
    for (size_t i = 0; i < 100; i++) {
        vsepr::ElementDescriptor desc;
        desc.atomic_number = 6; // All carbon
        desc.symbol = "C";
        pids.push_back(tracker.register_particle(desc, i, 0));
    }

    ASSERT(tracker.active_count() == 100);

    // Remove 10% (every 10th)
    std::vector<uint64_t> removed_ids;
    for (size_t i = 0; i < 100; i += 10) {
        auto status = tracker.remove_particle(pids[i], 50);
        ASSERT(status.is_ok());
        removed_ids.push_back(pids[i]);
    }

    ASSERT(tracker.active_count() == 90);
    ASSERT(tracker.total_count() == 100); // Records preserved

    // Verify removed particles are inactive
    for (uint64_t rid : removed_ids) {
        auto* rec = tracker.find_by_id(rid);
        ASSERT(rec != nullptr);
        ASSERT(!rec->active);
        ASSERT(rec->state_tag == "removed");
    }

    // Verify survivors
    for (size_t i = 0; i < 100; i++) {
        if (i % 10 != 0) {
            auto* rec = tracker.find_by_id(pids[i]);
            ASSERT(rec != nullptr);
            ASSERT(rec->active);
            ASSERT(rec->atomic_number == 6);
        }
    }

    // Double-remove should fail
    auto status = tracker.remove_particle(removed_ids[0], 51);
    ASSERT(status.is_error());

    auto v = tracker.verify();
    ASSERT(v.ok);
}

// ============================================================================
// Test 4: Seed reproducibility
// ============================================================================

void test_seed_reproducibility() {
    std::vector<int> allowed_Z = {1, 6, 7, 8, 14, 15, 16, 26, 92, 94};
    std::vector<double> weights = {100, 95, 80, 90, 15, 25, 35, 5, 1, 0.5};

    // Run 1
    vsepr::RandomElementPicker picker1(46017);
    picker1.set_pool(allowed_Z);
    picker1.set_weights(weights);

    std::vector<int> sequence1;
    for (int i = 0; i < 100; i++) {
        auto r = picker1.pick_weighted();
        ASSERT(r.valid);
        sequence1.push_back(r.atomic_number);
    }

    // Run 2 (same seed, same pool, same weights)
    vsepr::RandomElementPicker picker2(46017);
    picker2.set_pool(allowed_Z);
    picker2.set_weights(weights);

    std::vector<int> sequence2;
    for (int i = 0; i < 100; i++) {
        auto r = picker2.pick_weighted();
        ASSERT(r.valid);
        sequence2.push_back(r.atomic_number);
    }

    // Must be identical
    ASSERT(sequence1.size() == sequence2.size());
    for (size_t i = 0; i < sequence1.size(); i++) {
        ASSERT(sequence1[i] == sequence2[i]);
    }
}

// ============================================================================
// Test 5: Parent-child lineage
// ============================================================================

void test_parent_child_lineage() {
    vsepr::ElementIndexTracker tracker;

    // Create parent: Pu-239
    vsepr::ElementDescriptor pu;
    pu.atomic_number = 94;
    pu.symbol        = "Pu";
    pu.atomic_mass   = 239.0;
    pu.radioactive   = true;
    pu.mass_number   = 239;
    pu.isotope_label = "Pu-239";

    uint64_t parent_pid = tracker.register_particle(pu, 0, 0);

    // Simulate decay at step 450000
    tracker.log_transition(parent_pid, "decayed", 450000);

    // Create daughter: U-235
    vsepr::ElementDescriptor u;
    u.atomic_number = 92;
    u.symbol        = "U";
    u.atomic_mass   = 235.0;
    u.radioactive   = true;
    u.mass_number   = 235;
    u.isotope_label = "U-235";

    uint64_t child_pid = tracker.register_child(u, 1, 450000, parent_pid);

    // Verify parent
    auto* parent = tracker.find_by_id(parent_pid);
    ASSERT(parent != nullptr);
    ASSERT(parent->state_tag == "decayed");
    ASSERT(parent->atomic_number == 94);

    // Verify child
    auto* child = tracker.find_by_id(child_pid);
    ASSERT(child != nullptr);
    ASSERT(child->parent_id == parent_pid);
    ASSERT(child->generation == 1);
    ASSERT(child->atomic_number == 92);
    ASSERT(child->state_tag == "decay_product");
    ASSERT(child->isotope_label == "U-235");

    auto v = tracker.verify();
    ASSERT(v.ok);
}

// ============================================================================
// Test 6: Constrained selection
// ============================================================================

void test_constrained_selection() {
    vsepr::RandomElementPicker picker(99);

    std::vector<int> all_Z;
    for (int Z = 1; Z <= 20; Z++) all_Z.push_back(Z);
    picker.set_pool(all_Z);

    // Constraint: only even Z (nonmetals are not checked here; just even Z for simplicity)
    auto even_only = [](const vsepr::ElementDescriptor& d) {
        return d.atomic_number % 2 == 0;
    };

    for (int i = 0; i < 50; i++) {
        auto r = picker.pick_constrained(even_only);
        ASSERT(r.valid);
        ASSERT(r.atomic_number % 2 == 0);
    }
}

// ============================================================================
// Test 7: CSV export
// ============================================================================

void test_csv_export() {
    vsepr::ElementIndexTracker tracker;

    vsepr::ElementDescriptor h;
    h.atomic_number = 1;
    h.symbol = "H";
    h.atomic_mass = 1.008;

    vsepr::ElementDescriptor c;
    c.atomic_number = 6;
    c.symbol = "C";
    c.atomic_mass = 12.011;

    tracker.register_particle(h, 0, 0, true);
    tracker.register_particle(c, 1, 0, true);

    std::string csv = tracker.to_csv();
    ASSERT(!csv.empty());
    ASSERT(csv.find("persistent_id") != std::string::npos);
    ASSERT(csv.find(",H,") != std::string::npos);
    ASSERT(csv.find(",C,") != std::string::npos);
}

// ============================================================================
// Test 8: Event log completeness
// ============================================================================

void test_event_log() {
    vsepr::ElementIndexTracker tracker;

    vsepr::ElementDescriptor c;
    c.atomic_number = 6;
    c.symbol = "C";

    uint64_t pid = tracker.register_particle(c, 0, 0);
    tracker.log_transition(pid, "bonded", 12);
    tracker.log_transition(pid, "excited", 34);

    // Remap
    std::vector<size_t> remap = {5};
    tracker.remap_after_reorder(remap, 50);

    tracker.remove_particle(pid, 100);

    auto log = tracker.events_to_string();
    ASSERT(!log.empty());
    ASSERT(log.find("created") != std::string::npos);
    ASSERT(log.find("state") != std::string::npos);
    ASSERT(log.find("reindex") != std::string::npos);
    ASSERT(log.find("removed") != std::string::npos);
}

// ============================================================================
// Test 9: Frequency distribution (weighted)
// ============================================================================

void test_weighted_distribution() {
    vsepr::RandomElementPicker picker(777);

    // Pool: H, C, O with weights 10, 80, 10
    std::vector<int> pool = {1, 6, 8};
    std::vector<double> weights = {10.0, 80.0, 10.0};
    picker.set_pool(pool);
    picker.set_weights(weights);

    auto stats = picker.sample_distribution(vsepr::SelectionMode::WEIGHTED, 10000);

    // C should dominate (roughly 80%)
    size_t c_count = 0;
    for (size_t i = 0; i < stats.atomic_numbers.size(); i++) {
        if (stats.atomic_numbers[i] == 6) {
            c_count = stats.counts[i];
        }
    }

    // With 10000 samples and 80% weight, C should be > 70% at minimum
    double c_ratio = static_cast<double>(c_count) / 10000.0;
    ASSERT(c_ratio > 0.70);
    ASSERT(c_ratio < 0.90);
}

// ============================================================================
// Test 10: Case study Day #46 — 12-particle scenario
// ============================================================================

void test_case_study_day46() {
    // Exact scenario from spec: seed 46017, 12 particles
    std::vector<int> allowed_Z = {1, 6, 7, 8, 14, 16, 26, 92, 94};

    vsepr::RandomElementPicker picker(46017);
    picker.set_pool(allowed_Z);

    vsepr::ElementIndexTracker tracker;

    // Create 12 particles
    for (size_t i = 0; i < 12; i++) {
        auto pick = picker.pick_uniform();
        ASSERT(pick.valid);

        vsepr::ElementDescriptor desc;
        desc.atomic_number = pick.atomic_number;
        desc.symbol        = pick.symbol;

        tracker.register_particle(desc, i, 0, true);
    }

    ASSERT(tracker.total_count() == 12);
    ASSERT(tracker.active_count() == 12);

    // Build a random spatial sort permutation (deterministic with known seed)
    std::vector<size_t> old_to_new(12);
    std::iota(old_to_new.begin(), old_to_new.end(), 0);
    std::mt19937 sort_rng(46017);
    std::shuffle(old_to_new.begin(), old_to_new.end(), sort_rng);

    // Apply remap
    auto status = tracker.remap_after_reorder(old_to_new, 34);
    ASSERT(status.is_ok());

    // Verify
    auto v = tracker.verify();
    ASSERT(v.ok);
    ASSERT(v.unique_ids == 12);
    ASSERT(v.active_particles == 12);
    ASSERT(v.index_collisions == 0);

    // Print summary for case study
    std::cout << "\n";
    std::cout << "    Case Study Day #46 Summary:\n";
    std::cout << "    Particles: " << tracker.total_count() << "\n";
    std::cout << "    Active:    " << tracker.active_count() << "\n";
    std::cout << "    Events:    " << tracker.events().size() << "\n";
    std::cout << "    Verify:    " << v.summary << "\n";
    std::cout << "    ";
}

// ============================================================================
// Test 11: Compute metrics
// ============================================================================

void test_compute_metrics() {
    vsepr::ElementIndexTracker tracker;
    vsepr::RandomElementPicker picker(99);
    picker.set_pool({1, 6, 7, 8});

    // Register 20 particles
    for (size_t i = 0; i < 20; i++) {
        auto pick = picker.pick_uniform();
        vsepr::ElementDescriptor desc;
        desc.atomic_number = pick.atomic_number;
        desc.symbol = pick.symbol;
        tracker.register_particle(desc, i, 0, true);
    }

    // Register 2 children
    vsepr::ElementDescriptor child_desc;
    child_desc.atomic_number = 2;
    child_desc.symbol = "He";
    tracker.register_child(child_desc, 20, 1, 1);
    tracker.register_child(child_desc, 21, 1, 1);

    // Remove 3
    tracker.remove_particle(5, 2);
    tracker.remove_particle(10, 2);
    tracker.remove_particle(15, 2);

    // Log some state transitions
    tracker.log_transition(1, "bonded", 3);
    tracker.log_transition(2, "excited", 3);

    auto m = tracker.compute_metrics();
    ASSERT(m.total_registered == 22);
    ASSERT(m.active_particles == 19);
    ASSERT(m.inactive_particles == 3);
    ASSERT(m.random_selections == 20);  // all 20 originals were random
    ASSERT(m.child_particles == 2);
    ASSERT(m.highest_id == 22);
    ASSERT(m.unique_elements >= 2);  // at minimum H and He
    ASSERT(m.creation_events == 22);
    ASSERT(m.removal_events == 3);
    ASSERT(m.state_events == 2);
    ASSERT(!m.summary.empty());
    ASSERT(m.element_frequency.count(2) > 0);  // He present

    // Verify element_frequency() matches
    auto freq = tracker.element_frequency();
    size_t freq_total = 0;
    for (auto& [z, c] : freq) freq_total += c;
    ASSERT(freq_total == 19);  // 19 active particles
}

// ============================================================================
// Test 12: Domain-specific error codes
// ============================================================================

void test_domain_error_codes() {
    vsepr::ElementIndexTracker tracker;

    vsepr::ElementDescriptor desc;
    desc.atomic_number = 6;
    desc.symbol = "C";
    uint64_t pid = tracker.register_particle(desc, 0, 0);

    // Update a non-existent particle
    auto s1 = tracker.update_index(999, 5);
    ASSERT(s1.is_error());
    ASSERT(s1.error().code == vsepr::ErrorCode::TRACKER_PARTICLE_NOT_FOUND);

    // Remove the particle, then try to update it
    tracker.remove_particle(pid, 1);
    auto s2 = tracker.update_index(pid, 5);
    ASSERT(s2.is_error());
    ASSERT(s2.error().code == vsepr::ErrorCode::TRACKER_PARTICLE_INACTIVE);

    // Try to remove it again
    auto s3 = tracker.remove_particle(pid, 2);
    ASSERT(s3.is_error());
    ASSERT(s3.error().code == vsepr::ErrorCode::TRACKER_PARTICLE_INACTIVE);

    // Remap with out-of-range index
    vsepr::ElementDescriptor desc2;
    desc2.atomic_number = 1;
    desc2.symbol = "H";
    tracker.register_particle(desc2, 100, 3);
    std::vector<size_t> remap = {0, 1, 2};  // too small for index 100
    auto s4 = tracker.remap_after_reorder(remap, 4);
    ASSERT(s4.is_error());
    ASSERT(s4.error().code == vsepr::ErrorCode::TRACKER_REMAP_OUT_OF_RANGE);

    // Log transition on non-existent particle
    auto s5 = tracker.log_transition(9999, "decayed", 5);
    ASSERT(s5.is_error());
    ASSERT(s5.error().code == vsepr::ErrorCode::TRACKER_PARTICLE_NOT_FOUND);
}

// ============================================================================
// Test 13: Weight loading from JSON
// ============================================================================

void test_weight_loading() {
    vsepr::RandomElementPicker picker(42);
    picker.set_pool({1, 6, 7, 8, 9, 15, 16, 17, 35, 53});

    // Load default weights
    auto r = picker.load_weights_from_json("data/element_weights.json");
    ASSERT(r.ok);
    ASSERT(r.matched > 0);  // At least some elements should match
    ASSERT(r.file_entries >= 100);  // JSON has 118 elements

    // Weights should be set
    const auto& w = picker.weights();
    ASSERT(w.size() == 10);
    ASSERT(w[0] > 0.0);   // H should have weight > 0
    ASSERT(w[1] > 0.0);   // C should have weight > 0

    // Load organic preset
    auto r2 = picker.load_weights_from_json("data/element_weights.json", "organic");
    ASSERT(r2.ok);
    ASSERT(r2.matched > 0);
    const auto& w2 = picker.weights();
    ASSERT(w2[0] == 100.0);  // H in organic preset = 100.0
    ASSERT(w2[1] == 95.0);   // C in organic preset = 95.0

    // Load non-existent preset should fail
    auto r3 = picker.load_weights_from_json("data/element_weights.json", "nonexistent");
    ASSERT(!r3.ok);
    ASSERT(!r3.error.empty());

    // Load non-existent file should fail
    auto r4 = picker.load_weights_from_json("nonexistent.json");
    ASSERT(!r4.ok);
    ASSERT(!r4.error.empty());
}

// ============================================================================
// Test 14: Verification with metrics summary
// ============================================================================

void test_metrics_summary() {
    vsepr::ElementIndexTracker tracker;
    vsepr::RandomElementPicker picker(77);
    picker.set_pool({1, 6, 8, 26, 92});

    for (size_t i = 0; i < 50; i++) {
        auto pick = picker.pick_uniform();
        vsepr::ElementDescriptor desc;
        desc.atomic_number = pick.atomic_number;
        desc.symbol = pick.symbol;
        tracker.register_particle(desc, i, 0, true);
    }

    auto m = tracker.compute_metrics();
    ASSERT(m.total_registered == 50);
    ASSERT(m.active_particles == 50);
    ASSERT(m.inactive_particles == 0);
    ASSERT(m.unique_elements <= 5);  // pool has 5
    ASSERT(m.unique_elements >= 1);  // at least 1 must appear
    ASSERT(m.max_generation == 0);   // no children

    // Summary string should contain key info
    ASSERT(m.summary.find("50") != std::string::npos);
    ASSERT(m.summary.find("active") != std::string::npos);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "====================================\n";
    std::cout << " Element Index Tracker — Test Suite\n";
    std::cout << "====================================\n\n";

    auto t0 = std::chrono::steady_clock::now();

    TEST(stable_creation);
    TEST(reorder_stability);
    TEST(deletion_compaction);
    TEST(seed_reproducibility);
    TEST(parent_child_lineage);
    TEST(constrained_selection);
    TEST(csv_export);
    TEST(event_log);
    TEST(weighted_distribution);
    TEST(case_study_day46);
    TEST(compute_metrics);
    TEST(domain_error_codes);
    TEST(weight_loading);
    TEST(metrics_summary);

    auto t1 = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "\n====================================\n";
    std::cout << " Results: " << tests_passed << " passed, "
              << tests_failed << " failed"
              << " (" << std::fixed << std::setprecision(1) << elapsed_ms << " ms)\n";
    std::cout << "====================================\n";

    return tests_failed > 0 ? 1 : 0;
}
