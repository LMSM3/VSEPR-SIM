#include "analysis_panel.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace vsepr {
namespace render {

AnalysisPanel::AnalysisPanel() = default;

void AnalysisPanel::update(const AtomicGeometry& geom,
                          float mouse_x, float mouse_y,
                          int screen_width, int screen_height,
                          const float* view_matrix,
                          const float* proj_matrix) {
    
    cached_geom_ = &geom;
    
    if (!tooltips_enabled_) {
        current_atom_ = std::nullopt;
        current_bond_ = std::nullopt;
        return;
    }
    
    // Pick closest object under mouse
    atom_is_closer_ = picker_.pick_closest(geom, mouse_x, mouse_y,
                                           screen_width, screen_height,
                                           view_matrix, proj_matrix,
                                           current_atom_, current_bond_);
}

void AnalysisPanel::render() {
    if (!tooltips_enabled_ || !cached_geom_) {
        return;
    }
    
    // Render tooltip based on what's hovered
    if (atom_is_closer_ && current_atom_) {
        render_atom_tooltip(*cached_geom_, *current_atom_);
    } else if (current_bond_) {
        render_bond_tooltip(*cached_geom_, *current_bond_);
    }
}

// ============================================================================
// Atom Tooltip (Rich Information)
// ============================================================================

void AnalysisPanel::render_atom_tooltip(const AtomicGeometry& geom, const AtomPick& pick) {
    ImGui::BeginTooltip();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    int Z = pick.atomic_number;
    std::string symbol = get_element_symbol(Z);
    std::string name = get_element_name(Z);
    
    // ========================================================================
    // Header (Element Name + Symbol)
    // ========================================================================
    
    ImGui::PushStyleColor(ImGuiCol_Text, Windows11Theme::get_accent_color());
    ImGui::Text("%s (%s)", name.c_str(), symbol.c_str());
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // ========================================================================
    // Basic Properties
    // ========================================================================
    
    Windows11Theme::section_header("Properties");
    
    ImGui::Text("Atomic Number:");
    ImGui::SameLine(150);
    ImGui::TextColored(Windows11Theme::get_accent_color(), "%d", Z);
    
    ImGui::Text("Atomic Mass:");
    ImGui::SameLine(150);
    ImGui::Text("%.2f u", get_element_mass(Z));
    
    ImGui::Text("Electronegativity:");
    ImGui::SameLine(150);
    float en = get_electronegativity(Z);
    if (en > 0) {
        ImGui::Text("%.2f (Pauling)", en);
    } else {
        ImGui::TextDisabled("N/A");
    }
    
    // ========================================================================
    // Geometry
    // ========================================================================
    
    Windows11Theme::section_header("Geometry");
    
    ImGui::Text("Position:");
    ImGui::SameLine(150);
    ImGui::Text("(%.2f, %.2f, %.2f) Å", 
                pick.position.x, pick.position.y, pick.position.z);
    
    ImGui::Text("vdW Radius:");
    ImGui::SameLine(150);
    ImGui::Text("%.2f Å", MoleculeRendererBase::get_vdw_radius(Z));
    
    ImGui::Text("Covalent Radius:");
    ImGui::SameLine(150);
    ImGui::Text("%.2f Å", MoleculeRendererBase::get_covalent_radius(Z));
    
    // ========================================================================
    // Bonding Environment
    // ========================================================================
    
    int neighbor_count = count_neighbors(geom, pick.atom_index);
    if (neighbor_count > 0) {
        Windows11Theme::section_header("Bonding");
        
        ImGui::Text("Coordination:");
        ImGui::SameLine(150);
        ImGui::TextColored(Windows11Theme::get_success_color(), "%d neighbors", neighbor_count);
        
        auto bonded = get_bonded_atoms(geom, pick.atom_index);
        if (!bonded.empty()) {
            ImGui::Text("Bonded to:");
            ImGui::Indent(20);
            for (int bonded_idx : bonded) {
                if (bonded_idx >= 0 && bonded_idx < (int)geom.atomic_numbers.size()) {
                    int bonded_Z = geom.atomic_numbers[bonded_idx];
                    std::string bonded_symbol = get_element_symbol(bonded_Z);
                    
                    Vec3 pos_i = geom.positions[pick.atom_index];
                    Vec3 pos_j = geom.positions[bonded_idx];
                    float dx = pos_j.x - pos_i.x;
                    float dy = pos_j.y - pos_i.y;
                    float dz = pos_j.z - pos_i.z;
                    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                    
                    ImGui::BulletText("%s (#%d) at %.2f Å", 
                                     bonded_symbol.c_str(), bonded_idx, dist);
                }
            }
            ImGui::Unindent(20);
        }
    }
    
    // ========================================================================
    // Additional Info (if available)
    // ========================================================================
    
    if (!geom.charges.empty() && pick.atom_index < (int)geom.charges.size()) {
        Windows11Theme::section_header("Electronic");
        
        ImGui::Text("Partial Charge:");
        ImGui::SameLine(150);
        float charge = geom.charges[pick.atom_index];
        if (charge > 0) {
            ImGui::TextColored(Windows11Theme::get_warning_color(), "+%.3f e", charge);
        } else if (charge < 0) {
            ImGui::TextColored(Windows11Theme::get_accent_color(), "%.3f e", charge);
        } else {
            ImGui::Text("%.3f e", charge);
        }
    }
    
    ImGui::PopStyleVar();
    ImGui::EndTooltip();
}

// ============================================================================
// Bond Tooltip (Simple - Just Length)
// ============================================================================

void AnalysisPanel::render_bond_tooltip(const AtomicGeometry& geom, const BondPick& pick) {
    ImGui::BeginTooltip();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    // Get atom symbols
    int Z1 = geom.atomic_numbers[pick.atom1];
    int Z2 = geom.atomic_numbers[pick.atom2];
    std::string symbol1 = get_element_symbol(Z1);
    std::string symbol2 = get_element_symbol(Z2);
    
    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, Windows11Theme::get_accent_color());
    ImGui::Text("%s—%s Bond", symbol1.c_str(), symbol2.c_str());
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Bond length (THE ONE NUMBER as requested)
    ImGui::Text("Bond Length:");
    ImGui::SameLine(120);
    ImGui::PushStyleColor(ImGuiCol_Text, Windows11Theme::get_accent_color());
    ImGui::Text("%.3f Å", pick.length);
    ImGui::PopStyleColor();
    
