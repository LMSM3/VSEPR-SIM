#include "vsepr/spec_parser.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>

namespace vsepr {

// ============================================================================
// Helper Functions
// ============================================================================

static void trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        trim(item);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

static bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && 
           str.compare(0, prefix.size(), prefix) == 0;
}

static double parse_double(const std::string& s) {
    try {
        return std::stod(s);
    } catch (...) {
        throw std::runtime_error("Failed to parse number: " + s);
    }
}

static int parse_int(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (...) {
        throw std::runtime_error("Failed to parse integer: " + s);
    }
}

// ============================================================================
// MixtureSpec Methods
// ============================================================================

void MixtureSpec::normalize() {
    if (percentages.empty()) return;
    
    double sum = 0.0;
    for (double p : percentages) {
        sum += p;
    }
    
    if (sum > 0.0) {
        for (double& p : percentages) {
            p = (p / sum) * 100.0;
        }
    }
}

// ============================================================================
// JSON Serialization
// ============================================================================

static std::string escape_json(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) 
                        << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

static std::string position_to_json(const PositionInitializer& pos) {
    if (std::holds_alternative<RandomPosition>(pos)) {
        return R"({"mode":"random"})";
    } else if (std::holds_alternative<FixedPosition>(pos)) {
        const auto& fp = std::get<FixedPosition>(pos);
        std::ostringstream oss;
        oss << R"({"mode":"fixed","x":)" << fp.x 
            << R"(,"y":)" << fp.y 
            << R"(,"z":)" << fp.z << "}";
        return oss.str();
    } else if (std::holds_alternative<SeededPosition>(pos)) {
        const auto& sp = std::get<SeededPosition>(pos);
        std::ostringstream oss;
        oss << R"({"mode":"seeded","seed":)" << sp.seed
            << R"(,"box":[)" << sp.box_x << "," << sp.box_y << "," << sp.box_z << "]}";
        return oss.str();
    }
    return "{}";
}

std::string to_json(const MoleculeSpec& spec) {
    std::ostringstream oss;
    oss << "{";
    oss << R"("formula":")" << escape_json(spec.formula) << "\"";
    
    if (spec.temperature.has_value()) {
        oss << R"(,"T":)" << spec.temperature.value();
    }
    
    if (spec.position.has_value()) {
        oss << R"(,"pos":)" << position_to_json(spec.position.value());
    }
    
    if (spec.count != 1) {
        oss << R"(,"count":)" << spec.count;
    }
    
    oss << "}";
    return oss.str();
}

std::string to_json(const SimulationSpec& spec) {
    std::ostringstream oss;
    
    if (spec.is_single_molecule() && spec.mixture.percentages.empty()) {
        // Simple single molecule
        return to_json(spec.get_single());
    } else {
        // Mixture format
        oss << "{";
        oss << R"("mixture":[)";
        
        for (size_t i = 0; i < spec.mixture.components.size(); ++i) {
            if (i > 0) oss << ",";
            oss << to_json(spec.mixture.components[i]);
        }
        
        oss << "]";
        
        if (!spec.mixture.percentages.empty()) {
            oss << R"(,"per":[)";
            for (size_t i = 0; i < spec.mixture.percentages.size(); ++i) {
                if (i > 0) oss << ",";
                oss << spec.mixture.percentages[i];
            }
            oss << "]";
        }
        
        oss << "}";
        return oss.str();
    }
}

// ============================================================================
// DSL Parser Implementation
// ============================================================================

class DSLParser {
private:
    std::string input;
    size_t pos;
    
    void skip_whitespace() {
        while (pos < input.size() && std::isspace(input[pos])) {
            ++pos;
        }
    }
    
    char peek() const {
        return (pos < input.size()) ? input[pos] : '\0';
    }
    
    char consume() {
        return (pos < input.size()) ? input[pos++] : '\0';
    }
    
    bool expect(char c) {
        if (peek() == c) {
            consume();
            return true;
        }
        return false;
    }
    
