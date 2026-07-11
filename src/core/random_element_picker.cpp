/**
 * random_element_picker.cpp
 * =========================
 * Seeded random element selection — three modes.
 *
 * Deterministic: same seed, same pool, same weights → same sequence.
 * Every pick is logged. Every seed is traceable.
 */

#include "core/random_element_picker.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <fstream>
#include <sstream>

namespace vsepr {

// ============================================================================
// Known symbols for minimal descriptors
// ============================================================================

static const char* SYMBOLS[] = {
    "??",
    "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar", "K",  "Ca",
    "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
    "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y",  "Zr",
    "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn",
    "Sb", "Te", "I",  "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd",
    "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb",
    "Lu", "Hf", "Ta", "W",  "Re", "Os", "Ir", "Pt", "Au", "Hg",
    "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr", "Ra", "Ac", "Th",
    "Pa", "U",  "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm",
    "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds",
    "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"
};

static constexpr int MAX_SYMBOL_Z = 118;

// Radioactive elements: Tc(43), Pm(61), Po(84)-Og(118)
static bool is_radioactive(int Z) {
    if (Z == 43 || Z == 61) return true;
    if (Z >= 84 && Z <= 118) return true;
    return false;
}

// Approximate atomic masses for minimal descriptors
static const double APPROX_MASS[] = {
    0.0,
    1.008,  4.003,  6.941,  9.012,  10.81,  12.01,  14.01,  16.00,  19.00,  20.18,
    22.99,  24.31,  26.98,  28.09,  30.97,  32.06,  35.45,  39.95,  39.10,  40.08,
    44.96,  47.87,  50.94,  52.00,  54.94,  55.85,  58.93,  58.69,  63.55,  65.38,
    69.72,  72.63,  74.92,  78.96,  79.90,  83.80,  85.47,  87.62,  88.91,  91.22,
    92.91,  95.96,  98.00, 101.07, 102.91, 106.42, 107.87, 112.41, 114.82, 118.71,
    121.76, 127.60, 126.90, 131.29, 132.91, 137.33, 138.91, 140.12, 140.91, 144.24,
    145.00, 150.36, 151.96, 157.25, 158.93, 162.50, 164.93, 167.26, 168.93, 173.04,
    174.97, 178.49, 180.95, 183.84, 186.21, 190.23, 192.22, 195.08, 196.97, 200.59,
    204.38, 207.20, 208.98, 209.00, 210.00, 222.00, 223.00, 226.00, 227.00, 232.04,
    231.04, 238.03, 237.00, 244.00, 243.00, 247.00, 247.00, 251.00, 252.00, 257.00,
    258.00, 259.00, 262.00, 267.00, 270.00, 271.00, 270.00, 277.00, 276.00, 281.00,
    280.00, 285.00, 286.00, 289.00, 289.00, 293.00, 294.00, 294.00
};

// ============================================================================
// Construction
// ============================================================================

RandomElementPicker::RandomElementPicker(uint64_t seed)
    : seed_(seed)
    , rng_(seed)
{}

void RandomElementPicker::reseed(uint64_t seed) {
    seed_ = seed;
    rng_.seed(seed);
}

// ============================================================================
// Pool management
// ============================================================================

void RandomElementPicker::set_pool(const std::vector<int>& allowed_Z) {
    pool_.clear();
    pool_.reserve(allowed_Z.size());
    for (int Z : allowed_Z) {
        pool_.push_back(make_minimal_descriptor(Z));
    }
}

void RandomElementPicker::set_pool(const std::vector<ElementDescriptor>& descriptors) {
    pool_ = descriptors;
}

void RandomElementPicker::set_weights(const std::vector<double>& weights) {
    weights_ = weights;
}

// ============================================================================
// Weight loading from JSON
// ============================================================================

// Minimal JSON key-value extractor for { "key": number } pairs.
// Not a general parser — only handles the element_weights.json format.
// Avoids adding nlohmann/json as a dependency for this translation unit.

static std::string extract_block(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find('{', pos);
    if (pos == std::string::npos) return "";
    int depth = 1;
    size_t start = pos;
    for (size_t i = pos + 1; i < json.size() && depth > 0; i++) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') depth--;
        if (depth == 0) return json.substr(start, i - start + 1);
    }
    return "";
}

