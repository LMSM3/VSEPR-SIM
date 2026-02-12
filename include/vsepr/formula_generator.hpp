#pragma once
/**
 * @file formula_generator.hpp
 * @brief Automated random chemical formula generation for testing
 * 
 * Generates valid random chemical formulas for:
 * - Fuzz testing the formula parser
 * - Stress testing molecule builders
 * - Automated validation pipelines
 * - Property testing
 * 
 * Namespace: vsepr::formula
 */

#include "pot/periodic_db.hpp"
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <algorithm>

namespace vsepr {
namespace formula {

/**
 * @brief Configuration for random formula generation
 */
struct GeneratorConfig {
    int min_elements = 1;      // Minimum number of distinct elements
    int max_elements = 4;      // Maximum number of distinct elements
    int min_total_atoms = 2;   // Minimum total atoms
    int max_total_atoms = 20;  // Maximum total atoms
    int min_count = 1;         // Minimum count per element
    int max_count = 10;        // Maximum count per element
    
    bool allow_metals = true;
    bool allow_nonmetals = true;
    bool allow_noble_gases = false;
    bool allow_hydrogen = true;
    
    // Element Z ranges
    int min_Z = 1;
    int max_Z = 36;  // Up to Kr
    
    // Parentheses (future extension)
    bool use_parentheses = false;
    double parentheses_probability = 0.0;
};

/**
 * @brief Random chemical formula generator
 */
class FormulaGenerator {
private:
    std::mt19937 rng_;
    const PeriodicTable& periodic_table_;
    GeneratorConfig config_;
    
    /**
     * Get list of allowed elements based on config
     */
    std::vector<int> get_allowed_elements() const {
        std::vector<int> allowed;
        
        for (int Z = config_.min_Z; Z <= config_.max_Z; ++Z) {
            const Element* elem = periodic_table_.by_Z(Z);
            if (!elem) continue;
            
            // Filter by properties
            if (Z == 1 && !config_.allow_hydrogen) continue;
            
            // Skip noble gases if not allowed
            if (!config_.allow_noble_gases) {
                if (Z == 2 || Z == 10 || Z == 18 || Z == 36 || Z == 54 || Z == 86) {
                    continue;
                }
            }
            
            // Basic metal/nonmetal classification (simplified)
            bool is_metal = (Z >= 3 && Z <= 4)   // Li, Be
                         || (Z >= 11 && Z <= 12)  // Na, Mg
                         || (Z >= 13 && Z <= 13)  // Al
                         || (Z >= 19 && Z <= 31)  // K-Ga
                         || (Z >= 37);             // Rb and beyond
            
            if (is_metal && !config_.allow_metals) continue;
            if (!is_metal && !config_.allow_nonmetals) continue;
            
            allowed.push_back(Z);
        }
        
        return allowed;
    }
    
    /**
     * Generate random element from allowed list
     */
    int random_element(const std::vector<int>& allowed) {
        if (allowed.empty()) {
            throw std::runtime_error("No allowed elements in config");
        }
        std::uniform_int_distribution<size_t> dist(0, allowed.size() - 1);
        return allowed[dist(rng_)];
    }
    
    /**
     * Generate random count for an element
     */
    int random_count() {
        std::uniform_int_distribution<int> dist(config_.min_count, config_.max_count);
        return dist(rng_);
    }
    
public:
    FormulaGenerator(const PeriodicTable& pt, unsigned seed = std::random_device{}())
        : rng_(seed), periodic_table_(pt), config_() {}
        
    FormulaGenerator(const PeriodicTable& pt, const GeneratorConfig& cfg, unsigned seed = std::random_device{}())
        : rng_(seed), periodic_table_(pt), config_(cfg) {}
    
    /**
     * @brief Generate a single random formula
     */
    std::string generate() {
        auto allowed = get_allowed_elements();
        
        // Decide how many distinct elements
        std::uniform_int_distribution<int> elem_dist(config_.min_elements, config_.max_elements);
        int num_elements = elem_dist(rng_);
        num_elements = std::min(num_elements, static_cast<int>(allowed.size()));
        
        // Select distinct elements
        std::vector<int> selected;
        std::vector<int> temp_allowed = allowed;
        
        for (int i = 0; i < num_elements && !temp_allowed.empty(); ++i) {
            std::uniform_int_distribution<size_t> pick(0, temp_allowed.size() - 1);
            size_t idx = pick(rng_);
            selected.push_back(temp_allowed[idx]);
            temp_allowed.erase(temp_allowed.begin() + idx);
        }
        
        // Sort by Z (standard formula convention: C before H, etc.)
        std::sort(selected.begin(), selected.end());
        
        // Generate counts ensuring we meet min/max total atoms
        std::map<int, int> composition;
        int total = 0;
        
        for (size_t i = 0; i < selected.size(); ++i) {
            int count;
            if (i == selected.size() - 1) {
                // Last element - ensure we meet minimum
                int remaining = config_.min_total_atoms - total;
                if (remaining > 0) {
                    count = remaining;
                } else {
                    count = random_count();
                }
            } else {
                count = random_count();
            }
            
            // Cap to not exceed max total
            if (total + count > config_.max_total_atoms) {
                count = config_.max_total_atoms - total;
            }
            
            if (count > 0) {
                composition[selected[i]] = count;
                total += count;
            }
            
            if (total >= config_.max_total_atoms) break;
        }
        
        // Build formula string
        std::ostringstream oss;
        for (const auto& [Z, count] : composition) {
            const Element* elem = periodic_table_.by_Z(Z);
            if (!elem) continue;
            
            oss << elem->symbol;
            if (count > 1) {
                oss << count;
            }
        }
        
        return oss.str();
    }
    
