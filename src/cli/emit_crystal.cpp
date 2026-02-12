#include "cli/emit_crystal.hpp"
#include "cli/emit_output.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace vsepr {
namespace cli {

// Simple 3D vector for fractional coordinates
struct Vec3 {
    double x, y, z;
};

// ============================================================================
// CRYSTAL STRUCTURE MOTIFS
// ============================================================================

// Rocksalt structure (NaCl-type, FCC)
// Space group: Fm-3m (cubic)
// Atoms per unit cell: 8 (4 cations + 4 anions)
// Stoichiometry: 1:1
struct RocksaltMotif {
    static const std::vector<Vec3> cation_positions() {
        return {
            {0.0, 0.0, 0.0},
            {0.5, 0.5, 0.0},
            {0.5, 0.0, 0.5},
            {0.0, 0.5, 0.5}
        };
    }

    static const std::vector<Vec3> anion_positions() {
        return {
            {0.5, 0.0, 0.0},
            {0.0, 0.5, 0.0},
            {0.0, 0.0, 0.5},
            {0.5, 0.5, 0.5}
        };
    }
};

// Rutile structure (TiO₂-type, tetragonal)
// Space group: P4₂/mnm
// Atoms per unit cell: 6 (2 cations + 4 anions)
// Stoichiometry: 1:2
// Used by: MgF₂, SnO₂, GeO₂, RuO₂
struct RutileMotif {
    static const std::vector<Vec3> cation_positions() {
        return {
            {0.0, 0.0, 0.0},    // Ti at origin
            {0.5, 0.5, 0.5}     // Ti at body center
        };
    }

    static const std::vector<Vec3> anion_positions() {
        return {
            {0.3, 0.3, 0.0},    // O positions (parameter u ≈ 0.3)
            {0.7, 0.7, 0.0},
            {0.8, 0.2, 0.5},
            {0.2, 0.8, 0.5}
        };
    }
};

// Tysonite structure (LaF₃-type, hexagonal/trigonal)
// Space group: P-3c1 (trigonal)
// Atoms per unit cell: 12 (3 cations + 9 anions)
// Stoichiometry: 1:3
// Used by: CeF₃, PrF₃, NdF₃
struct TysoniteMotif {
    static const std::vector<Vec3> cation_positions() {
        return {
            {0.0, 0.0, 0.25},     // La/Ce positions
            {0.0, 0.0, 0.75},
            {0.333, 0.667, 0.0}   // 2/3, 1/3 in hexagonal
        };
    }

