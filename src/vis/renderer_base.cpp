#include "renderer_base.hpp"
#include "renderer_classic.hpp"
#include <cmath>
#include <algorithm>
#include <array>

namespace vsepr {
namespace render {

// ============================================================================
// AtomicGeometry Factory Methods
// ============================================================================

AtomicGeometry AtomicGeometry::from_xyz(const std::vector<int>& Z,
                                       const std::vector<Vec3>& pos) {
    AtomicGeometry geom;
    geom.atomic_numbers = Z;
    geom.positions = pos;
    return geom;
}

AtomicGeometry AtomicGeometry::from_xyz_with_bonds(const std::vector<int>& Z,
                                                   const std::vector<Vec3>& pos,
                                                   const std::vector<std::pair<int,int>>& bonds) {
    AtomicGeometry geom;
    geom.atomic_numbers = Z;
    geom.positions = pos;
    geom.bonds = bonds;
    return geom;
}

// ============================================================================
// Chemistry Type Detection
// ============================================================================

ChemistryType MoleculeRendererBase::detect_chemistry_type(const AtomicGeometry& geom) {
    if (geom.atomic_numbers.empty()) {
        return ChemistryType::UNKNOWN;
    }
    
    int n_total = static_cast<int>(geom.atomic_numbers.size());
    int n_organic = 0;   // C, H, N, O, S, P
    int n_metallic = 0;  // Transition metals, lanthanides, actinides
    
    for (int Z : geom.atomic_numbers) {
        // Organic elements
        if (Z == 1 || Z == 6 || Z == 7 || Z == 8 || Z == 15 || Z == 16) {
            n_organic++;
        }
        
        // Transition metals (Sc-Zn, Y-Cd, La-Hg)
        if ((Z >= 21 && Z <= 30) ||   // 3d metals
            (Z >= 39 && Z <= 48) ||   // 4d metals
            (Z >= 57 && Z <= 80)) {   // Lanthanides + 5d metals + Hg
            n_metallic++;
        }
    }
    
    float organic_frac = static_cast<float>(n_organic) / n_total;
    float metallic_frac = static_cast<float>(n_metallic) / n_total;
    
    // Decision rules
    if (organic_frac > 0.5f) {
        if (metallic_frac > 0.05f) {
            return ChemistryType::MIXED;  // Metalloprotein
        }
        return ChemistryType::ORGANIC;
    }
    
    if (metallic_frac > 0.1f) {
        return ChemistryType::METALLIC;  // Coordination complex
    }
    
    return ChemistryType::CLASSIC;  // Main group
}

// ============================================================================
// CPK Colors (Corey-Pauling-Koltun) - Complete Periodic Table
// ============================================================================

// Get CPK color table (initialized once)
static const std::vector<std::array<float, 3>>& get_cpk_color_table() {
    static const std::vector<std::array<float, 3>> colors = {
        // CPK/Jmol color scheme for all 118 elements
        // Reference: http://jmol.sourceforge.net/jscolors/
        {1.00f, 0.08f, 0.58f}, // 0 - Unknown (magenta)
        {1.00f, 1.00f, 1.00f}, // 1 - H (white)
        {0.85f, 1.00f, 1.00f}, // 2 - He (cyan)
        {0.80f, 0.50f, 1.00f}, // 3 - Li (violet)
        {0.76f, 1.00f, 0.00f}, // 4 - Be (green)
        {1.00f, 0.71f, 0.71f}, // 5 - B (pink)
        {0.30f, 0.30f, 0.30f}, // 6 - C (gray)
        {0.05f, 0.05f, 1.00f}, // 7 - N (blue)
        {1.00f, 0.05f, 0.05f}, // 8 - O (red)
        {0.70f, 1.00f, 1.00f}, // 9 - F (light green)
        {0.70f, 0.89f, 0.96f}, // 10 - Ne (light blue)
        {0.67f, 0.36f, 0.95f}, // 11 - Na (blue)
        {0.54f, 1.00f, 0.00f}, // 12 - Mg (forest green)
        {0.75f, 0.65f, 0.65f}, // 13 - Al (gray)
        {0.94f, 0.78f, 0.63f}, // 14 - Si (tan)
        {1.00f, 0.50f, 0.00f}, // 15 - P (orange)
        {1.00f, 1.00f, 0.19f}, // 16 - S (yellow)
        {0.12f, 0.94f, 0.12f}, // 17 - Cl (green)
        {0.50f, 0.82f, 0.89f}, // 18 - Ar (cyan)
        {0.56f, 0.25f, 0.83f}, // 19 - K (violet)
        {0.24f, 1.00f, 0.00f}, // 20 - Ca (green)
        {0.90f, 0.90f, 0.90f}, // 21 - Sc (light gray)
        {0.75f, 0.76f, 0.78f}, // 22 - Ti (gray)
        {0.65f, 0.65f, 0.67f}, // 23 - V (gray)
        {0.54f, 0.60f, 0.78f}, // 24 - Cr (steel blue)
        {0.61f, 0.48f, 0.78f}, // 25 - Mn (purple)
        {0.88f, 0.40f, 0.20f}, // 26 - Fe (orange)
        {0.94f, 0.56f, 0.63f}, // 27 - Co (pink)
        {0.31f, 0.82f, 0.31f}, // 28 - Ni (green)
        {0.78f, 0.50f, 0.20f}, // 29 - Cu (copper)
        {0.49f, 0.50f, 0.69f}, // 30 - Zn (blue-gray)
        {0.76f, 0.56f, 0.56f}, // 31 - Ga (gray)
        {0.40f, 0.56f, 0.56f}, // 32 - Ge (gray)
        {0.74f, 0.50f, 0.89f}, // 33 - As (purple)
        {1.00f, 0.63f, 0.00f}, // 34 - Se (orange)
        {0.65f, 0.16f, 0.16f}, // 35 - Br (dark red)
        {0.36f, 0.72f, 0.82f}, // 36 - Kr (cyan)
        {0.44f, 0.18f, 0.69f}, // 37 - Rb (violet)
        {0.00f, 1.00f, 0.00f}, // 38 - Sr (green)
        {0.58f, 1.00f, 1.00f}, // 39 - Y (cyan)
        {0.58f, 0.88f, 0.88f}, // 40 - Zr (cyan)
        {0.45f, 0.76f, 0.79f}, // 41 - Nb (cyan)
        {0.33f, 0.71f, 0.71f}, // 42 - Mo (teal)
        {0.23f, 0.62f, 0.62f}, // 43 - Tc (teal)
        {0.14f, 0.56f, 0.56f}, // 44 - Ru (teal)
        {0.04f, 0.49f, 0.55f}, // 45 - Rh (dark teal)
        {0.00f, 0.41f, 0.52f}, // 46 - Pd (dark teal)
        {0.75f, 0.75f, 0.75f}, // 47 - Ag (silver)
        {1.00f, 0.85f, 0.56f}, // 48 - Cd (yellow)
        {0.65f, 0.46f, 0.45f}, // 49 - In (brown)
        {0.40f, 0.50f, 0.50f}, // 50 - Sn (gray)
        {0.62f, 0.39f, 0.71f}, // 51 - Sb (purple)
        {0.83f, 0.48f, 0.00f}, // 52 - Te (orange)
        {0.58f, 0.00f, 0.58f}, // 53 - I (purple)
        {0.26f, 0.62f, 0.69f}, // 54 - Xe (cyan)
        {0.34f, 0.09f, 0.56f}, // 55 - Cs (purple)
        {0.00f, 0.79f, 0.00f}, // 56 - Ba (green)
        {0.44f, 0.83f, 1.00f}, // 57 - La (cyan)
        {1.00f, 1.00f, 0.78f}, // 58 - Ce (light yellow)
        {0.85f, 1.00f, 0.78f}, // 59 - Pr (light green)
        {0.78f, 1.00f, 0.78f}, // 60 - Nd (green)
        {0.64f, 1.00f, 0.78f}, // 61 - Pm (green)
        {0.56f, 1.00f, 0.78f}, // 62 - Sm (green)
        {0.38f, 1.00f, 0.78f}, // 63 - Eu (green)
        {0.27f, 1.00f, 0.78f}, // 64 - Gd (green)
        {0.19f, 1.00f, 0.78f}, // 65 - Tb (green)
        {0.12f, 1.00f, 0.78f}, // 66 - Dy (green)
        {0.00f, 1.00f, 0.61f}, // 67 - Ho (green)
        {0.00f, 0.90f, 0.46f}, // 68 - Er (green)
        {0.00f, 0.83f, 0.32f}, // 69 - Tm (green)
        {0.00f, 0.75f, 0.22f}, // 70 - Yb (green)
        {0.00f, 0.67f, 0.14f}, // 71 - Lu (green)
        {0.30f, 0.76f, 1.00f}, // 72 - Hf (cyan)
        {0.30f, 0.65f, 1.00f}, // 73 - Ta (blue)
        {0.13f, 0.58f, 0.84f}, // 74 - W (blue)
        {0.15f, 0.49f, 0.67f}, // 75 - Re (blue)
        {0.15f, 0.40f, 0.59f}, // 76 - Os (blue)
        {0.09f, 0.33f, 0.53f}, // 77 - Ir (blue)
        {0.82f, 0.82f, 0.88f}, // 78 - Pt (silver)
        {1.00f, 0.82f, 0.14f}, // 79 - Au (gold)
        {0.72f, 0.72f, 0.82f}, // 80 - Hg (silver)
        {0.65f, 0.33f, 0.30f}, // 81 - Tl (brown)
        {0.34f, 0.35f, 0.38f}, // 82 - Pb (gray)
        {0.62f, 0.31f, 0.71f}, // 83 - Bi (purple)
        {0.67f, 0.36f, 0.00f}, // 84 - Po (orange)
        {0.46f, 0.31f, 0.27f}, // 85 - At (brown)
        {0.26f, 0.51f, 0.59f}, // 86 - Rn (cyan)
        {0.26f, 0.00f, 0.40f}, // 87 - Fr (purple)
        {0.00f, 0.49f, 0.00f}, // 88 - Ra (green)
        {0.44f, 0.67f, 0.98f}, // 89 - Ac (blue)
        {0.00f, 0.73f, 1.00f}, // 90 - Th (cyan)
        {0.00f, 0.63f, 1.00f}, // 91 - Pa (blue)
        {0.00f, 0.56f, 1.00f}, // 92 - U (blue)
        {0.00f, 0.50f, 1.00f}, // 93 - Np (blue)
        {0.00f, 0.42f, 1.00f}, // 94 - Pu (blue)
        {0.33f, 0.36f, 0.95f}, // 95 - Am (blue)
        {0.47f, 0.36f, 0.89f}, // 96 - Cm (purple)
        {0.54f, 0.31f, 0.89f}, // 97 - Bk (purple)
        {0.63f, 0.21f, 0.83f}, // 98 - Cf (purple)
        {0.70f, 0.12f, 0.83f}, // 99 - Es (purple)
        {0.70f, 0.12f, 0.73f}, // 100 - Fm (purple)
        {0.70f, 0.05f, 0.65f}, // 101 - Md (purple)
        {0.74f, 0.05f, 0.53f}, // 102 - No (purple)
        {0.78f, 0.00f, 0.40f}, // 103 - Lr (purple)
        {0.80f, 0.00f, 0.35f}, // 104 - Rf (purple)
        {0.82f, 0.00f, 0.31f}, // 105 - Db (purple)
        {0.85f, 0.00f, 0.27f}, // 106 - Sg (purple)
        {0.88f, 0.00f, 0.22f}, // 107 - Bh (purple)
        {0.90f, 0.00f, 0.18f}, // 108 - Hs (purple)
        {0.92f, 0.00f, 0.15f}, // 109 - Mt (purple)
        {0.93f, 0.00f, 0.12f}, // 110 - Ds (purple)
        {0.94f, 0.00f, 0.10f}, // 111 - Rg (purple)
        {0.95f, 0.00f, 0.09f}, // 112 - Cn (purple)
        {0.96f, 0.00f, 0.08f}, // 113 - Nh (purple)
        {0.97f, 0.00f, 0.07f}, // 114 - Fl (purple)
        {0.98f, 0.00f, 0.06f}, // 115 - Mc (purple)
        {0.99f, 0.00f, 0.05f}, // 116 - Lv (purple)
        {0.99f, 0.00f, 0.04f}, // 117 - Ts (purple)
        {1.00f, 0.00f, 0.03f}, // 118 - Og (purple)
    };
    
    return colors;
}

void MoleculeRendererBase::get_cpk_color(int Z, float* rgb) {
    const auto& colors = get_cpk_color_table();
    
    // Clamp Z to valid range
    if (Z < 0 || Z > 118) {
        Z = 0;  // Unknown element → magenta
    }
    
    const auto& color = colors[Z];
    rgb[0] = color[0];
    rgb[1] = color[1];
    rgb[2] = color[2];
}

// ============================================================================
// Van der Waals Radii (Å)
// ============================================================================

float MoleculeRendererBase::get_vdw_radius(int Z) {
    // Van der Waals radii from Bondi (1964) J. Phys. Chem. 68, 441
    // and Rowland & Taylor (1996) J. Phys. Chem. 100, 7384
    
    switch (Z) {
        case 1:  return 1.20f;  // H
        case 6:  return 1.70f;  // C
        case 7:  return 1.55f;  // N
        case 8:  return 1.52f;  // O
        case 9:  return 1.47f;  // F
        case 14: return 2.10f;  // Si
        case 15: return 1.80f;  // P
        case 16: return 1.80f;  // S
        case 17: return 1.75f;  // Cl
        case 35: return 1.85f;  // Br
        case 53: return 1.98f;  // I
        
        // Alkali metals
        case 3:  return 1.82f;  // Li
        case 11: return 2.27f;  // Na
        case 19: return 2.75f;  // K
        
        // Alkaline earths
        case 4:  return 1.53f;  // Be
        case 12: return 1.73f;  // Mg
        case 20: return 2.31f;  // Ca
        
        // Transition metals (approximate)
        case 26: return 2.00f;  // Fe
        case 29: return 1.40f;  // Cu
        case 30: return 1.39f;  // Zn
        
        default: return 2.00f;  // Default fallback
    }
}

// ============================================================================
// Covalent Radii (Å)
// ============================================================================

float MoleculeRendererBase::get_covalent_radius(int Z) {
    // Covalent radii from Cordero et al. (2008) Dalton Trans., 2832
    
    switch (Z) {
        case 1:  return 0.31f;  // H
        case 6:  return 0.76f;  // C (sp3)
        case 7:  return 0.71f;  // N
        case 8:  return 0.66f;  // O
        case 9:  return 0.57f;  // F
        case 14: return 1.11f;  // Si
        case 15: return 1.07f;  // P
        case 16: return 1.05f;  // S
        case 17: return 1.02f;  // Cl
        case 35: return 1.20f;  // Br
        case 53: return 1.39f;  // I
        
        // Alkali metals
        case 3:  return 1.28f;  // Li
        case 11: return 1.66f;  // Na
        case 19: return 2.03f;  // K
        
        // Alkaline earths
        case 4:  return 0.96f;  // Be
        case 12: return 1.41f;  // Mg
        case 20: return 1.76f;  // Ca
        
        // Transition metals
        case 22: return 1.60f;  // Ti
        case 24: return 1.39f;  // Cr
        case 25: return 1.39f;  // Mn
        case 26: return 1.32f;  // Fe
        case 27: return 1.26f;  // Co
        case 28: return 1.24f;  // Ni
        case 29: return 1.32f;  // Cu
        case 30: return 1.22f;  // Zn
        
        default: return 1.50f;  // Default fallback
    }
}

// ============================================================================
// RendererFactory Implementation
// ============================================================================

std::unique_ptr<MoleculeRendererBase> RendererFactory::create_auto(const AtomicGeometry& geom) {
    ChemistryType type = MoleculeRendererBase::detect_chemistry_type(geom);
    
    switch (type) {
        case ChemistryType::ORGANIC:
            // TODO: return create_organic();
            return create_classic();  // Fallback to classic for now
        
        case ChemistryType::METALLIC:
            // TODO: return create_metallic();
            return create_classic();  // Fallback to classic for now
        
        case ChemistryType::CLASSIC:
        case ChemistryType::MIXED:
        case ChemistryType::UNKNOWN:
        default:
            return create_classic();
    }
}

std::unique_ptr<MoleculeRendererBase> RendererFactory::create_organic() {
    // TODO: Implement OrganicRenderer
    return create_classic();  // Fallback
}

std::unique_ptr<MoleculeRendererBase> RendererFactory::create_classic() {
    return std::make_unique<ClassicRenderer>();
}

std::unique_ptr<MoleculeRendererBase> RendererFactory::create_metallic() {
    // TODO: Implement MetallicRenderer
    return create_classic();  // Fallback
}

std::unique_ptr<MoleculeRendererBase> RendererFactory::create_by_name(const std::string& name) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name == "organic") {
        return create_organic();
    }
    if (lower_name == "classic" || lower_name == "ballstick") {
        return create_classic();
    }
    if (lower_name == "metallic" || lower_name == "metal") {
        return create_metallic();
    }
    
    // Default
    return create_classic();
}

} // namespace render
} // namespace vsepr