    std::string consume_until(const std::string& delims) {
        std::string result;
        while (pos < input.size()) {
            char c = peek();
            if (delims.find(c) != std::string::npos) {
                break;
            }
            result += consume();
        }
        return result;
    }
    
    std::string parse_formula() {
        skip_whitespace();
        std::string formula;
        
        // Formula is [A-Za-z0-9()]+
        while (pos < input.size()) {
            char c = peek();
            if (std::isalnum(c) || c == '(' || c == ')') {
                formula += consume();
            } else {
                break;
            }
        }
        
        if (formula.empty()) {
            throw std::runtime_error("Expected formula at position " + std::to_string(pos));
        }
        
        return formula;
    }
    
    double parse_number() {
        skip_whitespace();
        std::string num_str;
        
        // Handle negative sign
        if (peek() == '-') {
            num_str += consume();
        }
        
        // Parse digits and decimal point
        while (pos < input.size()) {
            char c = peek();
            if (std::isdigit(c) || c == '.') {
                num_str += consume();
            } else {
                break;
            }
        }
        
        return parse_double(num_str);
    }
    
    int parse_integer() {
        skip_whitespace();
        std::string num_str;
        
        while (pos < input.size()) {
            char c = peek();
            if (std::isdigit(c)) {
                num_str += consume();
            } else {
                break;
            }
        }
        
        return parse_int(num_str);
    }
    
    PositionInitializer parse_position_mode() {
        skip_whitespace();
        std::string mode = consume_until(":,}");
        trim(mode);
        
        if (mode == "random") {
            return RandomPosition{};
        } else if (mode == "fixed") {
            expect(':');
            double x = parse_number();
            expect(',');
            double y = parse_number();
            expect(',');
            double z = parse_number();
            return FixedPosition{x, y, z};
        } else if (mode == "seeded") {
            expect(':');
            int seed = parse_integer();
            expect(':');
            double bx = parse_number();
            expect(',');
            double by = parse_number();
            expect(',');
            double bz = parse_number();
            return SeededPosition{seed, bx, by, bz};
        } else {
            throw std::runtime_error("Unknown position mode: " + mode);
        }
    }
    
    void parse_modifier(MoleculeSpec& spec) {
        skip_whitespace();
        
        if (starts_with(input.substr(pos), "--T=")) {
            pos += 4;
            spec.temperature = parse_number();
        } else if (starts_with(input.substr(pos), "-n=")) {
            pos += 3;
            spec.count = parse_integer();
        } else if (starts_with(input.substr(pos), "-pos{")) {
            pos += 5;
            spec.position = parse_position_mode();
            skip_whitespace();
            if (!expect('}')) {
                throw std::runtime_error("Expected '}' after position spec");
            }
        }
    }
    
    MoleculeSpec parse_item() {
        skip_whitespace();
        MoleculeSpec spec;
        spec.formula = parse_formula();
        
        // Parse modifiers
        while (pos < input.size()) {
            skip_whitespace();
            char c = peek();
            
            // Stop at comma or end
            if (c == ',' || c == '\0') {
                break;
            }
            
            // Stop at -per{
            if (starts_with(input.substr(pos), "-per{")) {
                break;
            }
            
            if (c == '-') {
                parse_modifier(spec);
            } else {
                break;
            }
        }
        
        return spec;
    }
    
    std::vector<double> parse_percentages() {
        skip_whitespace();
        
        if (!starts_with(input.substr(pos), "-per{")) {
            return {};
        }
        
        pos += 5; // Skip "-per{"
        std::vector<double> percentages;
        
        while (pos < input.size()) {
            skip_whitespace();
            
            if (peek() == '}') {
                consume();
                break;
            }
            
            percentages.push_back(parse_number());
            
            skip_whitespace();
            if (peek() == ',') {
                consume();
            }
        }
        
        return percentages;
    }
    
public:
    explicit DSLParser(const std::string& input) : input(input), pos(0) {}
    