    // Atom indices
    ImGui::Spacing();
    ImGui::TextDisabled("Atoms: #%d ↔ #%d", pick.atom1, pick.atom2);
    
    ImGui::PopStyleVar();
    ImGui::EndTooltip();
}

// ============================================================================
// Utility Functions
// ============================================================================

int AnalysisPanel::count_neighbors(const AtomicGeometry& geom, int atom_index) const {
    int count = 0;
    for (const auto& bond : geom.bonds) {
        if (bond.first == atom_index || bond.second == atom_index) {
            count++;
        }
    }
    return count;
}

std::vector<int> AnalysisPanel::get_bonded_atoms(const AtomicGeometry& geom, int atom_index) const {
    std::vector<int> bonded;
    for (const auto& bond : geom.bonds) {
        if (bond.first == atom_index) {
            bonded.push_back(bond.second);
        } else if (bond.second == atom_index) {
            bonded.push_back(bond.first);
        }
    }
    return bonded;
}

// ============================================================================
// Element Data
// ============================================================================

std::string AnalysisPanel::get_element_symbol(int Z) const {
    static const char* symbols[] = {
        "??", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne",
        "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",
        "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
        "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y", "Zr",
        "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn",
        "Sb", "Te", "I", "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd",
        "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb",
        "Lu", "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt", "Au", "Hg",
        "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr", "Ra", "Ac", "Th",
        "Pa", "U", "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm",
        "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds",
        "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"
    };
    
    if (Z < 0 || Z > 118) return "??";
    return symbols[Z];
}

std::string AnalysisPanel::get_element_name(int Z) const {
    static const char* names[] = {
        "Unknown", "Hydrogen", "Helium", "Lithium", "Beryllium", "Boron",
        "Carbon", "Nitrogen", "Oxygen", "Fluorine", "Neon",
        "Sodium", "Magnesium", "Aluminum", "Silicon", "Phosphorus",
        "Sulfur", "Chlorine", "Argon", "Potassium", "Calcium",
        "Scandium", "Titanium", "Vanadium", "Chromium", "Manganese",
        "Iron", "Cobalt", "Nickel", "Copper", "Zinc",
        "Gallium", "Germanium", "Arsenic", "Selenium", "Bromine",
        "Krypton", "Rubidium", "Strontium", "Yttrium", "Zirconium",
        "Niobium", "Molybdenum", "Technetium", "Ruthenium", "Rhodium",
        "Palladium", "Silver", "Cadmium", "Indium", "Tin",
        "Antimony", "Tellurium", "Iodine", "Xenon", "Cesium",
        "Barium", "Lanthanum", "Cerium", "Praseodymium", "Neodymium",
        "Promethium", "Samarium", "Europium", "Gadolinium", "Terbium",
        "Dysprosium", "Holmium", "Erbium", "Thulium", "Ytterbium",
        "Lutetium", "Hafnium", "Tantalum", "Tungsten", "Rhenium",
        "Osmium", "Iridium", "Platinum", "Gold", "Mercury",
        "Thallium", "Lead", "Bismuth", "Polonium", "Astatine",
        "Radon", "Francium", "Radium", "Actinium", "Thorium",
        "Protactinium", "Uranium", "Neptunium", "Plutonium", "Americium",
        "Curium", "Berkelium", "Californium", "Einsteinium", "Fermium",
        "Mendelevium", "Nobelium", "Lawrencium", "Rutherfordium", "Dubnium",
        "Seaborgium", "Bohrium", "Hassium", "Meitnerium", "Darmstadtium",
        "Roentgenium", "Copernicium", "Nihonium", "Flerovium", "Moscovium",
        "Livermorium", "Tennessine", "Oganesson"
    };
    
    if (Z < 0 || Z > 118) return "Unknown";
    return names[Z];
}

