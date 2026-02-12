/**
 * periodic_table_loader.hpp
 * ==========================
 * JSON-based loader for complete periodic table data
 * Efficient loading of all 102 elements with isotope support
 */

#ifndef VSEPR_PERIODIC_TABLE_LOADER_HPP
#define VSEPR_PERIODIC_TABLE_LOADER_HPP

#include "core/periodic_table_complete.hpp"
#include <string>
#include <fstream>

namespace vsepr {
namespace periodic {

/**
 * Load periodic table data from JSON file
 * @param json_path Path to periodic_table_102.json
 * @return true if successful, false otherwise
 */
bool load_periodic_table_from_json(const std::string& json_path,
                                   std::array<ElementData, 103>& elements);

/**
 * Parse element data from JSON string
 */
bool parse_element_json(const std::string& json_string,
                       std::array<ElementData, 103>& elements);

/**
 * Quick initialization with embedded compact data
 * Uses hardcoded essential data for all 102 elements
 * Lighter weight than full JSON parsing
 */
void init_periodic_table_compact(std::array<ElementData, 103>& elements);

} // namespace periodic
} // namespace vsepr

#endif // VSEPR_PERIODIC_TABLE_LOADER_HPP