static std::unordered_map<std::string, double> parse_weight_block(const std::string& block) {
    std::unordered_map<std::string, double> result;
    size_t i = 0;
    while (i < block.size()) {
        auto q1 = block.find('"', i);
        if (q1 == std::string::npos) break;
        auto q2 = block.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string key = block.substr(q1 + 1, q2 - q1 - 1);
        auto colon = block.find(':', q2);
        if (colon == std::string::npos) break;
        // Skip whitespace after colon
        size_t ns = colon + 1;
        while (ns < block.size() && (block[ns] == ' ' || block[ns] == '\t' || block[ns] == '\n' || block[ns] == '\r')) ns++;
        // If value is a string (starts with "), skip past the closing quote
        if (ns < block.size() && block[ns] == '"') {
            auto close_quote = block.find('"', ns + 1);
            i = (close_quote != std::string::npos) ? close_quote + 1 : block.size();
            continue;
        }
        // Try to parse as number
        size_t ne = ns;
        while (ne < block.size() && (std::isdigit(block[ne]) || block[ne] == '.' || block[ne] == '-' || block[ne] == '+' || block[ne] == 'e' || block[ne] == 'E')) ne++;
        if (ne > ns) {
            try {
                double val = std::stod(block.substr(ns, ne - ns));
                result[key] = val;
            } catch (...) {}
        }
        i = (ne > ns) ? ne : ns + 1;
    }
    return result;
}

RandomElementPicker::WeightLoadResult RandomElementPicker::load_weights_from_json(
    const std::string& path, const std::string& preset)
{
    WeightLoadResult r;

    std::ifstream f(path);
    if (!f) {
        r.error = "Cannot open weight file: " + path;
        return r;
    }

    std::ostringstream buf;
    buf << f.rdbuf();
    std::string json = buf.str();

    // Determine which block to parse
    std::unordered_map<std::string, double> wmap;
    if (!preset.empty()) {
        std::string presets_block = extract_block(json, "presets");
        if (presets_block.empty()) {
            r.error = "No 'presets' block found in " + path;
            return r;
        }
        std::string preset_block = extract_block(presets_block, preset);
        if (preset_block.empty()) {
            r.error = "Preset '" + preset + "' not found in " + path;
            return r;
        }
        wmap = parse_weight_block(preset_block);
    } else {
        std::string weights_block = extract_block(json, "weights");
        if (weights_block.empty()) {
            r.error = "No 'weights' block found in " + path;
            return r;
        }
        wmap = parse_weight_block(weights_block);
    }

    r.file_entries = wmap.size();

    // Match weights to current pool by symbol
    weights_.resize(pool_.size(), 0.0);
    for (size_t i = 0; i < pool_.size(); i++) {
        auto it = wmap.find(pool_[i].symbol);
        if (it != wmap.end()) {
            weights_[i] = it->second;
            r.matched++;
        } else {
            weights_[i] = 0.0;
            r.unmatched++;
        }
    }

    r.ok = true;
    return r;
}

// ============================================================================
// Mode 1: Uniform random
// ============================================================================

PickResult RandomElementPicker::pick_uniform() {
    PickResult result;
    result.mode = SelectionMode::UNIFORM;
    result.seed_used = seed_;

    if (pool_.empty()) {
        result.valid = false;
        return result;
    }

    std::uniform_int_distribution<size_t> dist(0, pool_.size() - 1);
    size_t idx = dist(rng_);

    result.atomic_number = pool_[idx].atomic_number;
    result.symbol        = pool_[idx].symbol;
    result.valid         = true;
    result.attempt_count = 1;
    total_picks_++;

    return result;
}

// ============================================================================
// Mode 2: Weighted random
// ============================================================================

PickResult RandomElementPicker::pick_weighted() {
    PickResult result;
    result.mode = SelectionMode::WEIGHTED;
    result.seed_used = seed_;

    if (pool_.empty()) {
        result.valid = false;
        return result;
    }

    // If no weights or size mismatch, fall back to uniform
    if (weights_.empty() || weights_.size() != pool_.size()) {
        return pick_uniform();
    }

    // Build CDF
    std::vector<double> cdf(weights_.size());
    std::partial_sum(weights_.begin(), weights_.end(), cdf.begin());
    double total = cdf.back();

    if (total <= 0.0) {
        result.valid = false;
        return result;
    }

    std::uniform_real_distribution<double> dist(0.0, total);
    double r = dist(rng_);

    size_t idx = static_cast<size_t>(
        std::lower_bound(cdf.begin(), cdf.end(), r) - cdf.begin()
    );
    if (idx >= pool_.size()) idx = pool_.size() - 1;

    result.atomic_number = pool_[idx].atomic_number;
    result.symbol        = pool_[idx].symbol;
    result.valid         = true;
    result.attempt_count = 1;
    total_picks_++;

    return result;
}

