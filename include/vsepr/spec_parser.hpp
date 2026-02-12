#ifndef VSEPR_SPEC_PARSER_HPP
#define VSEPR_SPEC_PARSER_HPP

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <memory>

namespace vsepr {

// ============================================================================
// Position Initializers
// ============================================================================

struct RandomPosition {
    // Random positions within simulation box
};

struct FixedPosition {
    double x, y, z;
};

struct SeededPosition {
    int seed;
    double box_x, box_y, box_z;
};

using PositionInitializer = std::variant<RandomPosition, FixedPosition, SeededPosition>;

// ============================================================================
// Canonical Object Model
// ============================================================================

/**
 * @brief Single molecule specification
 * 
 * Represents a single molecule or component in a simulation:
 * - formula: chemical formula (e.g., "H2O", "CH12CaO9")
 * - T: temperature in Kelvin (optional)
 * - pos: position initializer (optional)
 * - count: number of copies (default 1)
 */
struct MoleculeSpec {
    std::string formula;                              // Required: e.g., "H2O", "CO2"
    std::optional<double> temperature;                 // Optional: Kelvin
    std::optional<PositionInitializer> position;       // Optional: where to place
    int count = 1;                                     // Optional: how many copies
    
    MoleculeSpec() = default;
    explicit MoleculeSpec(const std::string& f) : formula(f) {}
};

/**
 * @brief Mixture specification
 * 
 * Represents a mixture of molecules with percentage weights:
 * - components: list of molecule specs
 * - percentages: weight/probability for each component (must match length)
 * 
 * Rule: percentages[i] corresponds to components[i]
 * Sum should be ~100 (or will be normalized)
 */
struct MixtureSpec {
    std::vector<MoleculeSpec> components;
    std::vector<double> percentages;  // Optional; if empty, assume equal weights
    
    // Validate that percentages match components
    bool is_valid() const {
        return percentages.empty() || percentages.size() == components.size();
    }
    
    // Normalize percentages to sum to 100
    void normalize();
};

/**
 * @brief Top-level simulation specification
 * 
 * Can represent either:
 * - A single molecule (if mixture.components.size() == 1)
 * - A mixture (if mixture.components.size() > 1)
 */
struct SimulationSpec {
    MixtureSpec mixture;
    
    bool is_single_molecule() const {
        return mixture.components.size() == 1;
    }
    
    const MoleculeSpec& get_single() const {
        return mixture.components[0];
    }
};

// ============================================================================
// JSON Serialization/Deserialization
// ============================================================================

/**
 * @brief Convert SimulationSpec to JSON string
 */
std::string to_json(const SimulationSpec& spec);

/**
 * @brief Convert MoleculeSpec to JSON string
 */
std::string to_json(const MoleculeSpec& spec);

/**
 * @brief Parse JSON string to SimulationSpec
 */
SimulationSpec from_json(const std::string& json_str);

// ============================================================================
// DSL Parser
// ============================================================================

/**
 * @brief Parse DSL spec string to SimulationSpec
 * 
 * Grammar:
 *   spec        := item ("," item)* (WS per_block)?
 *   item        := formula (WS modifier)*
 *   modifier    := temp | pos | count
 *   temp        := "--T=" number
 *   count       := "-n=" integer
 *   pos         := "-pos{" pos_mode "}"
 *   pos_mode    := "random" | "fixed:" vec3 | "seeded:" integer ":" box
 *   vec3        := number "," number "," number
 *   box         := number "," number "," number
 *   per_block   := "-per{" number ("," number)* "}"
 *   formula     := [A-Za-z0-9()]+
 * 
 * Examples:
 *   "CH12CaO9"
 *   "H2O, H2O --T=289, CO2 -pos{random} -per{80,16.7,3.3}"
 *   "H2O -n=100"
 *   "CO2 -pos{fixed:0,0,0} --T=300"
 * 
 * @param dsl_string The DSL specification string
 * @return SimulationSpec The parsed specification
 * @throws std::runtime_error if parsing fails
 */
SimulationSpec parse_dsl(const std::string& dsl_string);

// ============================================================================
// Utilities
// ============================================================================

/**
 * @brief Expand a mixture into individual runs
 * 
 * Based on percentages and counts, determine how many of each component
 * to create in a batch simulation.
 * 
 * @param spec The simulation specification
 * @param total_molecules Total number of molecules to generate
 * @return Vector of (formula, count, temperature, position) tuples
 */
struct RunPlanItem {
    std::string formula;
    int count;
    std::optional<double> temperature;
    std::optional<PositionInitializer> position;
};

std::vector<RunPlanItem> expand_to_run_plan(
    const SimulationSpec& spec, 
    int total_molecules = 100
);

/**
 * @brief Pretty-print a SimulationSpec
 */
std::string to_string(const SimulationSpec& spec);

} // namespace vsepr

#endif // VSEPR_SPEC_PARSER_HPP
