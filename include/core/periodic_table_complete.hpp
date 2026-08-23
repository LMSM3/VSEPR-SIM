/**
 * periodic_table_complete.hpp
 * ============================
 * Complete periodic table data for elements Z=1 to Z=102 (Hydrogen to Nobelium)
 * Includes isotope data, atomic properties, and visualization information
 * 
 * Features:
 * - All 102 elements with full data
 * - Isotope masses (most common and stable isotopes)
 * - CPK and Jmol color schemes (RGB)
 * - Van der Waals and covalent radii
 * - Electronegativity, ionization energy, electron affinity
 * - Oxidation states and coordination numbers
 * - Atomic masses (standard and common isotopes)
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * Version: 2.3.1
 */

#ifndef VSEPR_PERIODIC_TABLE_COMPLETE_HPP
#define VSEPR_PERIODIC_TABLE_COMPLETE_HPP

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace vsepr {
namespace periodic {

// ============================================================================
// Isotope Data
// ============================================================================

struct IsotopeData {
    uint16_t mass_number;    // A (nucleons)
    double atomic_mass;      // amu (unified atomic mass units)
    double abundance;        // % natural abundance (0-100)
    bool is_stable;          // Radioactive or stable
    double half_life_years;  // Half-life in years (0 = stable)
};

// ============================================================================
// Element Data Structure
// ============================================================================

struct ElementData {
    // Basic identification
    uint8_t atomic_number;              // Z
    std::string symbol;                 // Chemical symbol (H, He, Li, ...)
    std::string name;                   // Full name
    double standard_atomic_weight;      // Standard atomic weight (amu)
    
    // Isotope information
    std::vector<IsotopeData> isotopes;  // Common isotopes
    uint16_t most_common_isotope;       // A of most common isotope
    
    // Physical properties
    double covalent_radius_single;      // Å (single bond)
    double covalent_radius_double;      // Å (double bond)
    double covalent_radius_triple;      // Å (triple bond)
    double van_der_waals_radius;        // Å
    
    // Chemical properties
    double electronegativity_pauling;   // Pauling scale
    double ionization_energy_1st;       // eV (first ionization)
    double electron_affinity;           // eV
    std::vector<int> oxidation_states;  // Common oxidation states
    std::vector<int> coordination_nums; // Common coordination numbers
    
    // Electronic configuration
    std::string electron_config;        // e.g. "[He] 2s1" for Li
    uint8_t valence_electrons;          // Valence shell electrons
    uint8_t period;                     // Period (1-7)
    uint8_t group;                      // Group (1-18)
    
    // Visualization (CPK colors - RGB 0.0-1.0)
    float cpk_color_r;
    float cpk_color_g;
    float cpk_color_b;
    std::string cpk_hex;                // Hex color code
    
    // Jmol colors (alternative scheme - RGB 0.0-1.0)
    float jmol_color_r;
    float jmol_color_g;
    float jmol_color_b;
    std::string jmol_hex;
    
    // Element category
    std::string category;               // Alkali, Noble gas, etc.
    
    // Physical state at STP
    std::string state_at_stp;           // Solid, Liquid, Gas
    
    // Melting and boiling points (K)
    double melting_point_k;
    double boiling_point_k;
    
    // Density (g/cm³ at STP)
    double density;
};

// ============================================================================
// Periodic Table Database
// ============================================================================

class PeriodicTableComplete {
public:
    PeriodicTableComplete();
    
    // Access by atomic number (1-102)
    const ElementData& operator[](uint8_t Z) const;
    const ElementData& get_element(uint8_t Z) const;
    
    // Access by symbol
    const ElementData* get_element(const std::string& symbol) const;
    uint8_t get_atomic_number(const std::string& symbol) const;
    
    // Isotope queries
    double get_isotope_mass(uint8_t Z, uint16_t mass_number) const;
    double get_most_common_isotope_mass(uint8_t Z) const;
    const std::vector<IsotopeData>& get_isotopes(uint8_t Z) const;
    
    // Color queries (for rendering)
    void get_cpk_color(uint8_t Z, float& r, float& g, float& b) const;
    void get_jmol_color(uint8_t Z, float& r, float& g, float& b) const;
    std::string get_cpk_hex(uint8_t Z) const;
    std::string get_jmol_hex(uint8_t Z) const;
    
    // Radius queries
    double get_covalent_radius(uint8_t Z, int bond_order = 1) const;
    double get_vdw_radius(uint8_t Z) const;
    
    // Chemical property queries
    double get_electronegativity(uint8_t Z) const;
    double get_ionization_energy(uint8_t Z) const;
    const std::vector<int>& get_oxidation_states(uint8_t Z) const;
    int get_max_coordination(uint8_t Z) const;
    
    // Category queries
    std::string get_category(uint8_t Z) const;
    bool is_metal(uint8_t Z) const;
    bool is_nonmetal(uint8_t Z) const;
    bool is_metalloid(uint8_t Z) const;
    bool is_transition_metal(uint8_t Z) const;
    bool is_lanthanide(uint8_t Z) const;
    bool is_actinide(uint8_t Z) const;
    bool is_halogen(uint8_t Z) const;
    bool is_noble_gas(uint8_t Z) const;
    
    // Physical state
    std::string get_state_at_stp(uint8_t Z) const;
    
    // Utility
    size_t element_count() const { return 102; }
    bool is_valid_Z(uint8_t Z) const { return Z >= 1 && Z <= 102; }
    
private:
    std::array<ElementData, 103> elements_;  // Index 0 unused, 1-102 for elements
    
    void init_hydrogen_helium();
    void init_period_2();   // Li-Ne
    void init_period_3();   // Na-Ar
    void init_period_4();   // K-Kr
    void init_period_5();   // Rb-Xe
    void init_period_6();   // Cs-Rn (includes lanthanides)
    void init_period_7();   // Fr-No (includes actinides)
};

// ============================================================================
// Global Accessor
// ============================================================================

// Singleton instance
const PeriodicTableComplete& get_periodic_table();

// Initialize (call once at startup)
void init_periodic_table();

// ============================================================================
// Isotope Helper Functions
// ============================================================================

// Calculate atomic mass from isotope composition
double calculate_weighted_mass(const std::vector<IsotopeData>& isotopes);

// Get natural abundance of specific isotope
double get_natural_abundance(uint8_t Z, uint16_t mass_number);

// Check if isotope exists
bool isotope_exists(uint8_t Z, uint16_t mass_number);

// ============================================================================
// Color Conversion Helpers
// ============================================================================

// RGB to Hex
std::string rgb_to_hex(float r, float g, float b);

// Hex to RGB
void hex_to_rgb(const std::string& hex, float& r, float& g, float& b);

// RGB to CMYK (for print export)
struct CMYK {
    float c, m, y, k;  // 0.0-1.0
};

CMYK rgb_to_cmyk(float r, float g, float b);

// CMYK to RGB
void cmyk_to_rgb(float c, float m, float y, float k, float& r, float& g, float& b);

} // namespace periodic
} // namespace vsepr

#endif // VSEPR_PERIODIC_TABLE_COMPLETE_HPP