    static const std::vector<Vec3> anion_positions() {
        return {
            // F1 positions (general)
            {0.333, 0.0, 0.25},
            {0.0, 0.333, 0.25},
            {0.667, 0.667, 0.25},
            // F2 positions (general)
            {0.667, 0.0, 0.75},
            {0.0, 0.667, 0.75},
            {0.333, 0.333, 0.75},
            // F3 positions (special)
            {0.111, 0.111, 0.0},
            {0.889, 0.889, 0.0},
            {0.222, 0.778, 0.5}
        };
    }
};

// Convert fractional to Cartesian coordinates (cubic cell)
Atom fractional_to_cartesian(
    const Vec3& frac,
    const std::string& element,
    double a, double b, double c
) {
    return {element, frac.x * a, frac.y * b, frac.z * c};
}

// Forward declarations for preset handlers
std::vector<Atom> emit_rocksalt(const ParsedCommand& cmd, double a, double b, double c);
std::vector<Atom> emit_rutile(const ParsedCommand& cmd, double a, double b, double c);
std::vector<Atom> emit_tysonite(const ParsedCommand& cmd, double a, double b, double c);

// ============================================================================
// PUBLIC API: Generate crystal atoms (for use by other actions)
// ============================================================================

std::vector<Atom> generate_crystal_atoms(
    const std::string& preset,
    const ParsedCommand& cmd,
    double a, double b, double c
) {
    if (preset == "rocksalt") {
        return emit_rocksalt(cmd, a, b, c);
    } else if (preset == "rutile") {
        return emit_rutile(cmd, a, b, c);
    } else if (preset == "tysonite") {
        return emit_tysonite(cmd, a, b, c);
    }
    return {};  // Unknown preset
}

int emit_crystal(const ParsedCommand& cmd, RunContext& ctx) {
    // Validate preset
    if (cmd.action_params.preset.empty()) {
        std::cerr << "ERROR: Crystal emission requires --preset <ID>\n";
        std::cerr << "Available presets: rocksalt, rutile, tysonite\n";
        return 1;
    }

    // Get cell dimensions
    if (ctx.cell_or_box.size() != 3) {
        std::cerr << "ERROR: Invalid cell dimensions\n";
        return 1;
    }

    double a = ctx.cell_or_box[0];
    double b = ctx.cell_or_box[1];
    double c = ctx.cell_or_box[2];

    // Route to appropriate preset handler
    std::vector<Atom> atoms;

    if (cmd.action_params.preset == "rocksalt") {
        atoms = emit_rocksalt(cmd, a, b, c);
    } else if (cmd.action_params.preset == "rutile") {
        atoms = emit_rutile(cmd, a, b, c);
    } else if (cmd.action_params.preset == "tysonite") {
        atoms = emit_tysonite(cmd, a, b, c);
    } else {
        std::cerr << "ERROR: Unknown preset: " << cmd.action_params.preset << "\n";
        std::cerr << "Available presets: rocksalt, rutile, tysonite\n";
        return 1;
    }

    if (atoms.empty()) {
        return 1;  // Error already printed by specific handler
    }

    // Sort by element for consistent output
    std::sort(atoms.begin(), atoms.end(), 
        [](const Atom& a, const Atom& b) {
            return a.element < b.element;
        }
    );

    // Write XYZ
    write_xyz(ctx.output_path, atoms, cmd, ctx);

    return 0;
}

// ============================================================================
// PRESET HANDLERS
// ============================================================================

std::vector<Atom> emit_rocksalt(const ParsedCommand& cmd, double a, double b, double c) {
    // Validate composition (rocksalt requires 2 elements, 1:1 ratio)
    if (cmd.spec.composition.elements.size() != 2) {
        std::cerr << "ERROR: Rocksalt structure requires exactly 2 elements\n";
        std::cerr << "Got: " << cmd.spec.formula() << " ("
                  << cmd.spec.composition.elements.size() << " elements)\n";
        std::cerr << "Example: NaCl, MgO, LiF\n";
        return {};
    }

    auto& elem1 = cmd.spec.composition.elements[0];
    auto& elem2 = cmd.spec.composition.elements[1];

    if (elem1.second != elem2.second) {
        std::cerr << "ERROR: Rocksalt structure requires 1:1 stoichiometry\n";
        std::cerr << "Got: " << elem1.first << elem1.second 
                  << elem2.first << elem2.second << "\n";
        std::cerr << "Example: NaCl, MgO (not Na2Cl)\n";
        return {};
    }

    // Warn for non-cubic
    if (std::abs(a - b) > 1e-6 || std::abs(a - c) > 1e-6) {
        std::cerr << "WARNING: Non-cubic cell detected (a=" << a 
                  << ", b=" << b << ", c=" << c << ")\n";
        std::cerr << "Rocksalt structure assumes cubic symmetry.\n";
    }

    std::cout << "Generating rocksalt structure:\n";
    std::cout << "  Formula: " << cmd.spec.formula() << "\n";
    std::cout << "  Cell: " << a << " × " << b << " × " << c << " Å\n";
    std::cout << "  Cation: " << elem1.first << " (" << elem1.second << ")\n";
    std::cout << "  Anion: " << elem2.first << " (" << elem2.second << ")\n";

    // Generate atoms
    std::vector<Atom> atoms;

    for (const auto& frac : RocksaltMotif::cation_positions()) {
        atoms.push_back(fractional_to_cartesian(frac, elem1.first, a, b, c));
    }

    for (const auto& frac : RocksaltMotif::anion_positions()) {
        atoms.push_back(fractional_to_cartesian(frac, elem2.first, a, b, c));
    }

    return atoms;
}

std::vector<Atom> emit_rutile(const ParsedCommand& cmd, double a, double b, double c) {
    // Validate composition (rutile requires 2 elements, 1:2 ratio)
    if (cmd.spec.composition.elements.size() != 2) {
        std::cerr << "ERROR: Rutile structure requires exactly 2 elements\n";
        std::cerr << "Got: " << cmd.spec.formula() << " ("
                  << cmd.spec.composition.elements.size() << " elements)\n";
        std::cerr << "Example: MgF2, TiO2, SnO2\n";
        return {};
    }

    auto& elem1 = cmd.spec.composition.elements[0];
    auto& elem2 = cmd.spec.composition.elements[1];

    // Check stoichiometry: should be 1:2 (cation:anion)
    if (elem1.second * 2 != elem2.second) {
        std::cerr << "ERROR: Rutile structure requires 1:2 stoichiometry\n";
        std::cerr << "Got: " << elem1.first << elem1.second 
                  << elem2.first << elem2.second << "\n";
        std::cerr << "Example: MgF2, TiO2 (not Mg2F or MgF3)\n";
        return {};
    }

    // Warn for non-tetragonal (a should equal b, c different)
    if (std::abs(a - b) > 1e-6) {
        std::cerr << "WARNING: Rutile is tetragonal (expects a = b ≠ c)\n";
        std::cerr << "Got: a=" << a << ", b=" << b << ", c=" << c << "\n";
    }

    std::cout << "Generating rutile structure:\n";
    std::cout << "  Formula: " << cmd.spec.formula() << "\n";
    std::cout << "  Cell: " << a << " × " << b << " × " << c << " Å (tetragonal)\n";
    std::cout << "  Cation: " << elem1.first << " (" << elem1.second << ")\n";
    std::cout << "  Anion: " << elem2.first << " (" << elem2.second << ")\n";

    // Generate atoms
    std::vector<Atom> atoms;

    for (const auto& frac : RutileMotif::cation_positions()) {
        atoms.push_back(fractional_to_cartesian(frac, elem1.first, a, b, c));
    }

    for (const auto& frac : RutileMotif::anion_positions()) {
        atoms.push_back(fractional_to_cartesian(frac, elem2.first, a, b, c));
    }

    return atoms;
}

std::vector<Atom> emit_tysonite(const ParsedCommand& cmd, double a, double b, double c) {
    // Validate composition (tysonite requires 2 elements, 1:3 ratio)
    if (cmd.spec.composition.elements.size() != 2) {
        std::cerr << "ERROR: Tysonite structure requires exactly 2 elements\n";
        std::cerr << "Got: " << cmd.spec.formula() << " ("
                  << cmd.spec.composition.elements.size() << " elements)\n";
        std::cerr << "Example: CeF3, LaF3, PrF3\n";
        return {};
    }

    auto& elem1 = cmd.spec.composition.elements[0];
    auto& elem2 = cmd.spec.composition.elements[1];

    // Check stoichiometry: should be 1:3 (cation:anion)
    if (elem1.second * 3 != elem2.second) {
        std::cerr << "ERROR: Tysonite structure requires 1:3 stoichiometry\n";
        std::cerr << "Got: " << elem1.first << elem1.second 
                  << elem2.first << elem2.second << "\n";
        std::cerr << "Example: CeF3, LaF3 (not Ce2F3 or CeF2)\n";
        return {};
    }

    // Warn for non-hexagonal (a should equal b, c different)
    if (std::abs(a - b) > 1e-6) {
        std::cerr << "WARNING: Tysonite is hexagonal (expects a = b ≠ c)\n";
        std::cerr << "Got: a=" << a << ", b=" << b << ", c=" << c << "\n";
    }

    std::cout << "Generating tysonite structure:\n";
    std::cout << "  Formula: " << cmd.spec.formula() << "\n";
    std::cout << "  Cell: " << a << " × " << b << " × " << c << " Å (hexagonal)\n";
    std::cout << "  Cation: " << elem1.first << " (" << elem1.second << ")\n";
    std::cout << "  Anion: " << elem2.first << " (" << elem2.second << ")\n";

    // Generate atoms
    std::vector<Atom> atoms;

    for (const auto& frac : TysoniteMotif::cation_positions()) {
        atoms.push_back(fractional_to_cartesian(frac, elem1.first, a, b, c));
    }

    for (const auto& frac : TysoniteMotif::anion_positions()) {
        atoms.push_back(fractional_to_cartesian(frac, elem2.first, a, b, c));
    }

    return atoms;
}

} // namespace cli
} // namespace vsepr
