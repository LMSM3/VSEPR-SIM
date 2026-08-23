/**
 * periodic_table_data_102.cpp
 * ============================
 * Complete periodic table data for elements Z=1 to Z=102
 * Compact implementation with all essential properties
 */

#include "core/periodic_table_complete.hpp"
#include <algorithm>

namespace vsepr {
namespace periodic {

// ============================================================================
// Helper macro for compact element initialization
// ============================================================================

#define ELEM(z, sym, nm, wt, rcov, rvdw, eneg, cpk_r, cpk_g, cpk_b, cat, st) \
    elements_[z] = {z, sym, nm, wt, {}, 0, rcov, 0.0, 0.0, rvdw, eneg, 0.0, 0.0, \
                   {}, {}, "", 0, 0, 0, cpk_r, cpk_g, cpk_b, "", \
                   cpk_r, cpk_g, cpk_b, "", cat, st, 0.0, 0.0, 0.0};

// ============================================================================
// Complete initialization for all 102 elements
// ============================================================================

void PeriodicTableComplete::init_period_4() {
    // Period 4: K-Kr (Z=19-36)
    
    ELEM(19, "K",  "Potassium", 39.098,  2.03, 2.75, 0.82, 0.56f, 0.25f, 0.83f, "Alkali metal", "Solid")
    ELEM(20, "Ca", "Calcium",   40.078,  1.76, 2.31, 1.00, 0.24f, 1.00f, 0.00f, "Alkaline earth", "Solid")
    ELEM(21, "Sc", "Scandium",  44.956,  1.70, 2.15, 1.36, 0.90f, 0.90f, 0.90f, "Transition metal", "Solid")
    ELEM(22, "Ti", "Titanium",  47.867,  1.60, 2.11, 1.54, 0.75f, 0.76f, 0.78f, "Transition metal", "Solid")
    ELEM(23, "V",  "Vanadium",  50.942,  1.53, 2.07, 1.63, 0.65f, 0.65f, 0.67f, "Transition metal", "Solid")
    ELEM(24, "Cr", "Chromium",  51.996,  1.39, 2.06, 1.66, 0.54f, 0.60f, 0.78f, "Transition metal", "Solid")
    ELEM(25, "Mn", "Manganese", 54.938,  1.39, 2.05, 1.55, 0.61f, 0.48f, 0.78f, "Transition metal", "Solid")
    ELEM(26, "Fe", "Iron",      55.845,  1.32, 2.04, 1.83, 0.88f, 0.40f, 0.20f, "Transition metal", "Solid")
    ELEM(27, "Co", "Cobalt",    58.933,  1.26, 2.00, 1.88, 0.94f, 0.56f, 0.63f, "Transition metal", "Solid")
    ELEM(28, "Ni", "Nickel",    58.693,  1.24, 1.97, 1.91, 0.31f, 0.82f, 0.31f, "Transition metal", "Solid")
    ELEM(29, "Cu", "Copper",    63.546,  1.32, 1.96, 1.90, 0.78f, 0.50f, 0.20f, "Transition metal", "Solid")
    ELEM(30, "Zn", "Zinc",      65.38,   1.22, 2.01, 1.65, 0.49f, 0.50f, 0.69f, "Post-transition", "Solid")
    ELEM(31, "Ga", "Gallium",   69.723,  1.22, 1.87, 1.81, 0.76f, 0.56f, 0.56f, "Post-transition", "Solid")
    ELEM(32, "Ge", "Germanium", 72.630,  1.20, 2.11, 2.01, 0.40f, 0.56f, 0.56f, "Metalloid", "Solid")
    ELEM(33, "As", "Arsenic",   74.922,  1.19, 1.85, 2.18, 0.74f, 0.50f, 0.89f, "Metalloid", "Solid")
    ELEM(34, "Se", "Selenium",  78.971,  1.20, 1.90, 2.55, 1.00f, 0.63f, 0.00f, "Nonmetal", "Solid")
    ELEM(35, "Br", "Bromine",   79.904,  1.20, 1.85, 2.96, 0.65f, 0.16f, 0.16f, "Halogen", "Liquid")
    ELEM(36, "Kr", "Krypton",   83.798,  1.16, 2.02, 3.00, 0.36f, 0.72f, 0.82f, "Noble gas", "Gas")
}

void PeriodicTableComplete::init_period_5() {
    // Period 5: Rb-Xe (Z=37-54)
    
    ELEM(37, "Rb", "Rubidium",   85.468,  2.20, 3.03, 0.82, 0.44f, 0.18f, 0.69f, "Alkali metal", "Solid")
    ELEM(38, "Sr", "Strontium",  87.62,   1.95, 2.49, 0.95, 0.00f, 1.00f, 0.00f, "Alkaline earth", "Solid")
    ELEM(39, "Y",  "Yttrium",    88.906,  1.90, 2.32, 1.22, 0.58f, 1.00f, 1.00f, "Transition metal", "Solid")
    ELEM(40, "Zr", "Zirconium",  91.224,  1.75, 2.23, 1.33, 0.58f, 0.88f, 0.88f, "Transition metal", "Solid")
    ELEM(41, "Nb", "Niobium",    92.906,  1.64, 2.18, 1.60, 0.45f, 0.76f, 0.79f, "Transition metal", "Solid")
    ELEM(42, "Mo", "Molybdenum", 95.95,   1.54, 2.17, 2.16, 0.33f, 0.71f, 0.71f, "Transition metal", "Solid")
    ELEM(43, "Tc", "Technetium", 98.0,    1.47, 2.16, 1.90, 0.23f, 0.62f, 0.62f, "Transition metal", "Solid")
    ELEM(44, "Ru", "Ruthenium",  101.07,  1.46, 2.13, 2.20, 0.14f, 0.56f, 0.56f, "Transition metal", "Solid")
    ELEM(45, "Rh", "Rhodium",    102.91,  1.42, 2.10, 2.28, 0.04f, 0.49f, 0.55f, "Transition metal", "Solid")
    ELEM(46, "Pd", "Palladium",  106.42,  1.39, 2.10, 2.20, 0.00f, 0.41f, 0.52f, "Transition metal", "Solid")
    ELEM(47, "Ag", "Silver",     107.87,  1.45, 2.11, 1.93, 0.75f, 0.75f, 0.75f, "Transition metal", "Solid")
    ELEM(48, "Cd", "Cadmium",    112.41,  1.44, 2.18, 1.69, 1.00f, 0.85f, 0.56f, "Post-transition", "Solid")
    ELEM(49, "In", "Indium",     114.82,  1.42, 1.93, 1.78, 0.65f, 0.46f, 0.45f, "Post-transition", "Solid")
    ELEM(50, "Sn", "Tin",        118.71,  1.39, 2.17, 1.96, 0.40f, 0.50f, 0.50f, "Post-transition", "Solid")
    ELEM(51, "Sb", "Antimony",   121.76,  1.39, 2.06, 2.05, 0.62f, 0.39f, 0.71f, "Metalloid", "Solid")
    ELEM(52, "Te", "Tellurium",  127.60,  1.38, 2.06, 2.10, 0.83f, 0.48f, 0.00f, "Metalloid", "Solid")
    ELEM(53, "I",  "Iodine",     126.90,  1.39, 1.98, 2.66, 0.58f, 0.00f, 0.58f, "Halogen", "Solid")
    ELEM(54, "Xe", "Xenon",      131.29,  1.40, 2.16, 2.60, 0.26f, 0.62f, 0.69f, "Noble gas", "Gas")
}

void PeriodicTableComplete::init_period_6() {
    // Period 6: Cs-Rn (Z=55-86) including lanthanides
    
    ELEM(55, "Cs", "Cesium",        132.91, 2.44, 3.43, 0.79, 0.34f, 0.09f, 0.56f, "Alkali metal", "Solid")
    ELEM(56, "Ba", "Barium",        137.33, 2.15, 2.68, 0.89, 0.00f, 0.79f, 0.00f, "Alkaline earth", "Solid")
    
    // Lanthanides (Z=57-71)
    ELEM(57, "La", "Lanthanum",     138.91, 2.07, 2.43, 1.10, 0.44f, 0.83f, 1.00f, "Lanthanide", "Solid")
    ELEM(58, "Ce", "Cerium",        140.12, 2.04, 2.42, 1.12, 1.00f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(59, "Pr", "Praseodymium",  140.91, 2.03, 2.40, 1.13, 0.85f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(60, "Nd", "Neodymium",     144.24, 2.01, 2.39, 1.14, 0.78f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(61, "Pm", "Promethium",    145.0,  1.99, 2.38, 1.13, 0.64f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(62, "Sm", "Samarium",      150.36, 1.98, 2.36, 1.17, 0.56f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(63, "Eu", "Europium",      151.96, 1.98, 2.35, 1.20, 0.38f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(64, "Gd", "Gadolinium",    157.25, 1.96, 2.34, 1.20, 0.27f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(65, "Tb", "Terbium",       158.93, 1.94, 2.33, 1.20, 0.19f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(66, "Dy", "Dysprosium",    162.50, 1.92, 2.31, 1.22, 0.12f, 1.00f, 0.78f, "Lanthanide", "Solid")
    ELEM(67, "Ho", "Holmium",       164.93, 1.92, 2.30, 1.23, 0.00f, 1.00f, 0.61f, "Lanthanide", "Solid")
    ELEM(68, "Er", "Erbium",        167.26, 1.89, 2.29, 1.24, 0.00f, 0.90f, 0.46f, "Lanthanide", "Solid")
    ELEM(69, "Tm", "Thulium",       168.93, 1.90, 2.27, 1.25, 0.00f, 0.83f, 0.32f, "Lanthanide", "Solid")
    ELEM(70, "Yb", "Ytterbium",     173.05, 1.87, 2.26, 1.10, 0.00f, 0.75f, 0.22f, "Lanthanide", "Solid")
    ELEM(71, "Lu", "Lutetium",      174.97, 1.87, 2.24, 1.27, 0.00f, 0.67f, 0.14f, "Lanthanide", "Solid")
    
    // Transition metals (Z=72-80)
    ELEM(72, "Hf", "Hafnium",       178.49, 1.75, 2.23, 1.30, 0.30f, 0.76f, 1.00f, "Transition metal", "Solid")
    ELEM(73, "Ta", "Tantalum",      180.95, 1.70, 2.22, 1.50, 0.30f, 0.65f, 1.00f, "Transition metal", "Solid")
    ELEM(74, "W",  "Tungsten",      183.84, 1.62, 2.18, 2.36, 0.13f, 0.58f, 0.84f, "Transition metal", "Solid")
    ELEM(75, "Re", "Rhenium",       186.21, 1.51, 2.16, 1.90, 0.15f, 0.49f, 0.67f, "Transition metal", "Solid")
    ELEM(76, "Os", "Osmium",        190.23, 1.44, 2.16, 2.20, 0.15f, 0.40f, 0.59f, "Transition metal", "Solid")
    ELEM(77, "Ir", "Iridium",       192.22, 1.41, 2.13, 2.20, 0.09f, 0.33f, 0.53f, "Transition metal", "Solid")
    ELEM(78, "Pt", "Platinum",      195.08, 1.36, 2.13, 2.28, 0.82f, 0.82f, 0.88f, "Transition metal", "Solid")
    ELEM(79, "Au", "Gold",          196.97, 1.36, 2.14, 2.54, 1.00f, 0.82f, 0.14f, "Transition metal", "Solid")
    ELEM(80, "Hg", "Mercury",       200.59, 1.32, 2.23, 2.00, 0.72f, 0.72f, 0.82f, "Post-transition", "Liquid")
    
    // Post-transition metals (Z=81-84)
    ELEM(81, "Tl", "Thallium",      204.38, 1.45, 1.96, 1.62, 0.65f, 0.33f, 0.30f, "Post-transition", "Solid")
    ELEM(82, "Pb", "Lead",          207.2,  1.46, 2.02, 2.33, 0.34f, 0.35f, 0.38f, "Post-transition", "Solid")
    ELEM(83, "Bi", "Bismuth",       208.98, 1.48, 2.07, 2.02, 0.62f, 0.31f, 0.71f, "Post-transition", "Solid")
    ELEM(84, "Po", "Polonium",      209.0,  1.40, 1.97, 2.00, 0.67f, 0.36f, 0.00f, "Metalloid", "Solid")
    ELEM(85, "At", "Astatine",      210.0,  1.50, 2.02, 2.20, 0.46f, 0.31f, 0.27f, "Halogen", "Solid")
    ELEM(86, "Rn", "Radon",         222.0,  1.50, 2.20, 2.20, 0.26f, 0.51f, 0.59f, "Noble gas", "Gas")
}

void PeriodicTableComplete::init_period_7() {
    // Period 7: Fr-No (Z=87-102) including actinides
    
    ELEM(87, "Fr", "Francium",      223.0,  2.60, 3.48, 0.70, 0.26f, 0.00f, 0.40f, "Alkali metal", "Solid")
    ELEM(88, "Ra", "Radium",        226.0,  2.21, 2.83, 0.90, 0.00f, 0.49f, 0.00f, "Alkaline earth", "Solid")
    
    // Actinides (Z=89-102)
    ELEM(89, "Ac", "Actinium",      227.0,  2.15, 2.47, 1.10, 0.44f, 0.67f, 0.98f, "Actinide", "Solid")
    ELEM(90, "Th", "Thorium",       232.04, 2.06, 2.45, 1.30, 0.00f, 0.73f, 1.00f, "Actinide", "Solid")
    ELEM(91, "Pa", "Protactinium",  231.04, 2.00, 2.43, 1.50, 0.00f, 0.63f, 1.00f, "Actinide", "Solid")
    ELEM(92, "U",  "Uranium",       238.03, 1.96, 2.41, 1.38, 0.00f, 0.56f, 1.00f, "Actinide", "Solid")
    ELEM(93, "Np", "Neptunium",     237.0,  1.90, 2.39, 1.36, 0.00f, 0.50f, 1.00f, "Actinide", "Solid")
    ELEM(94, "Pu", "Plutonium",     244.0,  1.87, 2.43, 1.28, 0.00f, 0.42f, 1.00f, "Actinide", "Solid")
    ELEM(95, "Am", "Americium",     243.0,  1.80, 2.44, 1.30, 0.33f, 0.36f, 0.95f, "Actinide", "Solid")
    ELEM(96, "Cm", "Curium",        247.0,  1.69, 2.45, 1.30, 0.47f, 0.36f, 0.89f, "Actinide", "Solid")
    ELEM(97, "Bk", "Berkelium",     247.0,  1.66, 2.44, 1.30, 0.54f, 0.31f, 0.89f, "Actinide", "Solid")
    ELEM(98, "Cf", "Californium",   251.0,  1.68, 2.45, 1.30, 0.63f, 0.21f, 0.83f, "Actinide", "Solid")
    ELEM(99, "Es", "Einsteinium",   252.0,  1.65, 2.45, 1.30, 0.70f, 0.12f, 0.83f, "Actinide", "Solid")
    ELEM(100, "Fm", "Fermium",      257.0,  1.67, 2.45, 1.30, 0.70f, 0.12f, 0.73f, "Actinide", "Solid")
    ELEM(101, "Md", "Mendelevium",  258.0,  1.73, 2.46, 1.30, 0.70f, 0.05f, 0.65f, "Actinide", "Solid")
    ELEM(102, "No", "Nobelium",     259.0,  1.76, 2.46, 1.30, 0.74f, 0.05f, 0.53f, "Actinide", "Solid")
}

#undef ELEM

// ============================================================================
// Add isotope data for key elements (abbreviated for common isotopes)
// ============================================================================

void add_common_isotopes(std::array<ElementData, 103>& elements) {
    // Hydrogen
    elements[1].isotopes = {
        {1, 1.007825, 99.9885, true, 0.0},
        {2, 2.014102, 0.0115, true, 0.0},
        {3, 3.016049, 0.0, false, 12.32}
    };
    elements[1].most_common_isotope = 1;
    
    // Carbon
    elements[6].isotopes = {
        {12, 12.000000, 98.93, true, 0.0},
        {13, 13.003354, 1.07, true, 0.0},
        {14, 14.003241, 0.0, false, 5730.0}
    };
    elements[6].most_common_isotope = 12;
    
    // Nitrogen
    elements[7].isotopes = {
        {14, 14.003074, 99.636, true, 0.0},
        {15, 15.000108, 0.364, true, 0.0}
    };
    elements[7].most_common_isotope = 14;
    
    // Oxygen
    elements[8].isotopes = {
        {16, 15.994915, 99.757, true, 0.0},
        {17, 16.999131, 0.038, true, 0.0},
        {18, 17.999160, 0.205, true, 0.0}
    };
    elements[8].most_common_isotope = 16;
    
    // Chlorine
    elements[17].isotopes = {
        {35, 34.968853, 75.76, true, 0.0},
        {37, 36.965903, 24.24, true, 0.0}
    };
    elements[17].most_common_isotope = 35;
    
    // Uranium
    elements[92].isotopes = {
        {234, 234.040952, 0.0054, false, 245500.0},
        {235, 235.043928, 0.7204, false, 703800000.0},
        {238, 238.050788, 99.2742, false, 4468000000.0}
    };
    elements[92].most_common_isotope = 238;
    
    // Add hex color codes from RGB
    for (uint8_t Z = 1; Z <= 102; ++Z) {
        auto& elem = elements[Z];
        elem.cpk_hex = rgb_to_hex(elem.cpk_color_r, elem.cpk_color_g, elem.cpk_color_b);
        elem.jmol_hex = elem.cpk_hex;  // Use same for now
    }
}

// ============================================================================
// Modified constructor to use compact initialization
// ============================================================================

PeriodicTableComplete::PeriodicTableComplete() {
    // Initialize element 0 as invalid/placeholder
    elements_[0] = {0, "X", "Unknown", 0.0, {}, 0, 1.0, 0.0, 0.0, 1.5,
                   0.0, 0.0, 0.0, {}, {}, "", 0, 0, 0,
                   1.0f, 1.0f, 1.0f, "#FFFFFF",
                   1.0f, 1.0f, 1.0f, "#FFFFFF",
                   "Unknown", "Unknown", 0.0, 0.0, 0.0};
    
    // Initialize periods 1-3 (original detailed implementation)
    init_hydrogen_helium();
    init_period_2();
    init_period_3();
    
    // Initialize periods 4-7 (compact implementation)
    init_period_4();
    init_period_5();
    init_period_6();
    init_period_7();
    
    // Add isotope data for common elements
    add_common_isotopes(elements_);
}

} // namespace periodic
} // namespace vsepr