// ============================================================================
// Mode 3: Rule-constrained random
// ============================================================================

PickResult RandomElementPicker::pick_constrained(Predicate pred, size_t max_attempts) {
    PickResult result;
    result.mode = SelectionMode::CONSTRAINED;
    result.seed_used = seed_;

    if (pool_.empty()) {
        result.valid = false;
        return result;
    }

    // Build filtered subpool
    std::vector<size_t> valid_indices;
    for (size_t i = 0; i < pool_.size(); i++) {
        if (pred(pool_[i])) {
            valid_indices.push_back(i);
        }
    }

    if (valid_indices.empty()) {
        result.valid = false;
        result.attempt_count = 0;
        return result;
    }

    // If we have weights, build sub-weights for valid indices
    if (!weights_.empty() && weights_.size() == pool_.size()) {
        std::vector<double> sub_weights;
        sub_weights.reserve(valid_indices.size());
        for (size_t i : valid_indices) {
            sub_weights.push_back(weights_[i]);
        }

        std::vector<double> cdf(sub_weights.size());
        std::partial_sum(sub_weights.begin(), sub_weights.end(), cdf.begin());
        double total = cdf.back();

        if (total > 0.0) {
            std::uniform_real_distribution<double> dist(0.0, total);
            double r = dist(rng_);
            size_t sub_idx = static_cast<size_t>(
                std::lower_bound(cdf.begin(), cdf.end(), r) - cdf.begin()
            );
            if (sub_idx >= valid_indices.size()) sub_idx = valid_indices.size() - 1;

            size_t pool_idx = valid_indices[sub_idx];
            result.atomic_number = pool_[pool_idx].atomic_number;
            result.symbol        = pool_[pool_idx].symbol;
            result.valid         = true;
            result.attempt_count = 1;
            total_picks_++;
            return result;
        }
    }

    // Uniform fallback within valid indices
    std::uniform_int_distribution<size_t> dist(0, valid_indices.size() - 1);
    size_t sub_idx = dist(rng_);
    size_t pool_idx = valid_indices[sub_idx];

    result.atomic_number = pool_[pool_idx].atomic_number;
    result.symbol        = pool_[pool_idx].symbol;
    result.valid         = true;
    result.attempt_count = 1;
    total_picks_++;

    return result;
}

// ============================================================================
// Generic pick
// ============================================================================

PickResult RandomElementPicker::pick(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::UNIFORM:     return pick_uniform();
        case SelectionMode::WEIGHTED:    return pick_weighted();
        case SelectionMode::CONSTRAINED: {
            // No predicate = accept all = effectively uniform
            return pick_uniform();
        }
    }
    return {};
}

// ============================================================================
// Statistics
// ============================================================================

RandomElementPicker::FrequencyStats
RandomElementPicker::sample_distribution(SelectionMode mode, size_t n) {
    FrequencyStats stats;
    std::unordered_map<int, size_t> counts;

    for (size_t i = 0; i < n; i++) {
        auto res = pick(mode);
        if (res.valid) {
            counts[res.atomic_number]++;
        }
    }

    stats.total_picks = n;
    for (const auto& [Z, count] : counts) {
        stats.atomic_numbers.push_back(Z);
        stats.counts.push_back(count);
    }

    // Sort by Z for stable output
    std::vector<size_t> order(stats.atomic_numbers.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return stats.atomic_numbers[a] < stats.atomic_numbers[b];
    });

    std::vector<int>    sorted_Z(order.size());
    std::vector<size_t> sorted_counts(order.size());
    for (size_t i = 0; i < order.size(); i++) {
        sorted_Z[i]      = stats.atomic_numbers[order[i]];
        sorted_counts[i]  = stats.counts[order[i]];
    }
    stats.atomic_numbers = std::move(sorted_Z);
    stats.counts         = std::move(sorted_counts);

    return stats;
}

// ============================================================================
// Internal helpers
// ============================================================================

ElementDescriptor RandomElementPicker::make_minimal_descriptor(int Z) const {
    ElementDescriptor d;
    d.atomic_number = Z;

    if (Z >= 1 && Z <= MAX_SYMBOL_Z) {
        d.symbol      = SYMBOLS[Z];
        d.atomic_mass = APPROX_MASS[Z];
    } else {
        d.symbol = "??";
        d.atomic_mass = 0.0;
    }

    d.radioactive = is_radioactive(Z);
    return d;
}

} // namespace vsepr