float AnalysisPanel::get_element_mass(int Z) const {
    // Atomic masses (amu) for elements 1-118
    static const float masses[] = {
        0.0f,    1.008f,   4.003f,   6.941f,   9.012f,  10.811f,  12.011f,  14.007f,
        15.999f, 18.998f,  20.180f,  22.990f,  24.305f,  26.982f,  28.086f,  30.974f,
        32.065f, 35.453f,  39.948f,  39.098f,  40.078f,  44.956f,  47.867f,  50.942f,
        51.996f, 54.938f,  55.845f,  58.933f,  58.693f,  63.546f,  65.38f,   69.723f,
        72.64f,  74.922f,  78.96f,   79.904f,  83.798f,  85.468f,  87.62f,   88.906f,
        91.224f, 92.906f,  95.96f,   98.0f,   101.07f,  102.91f,  106.42f,  107.87f,
        112.41f, 114.82f, 118.71f,  121.76f,  127.60f,  126.90f,  131.29f,  132.91f,
        137.33f, 138.91f, 140.12f,  140.91f,  144.24f,  145.0f,   150.36f,  151.96f,
        157.25f, 158.93f, 162.50f,  164.93f,  167.26f,  168.93f,  173.05f,  174.97f,
        178.49f, 180.95f, 183.84f,  186.21f,  190.23f,  192.22f,  195.08f,  196.97f,
        200.59f, 204.38f, 207.2f,   208.98f,  209.0f,   210.0f,   222.0f,   223.0f,
        226.0f,  227.0f,  232.04f,  231.04f,  238.03f,  237.0f,   244.0f,   243.0f,
        247.0f,  247.0f,  251.0f,   252.0f,   257.0f,   258.0f,   259.0f,   262.0f,
        267.0f,  268.0f,  271.0f,   272.0f,   270.0f,   276.0f,   281.0f,   280.0f,
        285.0f,  284.0f,  289.0f,   288.0f,   293.0f,   294.0f,   294.0f
    };
    
    if (Z < 0 || Z > 118) return 0.0f;
    return masses[Z];
}

float AnalysisPanel::get_electronegativity(int Z) const {
    // Pauling electronegativity values
    static const float en[] = {
        0.0f,  2.20f, 0.0f,  0.98f, 1.57f, 2.04f, 2.55f, 3.04f, 3.44f, 3.98f, 0.0f,
        0.93f, 1.31f, 1.61f, 1.90f, 2.19f, 2.58f, 3.16f, 0.0f,  0.82f, 1.00f,
        1.36f, 1.54f, 1.63f, 1.66f, 1.55f, 1.83f, 1.88f, 1.91f, 1.90f, 1.65f,
        1.81f, 2.01f, 2.18f, 2.55f, 2.96f, 3.00f, 0.82f, 0.95f, 1.22f, 1.33f,
        1.6f,  2.16f, 1.9f,  2.2f,  2.28f, 2.20f, 1.93f, 1.69f, 1.78f, 1.96f,
        2.05f, 2.1f,  2.66f, 2.6f,  0.79f, 0.89f, 1.10f, 1.12f, 1.13f, 1.14f,
        0.0f,  1.17f, 0.0f,  1.20f, 0.0f,  1.22f, 1.23f, 1.24f, 1.25f, 0.0f,
        1.27f, 1.3f,  1.5f,  2.36f, 1.9f,  2.2f,  2.20f, 2.28f, 2.54f, 2.00f,
        1.62f, 2.33f, 2.02f, 2.0f,  2.2f,  0.0f,  0.7f,  0.9f,  1.1f,  1.3f,
        1.5f,  1.38f, 1.36f, 1.28f, 1.3f,  1.3f,  1.3f,  1.3f,  1.3f,  1.3f,
        1.3f,  1.3f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f
    };
    
    if (Z < 0 || Z > 118) return 0.0f;
    return en[Z];
}

} // namespace render
} // namespace vsepr