    /**
     * @brief Generate multiple random formulas
     */
    std::vector<std::string> generate_batch(int count) {
        std::vector<std::string> formulas;
        formulas.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            formulas.push_back(generate());
        }
        
        return formulas;
    }
    
    /**
     * @brief Generate organic-like formulas (C, H, O, N)
     */
    std::string generate_organic() {
        GeneratorConfig org_config;
        org_config.min_elements = 2;
        org_config.max_elements = 4;
        org_config.min_total_atoms = 3;
        org_config.max_total_atoms = 30;
        org_config.min_Z = 1;
        org_config.max_Z = 8;
        org_config.allow_metals = false;
        org_config.allow_noble_gases = false;
        
        GeneratorConfig old_config = config_;
        config_ = org_config;
        
        std::string formula = generate();
        
        config_ = old_config;
        return formula;
    }
    
    /**
     * @brief Generate inorganic salt-like formulas
     */
    std::string generate_salt() {
        GeneratorConfig salt_config;
        salt_config.min_elements = 2;
        salt_config.max_elements = 3;
        salt_config.min_total_atoms = 2;
        salt_config.max_total_atoms = 10;
        salt_config.min_Z = 1;
        salt_config.max_Z = 20;
        salt_config.allow_metals = true;
        salt_config.allow_nonmetals = true;
        
        GeneratorConfig old_config = config_;
        config_ = salt_config;
        
        std::string formula = generate();
        
        config_ = old_config;
        return formula;
    }
    
    /**
     * @brief Generate hydrate formulas (contains H and O)
     */
    std::string generate_hydrate() {
        auto formula = generate();
        
        // Ensure it contains H and O
        if (formula.find('H') == std::string::npos || formula.find('O') == std::string::npos) {
            // Add H2O to the formula
            formula += "H2O";
        }
        
        return formula;
    }
};

/**
 * @brief Predefined formula categories for testing
 */
namespace categories {

inline std::vector<std::string> simple_molecules() {
    return {
        "H2", "O2", "N2", "F2", "Cl2",
        "H2O", "CO2", "NH3", "CH4", "HCl",
        "H2O2", "N2O", "SO2", "NO2"
    };
}

inline std::vector<std::string> organic_molecules() {
    return {
        "CH4", "C2H6", "C3H8", "C4H10", "C5H12",
        "C6H6", "C6H12", "C6H14",
        "CH3OH", "C2H5OH", "C3H7OH",
        "CH2O", "C2H4O", "C3H6O",
        "C6H12O6", "C12H22O11"
    };
}

inline std::vector<std::string> inorganic_salts() {
    return {
        "NaCl", "KCl", "CaCl2", "MgCl2",
        "Na2SO4", "K2SO4", "CaSO4",
        "NaOH", "KOH", "Ca(OH)2", "Mg(OH)2",
        "HNO3", "H2SO4", "H3PO4"
    };
}

inline std::vector<std::string> complex_molecules() {
    return {
        "Ca(OH)2", "Mg(NO3)2", "Al(OH)3",
        "Ca3(PO4)2", "Fe2(SO4)3",
        "CH12CaO9",  // Ikaite (your example)
        "CaCO3", "MgCO3", "CaSO4",
        "Al2O3", "Fe2O3", "SiO2"
    };
}

inline std::vector<std::string> stress_test_formulas() {
    return {
        "H", "C", "O", "N",  // Single atoms
        "C100H202",  // Large counts
        "C10H22",    // Decane
        "C20H42",    // Eicosane
        "H2O10",     // Weird but valid
        "Fe2Cr3O12", // Complex oxide
        "Ca5(PO4)3OH"  // Hydroxyapatite
    };
}

} // namespace categories

} // namespace formula
} // namespace vsepr