    SimulationSpec parse() {
        SimulationSpec result;
        
        // Parse items separated by commas
        while (pos < input.size()) {
            skip_whitespace();
            if (pos >= input.size()) break;
            
            // Check if we've hit the percentage block
            if (starts_with(input.substr(pos), "-per{")) {
                break;
            }
            
            result.mixture.components.push_back(parse_item());
            
            skip_whitespace();
            if (peek() == ',') {
                consume();
            }
        }
        
        // Parse optional percentage block
        result.mixture.percentages = parse_percentages();
        
        // Validate
        if (result.mixture.components.empty()) {
            throw std::runtime_error("No molecules specified");
        }
        
        if (!result.mixture.is_valid()) {
            throw std::runtime_error(
                "Percentage count (" + std::to_string(result.mixture.percentages.size()) + 
                ") doesn't match component count (" + 
                std::to_string(result.mixture.components.size()) + ")"
            );
        }
        
        return result;
    }
};

SimulationSpec parse_dsl(const std::string& dsl_string) {
    DSLParser parser(dsl_string);
    return parser.parse();
}

// ============================================================================
// JSON Deserialization (Basic Implementation)
// ============================================================================

SimulationSpec from_json(const std::string& json_str) {
    // TODO: Implement full JSON parser or integrate with third-party library
    // For now, this is a placeholder
    throw std::runtime_error("JSON parsing not yet implemented. Use parse_dsl() instead.");
}

// ============================================================================
// Run Plan Expansion
// ============================================================================

std::vector<RunPlanItem> expand_to_run_plan(
    const SimulationSpec& spec, 
    int total_molecules
) {
    std::vector<RunPlanItem> plan;
    
    if (spec.mixture.percentages.empty()) {
        // No percentages - just use counts as-is
        for (const auto& comp : spec.mixture.components) {
            RunPlanItem item;
            item.formula = comp.formula;
            item.count = comp.count;
            item.temperature = comp.temperature;
            item.position = comp.position;
            plan.push_back(item);
        }
    } else {
        // Use percentages to determine counts
        for (size_t i = 0; i < spec.mixture.components.size(); ++i) {
            const auto& comp = spec.mixture.components[i];
            double percentage = spec.mixture.percentages[i];
            
            RunPlanItem item;
            item.formula = comp.formula;
            item.count = static_cast<int>(std::round(total_molecules * percentage / 100.0));
            item.temperature = comp.temperature;
            item.position = comp.position;
            
            if (item.count > 0) {
                plan.push_back(item);
            }
        }
    }
    
    return plan;
}

// ============================================================================
// Pretty Printing
// ============================================================================

static std::string position_to_string(const PositionInitializer& pos) {
    if (std::holds_alternative<RandomPosition>(pos)) {
        return "random";
    } else if (std::holds_alternative<FixedPosition>(pos)) {
        const auto& fp = std::get<FixedPosition>(pos);
        std::ostringstream oss;
        oss << "fixed:" << fp.x << "," << fp.y << "," << fp.z;
        return oss.str();
    } else if (std::holds_alternative<SeededPosition>(pos)) {
        const auto& sp = std::get<SeededPosition>(pos);
        std::ostringstream oss;
        oss << "seeded:" << sp.seed << ":" 
            << sp.box_x << "," << sp.box_y << "," << sp.box_z;
        return oss.str();
    }
    return "none";
}

std::string to_string(const SimulationSpec& spec) {
    std::ostringstream oss;
    
    oss << "Simulation Specification:\n";
    oss << "  Components: " << spec.mixture.components.size() << "\n";
    
    for (size_t i = 0; i < spec.mixture.components.size(); ++i) {
        const auto& comp = spec.mixture.components[i];
        oss << "  [" << i << "] " << comp.formula;
        
        if (comp.temperature.has_value()) {
            oss << " (T=" << comp.temperature.value() << "K)";
        }
        
        if (comp.position.has_value()) {
            oss << " (pos=" << position_to_string(comp.position.value()) << ")";
        }
        
        if (comp.count != 1) {
            oss << " (n=" << comp.count << ")";
        }
        
        if (!spec.mixture.percentages.empty()) {
            oss << " [" << spec.mixture.percentages[i] << "%]";
        }
        
        oss << "\n";
    }
    
    return oss.str();
}

} // namespace vsepr
