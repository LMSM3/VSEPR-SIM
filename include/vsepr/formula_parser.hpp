#pragma once
/**
 * @file formula_parser.hpp
 * @brief Namespaced chemical formula parser with robust error handling
 * 
 * This is a standalone, fully-namespaced parser for chemical formulas.
 * Supports standard chemical notation: H2O, C6H12O6, Ca(OH)2, etc.
 * 
 * Namespace: vsepr::formula
 * 
 * Features:
 * - Parentheses support: Ca(OH)2 → Ca1O2H2
 * - Multi-digit counts: C100H202
 * - Validation: checks for unknown elements
 * - Error messages: precise position and reason
 * - Header-only: easy to include
 * 
 * Example:
 *   using namespace vsepr::formula;
 *   auto pt = load_periodic_table("data/PeriodicTableJSON.json");
 *   auto composition = parse("H2O", pt);  // {1: 2, 8: 1}
 */

#include "pot/periodic_db.hpp"
#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <cctype>

namespace vsepr {
namespace formula {

/**
 * @brief Formula parsing result
 * 
 * Maps atomic number (Z) to count
 * Example: H2O → {1: 2, 8: 1}
 */
using Composition = std::map<int, int>;

/**
 * @brief Formula parsing exception with detailed error info
 */
class ParseError : public std::runtime_error {
public:
    size_t position;
    std::string formula;
    
    ParseError(const std::string& msg, const std::string& formula_str, size_t pos)
        : std::runtime_error(msg), position(pos), formula(formula_str) {}
        
    std::string detailed_message() const {
        std::ostringstream oss;
        oss << what() << "\n";
        oss << "Formula: " << formula << "\n";
        oss << "Position: " << position << "\n";
        oss << "         ";
        for (size_t i = 0; i < position && i < formula.size(); ++i) {
            oss << " ";
        }
        oss << "^";
        return oss.str();
    }
};

/**
 * @brief Internal parser state
 */
class FormulaParser {
private:
    std::string formula_;
    const PeriodicTable& periodic_table_;
    size_t pos_;
    
    char peek() const {
        return (pos_ < formula_.size()) ? formula_[pos_] : '\0';
    }
    
    char consume() {
        return (pos_ < formula_.size()) ? formula_[pos_++] : '\0';
    }
    
    void skip_whitespace() {
        while (pos_ < formula_.size() && std::isspace(formula_[pos_])) {
            ++pos_;
        }
    }
    
    void error(const std::string& msg) const {
        throw ParseError(msg, formula_, pos_);
    }
    
    /**
     * Parse element symbol: [A-Z][a-z]?
     */
    std::string parse_element() {
        skip_whitespace();
        
        if (!std::isupper(peek())) {
            error("Expected element symbol (uppercase letter)");
        }
        
        std::string symbol;
        symbol += consume();
        
        // Optional lowercase second letter
        if (std::islower(peek())) {
            symbol += consume();
        }
        
        return symbol;
    }
    
    /**
     * Parse integer count: [0-9]+
     */
    int parse_count() {
        skip_whitespace();
        
        if (!std::isdigit(peek())) {
            return 1;  // Default count
        }
        
        int count = 0;
        while (std::isdigit(peek())) {
            count = count * 10 + (consume() - '0');
            
            // Prevent integer overflow
            if (count > 999999) {
                error("Count too large (max 999999)");
            }
        }
        
        return count;
    }
    
    /**
     * Parse group: (<formula>)<count>
     */
    Composition parse_group() {
        if (peek() != '(') {
            error("Expected '(' for group");
        }
        consume();  // '('
        
        Composition group_comp;
        
        // Parse contents until ')'
        while (peek() != ')' && peek() != '\0') {
            std::string symbol = parse_element();
            int count = parse_count();
            
            // Look up element
            const Element* elem = periodic_table_.by_symbol(symbol);
            if (!elem) {
                error("Unknown element: " + symbol);
            }
            
            group_comp[elem->Z] += count;
        }
        
        if (peek() != ')') {
            error("Expected ')' to close group");
        }
        consume();  // ')'
        
        // Parse multiplier for the group
        int multiplier = parse_count();
        
        // Apply multiplier
        Composition result;
        for (const auto& [Z, count] : group_comp) {
            result[Z] = count * multiplier;
        }
        
        return result;
    }
    
