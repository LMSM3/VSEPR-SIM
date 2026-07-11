#pragma once
/**
 * random_element_picker.hpp
 * =========================
 * Seeded random element selection with three modes.
 *
 * Mode 1: Uniform   - every element in the allowed pool has equal chance
 * Mode 2: Weighted  - elements have user-supplied probability weights
 * Mode 3: Rule-constrained - selection must satisfy a user-defined predicate
 *
 * All modes are deterministic given the same seed, same pool, same weights.
 * The seed is stored and exported with every run for reproducibility.
 *
 * Integration:
 *   Returns ElementDescriptor objects ready for ElementIndexTracker::register_particle().
 *   Can load weights from data/element_weights.json.
 */

#include <cstdint>
#include <vector>
#include <random>
#include <string>
#include <functional>

#include "core/element_descriptor.hpp"

namespace vsepr {

enum class SelectionMode : uint8_t {
    UNIFORM     = 0,
    WEIGHTED    = 1,
    CONSTRAINED = 2
};

struct PickResult {
    int         atomic_number   = 0;
    std::string symbol;
    SelectionMode mode          = SelectionMode::UNIFORM;
    uint64_t    seed_used       = 0;
    size_t      attempt_count   = 1;
    bool        valid           = false;
};

class RandomElementPicker {
public:
    explicit RandomElementPicker(uint64_t seed);

    void reseed(uint64_t seed);
    uint64_t seed() const { return seed_; }

    void set_pool(const std::vector<int>& allowed_Z);
    void set_pool(const std::vector<ElementDescriptor>& descriptors);
    void set_weights(const std::vector<double>& weights);

    struct WeightLoadResult {
        bool        ok              = false;
        size_t      matched         = 0;
        size_t      unmatched       = 0;
        size_t      file_entries    = 0;
        std::string error;
    };
    WeightLoadResult load_weights_from_json(
        const std::string& path,
        const std::string& preset = ""
    );

    const std::vector<ElementDescriptor>& pool() const { return pool_; }
    const std::vector<double>& weights() const { return weights_; }

    PickResult pick_uniform();
    PickResult pick_weighted();

    using Predicate = std::function<bool(const ElementDescriptor&)>;
    PickResult pick_constrained(Predicate pred, size_t max_attempts = 1000);

    PickResult pick(SelectionMode mode);

    struct FrequencyStats {
        std::vector<int>    atomic_numbers;
        std::vector<size_t> counts;
        size_t              total_picks = 0;
    };

    FrequencyStats sample_distribution(SelectionMode mode, size_t n);

    size_t total_picks() const { return total_picks_; }

private:
    uint64_t    seed_;
    std::mt19937_64 rng_;

    std::vector<ElementDescriptor> pool_;
    std::vector<double> weights_;

    size_t total_picks_ = 0;

    ElementDescriptor make_minimal_descriptor(int Z) const;
};

} // namespace vsepr