    /**
     * Parse entire formula
     */
    Composition parse_all() {
        Composition total;
        
        pos_ = 0;
        
        while (pos_ < formula_.size()) {
            skip_whitespace();
            
            if (pos_ >= formula_.size()) break;
            
            char c = peek();
            
            if (c == '(') {
                // Parse group
                Composition group = parse_group();
                for (const auto& [Z, count] : group) {
                    total[Z] += count;
                }
            } else if (std::isupper(c)) {
                // Parse element
                std::string symbol = parse_element();
                int count = parse_count();
                
                // Look up element
                const Element* elem = periodic_table_.by_symbol(symbol);
                if (!elem) {
                    error("Unknown element: " + symbol);
                }
                
                total[elem->Z] += count;
            } else {
                error("Unexpected character: '" + std::string(1, c) + "'");
            }
        }
        
        return total;
    }
    
public:
    FormulaParser(const std::string& formula, const PeriodicTable& pt)
        : formula_(formula), periodic_table_(pt), pos_(0) {}
        
    Composition parse() {
        if (formula_.empty()) {
            throw ParseError("Empty formula", formula_, 0);
        }
        
        Composition result = parse_all();
        
        if (result.empty()) {
            throw ParseError("No atoms parsed", formula_, 0);
        }
        
        return result;
    }
};

/**
 * @brief Parse chemical formula into atomic composition
 * 
 * @param formula Chemical formula string (e.g., "H2O", "Ca(OH)2", "C6H12O6")
 * @param periodic_table Periodic table for element lookup
 * @return Composition map (Z → count)
 * @throws ParseError on invalid formula or unknown elements
 * 
 * Examples:
 *   parse("H2O", pt)      → {1: 2, 8: 1}
 *   parse("CH4", pt)      → {6: 1, 1: 4}
 *   parse("Ca(OH)2", pt)  → {20: 1, 8: 2, 1: 2}
 *   parse("C6H12O6", pt)  → {6: 6, 1: 12, 8: 6}
 */
inline Composition parse(const std::string& formula, const PeriodicTable& periodic_table) {
    FormulaParser parser(formula, periodic_table);
    return parser.parse();
}

/**
 * @brief Validate formula without parsing (check syntax only)
 * 
 * @param formula Formula string to validate
 * @param periodic_table Periodic table for element lookup
 * @return true if valid, false otherwise
 */
inline bool validate(const std::string& formula, const PeriodicTable& periodic_table) {
    try {
        parse(formula, periodic_table);
        return true;
    } catch (const ParseError&) {
        return false;
    }
}

/**
 * @brief Convert composition back to normalized formula string
 * 
 * @param composition Atomic composition (Z → count)
 * @param periodic_table Periodic table for Z → symbol lookup
 * @return Normalized formula string (sorted by Z)
 * 
 * Example:
 *   {1: 2, 8: 1} → "H2O"
 *   {6: 1, 1: 4} → "CH4"
 */
inline std::string to_formula(const Composition& composition, const PeriodicTable& periodic_table) {
    std::ostringstream oss;
    
    for (const auto& [Z, count] : composition) {
        const Element* elem = periodic_table.by_Z(Z);
        if (!elem) {
            throw std::runtime_error("Unknown Z=" + std::to_string(Z));
        }
        
        oss << elem->symbol;
        if (count > 1) {
            oss << count;
        }
    }
    
    return oss.str();
}

/**
 * @brief Get total atom count from composition
 */
inline int total_atoms(const Composition& composition) {
    int total = 0;
    for (const auto& [Z, count] : composition) {
        total += count;
    }
    return total;
}

/**
 * @brief Get molecular mass from composition (in amu)
 */
inline double molecular_mass(const Composition& composition, const PeriodicTable& periodic_table) {
    double mass = 0.0;
    for (const auto& [Z, count] : composition) {
        const Element* elem = periodic_table.by_Z(Z);
        if (!elem) {
            throw std::runtime_error("Unknown Z=" + std::to_string(Z));
        }
        mass += elem->atomic_mass * count;
    }
    return mass;
}

} // namespace formula
} // namespace vsepr
