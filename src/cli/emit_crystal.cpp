#include "cli/emit_crystal.hpp"
#include "cli/emit_output.hpp"
#include "cli/crystal_visualizer.hpp"  // NEW: Visualization bridge
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "atomistic/crystal/crystal_metrics.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace vsepr {
namespace cli {

// Helper struct to carry lattice info through pipeline (defined early for forward use)
struct CrystalEmissionResult {
    std::vector<Atom> atoms;
    atomistic::crystal::Lattice lattice;
    std::string name;
    int space_group = 0;
    std::string space_symbol;
};

// Forward declarations
CrystalEmissionResult emit_atomistic_preset(const std::string& preset_name);

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
    // Try atomistic presets first
    auto result = emit_atomistic_preset(preset);
    if (!result.atoms.empty()) {
        return result.atoms;  // Lattice info discarded (legacy compatibility)
    }

    // Fall back to motif-based presets (composition-driven, user-supplied cell)
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
        std::cerr << "\nAtomistic presets (fixed cell params):\n";
        std::cerr << "  Metals:   al, fe, cu, au\n";
        std::cerr << "  Ionics:   nacl_atomistic, mgo, cscl\n";
        std::cerr << "  Covalent: si, diamond\n";
        std::cerr << "  Oxides:   tio2\n";
        std::cerr << "\nMotif-based presets (user-supplied cell):\n";
        std::cerr << "  rocksalt  (1:1 stoichiometry, e.g. NaCl@crystal)\n";
        std::cerr << "  rutile    (1:2 stoichiometry, e.g. TiO2@crystal)\n";
        std::cerr << "  tysonite  (1:3 stoichiometry, e.g. CeF3@crystal)\n";
        return 1;
    }

    // Try atomistic presets first (these have lattice info)
    CrystalEmissionResult result = emit_atomistic_preset(cmd.action_params.preset);

    std::vector<Atom> atoms;
    atomistic::crystal::Lattice lattice;
    std::string structure_name = cmd.action_params.preset;

    if (!result.atoms.empty()) {
        // Atomistic preset succeeded
        atoms = std::move(result.atoms);
        lattice = result.lattice;
        structure_name = result.name;
    } else {
        // Fall back to motif-based presets (composition-driven, no lattice tracking yet)
        double a = 1.0, b = 1.0, c = 1.0;
        if (ctx.cell_or_box.size() == 3) {
            a = ctx.cell_or_box[0];
            b = ctx.cell_or_box[1];
            c = ctx.cell_or_box[2];
        }

        atoms = generate_crystal_atoms(cmd.action_params.preset, cmd, a, b, c);

        if (atoms.empty()) {
            std::cerr << "ERROR: Unknown preset or generation failed: " 
                      << cmd.action_params.preset << "\n";
            std::cerr << "Run without --preset to see available options.\n";
            return 1;
        }

        // Motif-based: construct cubic lattice from cell params
        lattice = atomistic::crystal::Lattice::cubic(a);
        structure_name = cmd.spec.formula() + " (" + cmd.action_params.preset + ")";
    }

    // ========================================================================
    // SUPERCELL CONSTRUCTION (if requested)
    // ========================================================================

    if (!cmd.action_params.supercell.empty()) {
        if (cmd.action_params.supercell.size() != 3) {
            std::cerr << "ERROR: --supercell requires 3 integers (na, nb, nc)\n";
            std::cerr << "Example: --supercell 2,2,2\n";
            return 1;
        }

        int na = cmd.action_params.supercell[0];
        int nb = cmd.action_params.supercell[1];
        int nc = cmd.action_params.supercell[2];

        if (na < 1 || nb < 1 || nc < 1) {
            std::cerr << "ERROR: Supercell replication factors must be >= 1\n";
            return 1;
        }

        std::cout << "\nBuilding " << na << "×" << nb << "×" << nc << " supercell...\n";

        // Get lattice vectors
        atomistic::Vec3 a_vec = lattice.A.col(0);
        atomistic::Vec3 b_vec = lattice.A.col(1);
        atomistic::Vec3 c_vec = lattice.A.col(2);

        std::vector<Atom> supercell_atoms;
        supercell_atoms.reserve(atoms.size() * na * nb * nc);

        for (int p = 0; p < na; ++p) {
            for (int q = 0; q < nb; ++q) {
                for (int r = 0; r < nc; ++r) {
                    for (const auto& atom : atoms) {
                        Atom shifted = atom;
                        shifted.x += p * a_vec.x + q * b_vec.x + r * c_vec.x;
                        shifted.y += p * a_vec.y + q * b_vec.y + r * c_vec.y;
                        shifted.z += p * a_vec.z + q * b_vec.z + r * c_vec.z;
                        supercell_atoms.push_back(shifted);
                    }
                }
            }
        }

        std::cout << "  Unit cell: " << atoms.size() << " atoms\n";
        std::cout << "  Supercell: " << supercell_atoms.size() << " atoms\n";

        // Update lattice to expanded dimensions
        lattice = atomistic::crystal::Lattice(
            a_vec * static_cast<double>(na),
            b_vec * static_cast<double>(nb),
            c_vec * static_cast<double>(nc)
        );

        std::cout << "  Expanded box: " << lattice.a_len() << " × " 
                  << lattice.b_len() << " × " << lattice.c_len() << " Å\n";

        atoms = std::move(supercell_atoms);
    }

    // ========================================================================
    // VISUALIZATION (if --viz is set)
    // ========================================================================

    if (ctx.viz_enabled) {
        return launch_crystal_visualizer(atoms, lattice, cmd.action_params.supercell, structure_name);
    }

    // ========================================================================
    // XYZ OUTPUT (default path)
    // ========================================================================

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

// ============================================================================
// ATOMISTIC CRYSTAL PRESETS (using atomistic::crystal module)
// ============================================================================

// Convert atomistic::crystal::UnitCell to CLI Atom vector + preserve lattice
CrystalEmissionResult unit_cell_to_emission_result(const atomistic::crystal::UnitCell& uc) {
    CrystalEmissionResult result;
    result.lattice = uc.lattice;
    result.name = uc.name;
    result.space_group = uc.space_group_number;
    result.space_symbol = uc.space_group_symbol;

    // ── Crystal Verification Metrics (auto-computed on every emission) ──
    auto metrics = atomistic::crystal::compute_all_metrics(uc);
    metrics.print_summary();

    result.atoms.reserve(uc.num_atoms());

    for (const auto& basis_atom : uc.basis) {
        atomistic::Vec3 cart = uc.lattice.to_cartesian(basis_atom.frac);

        // Map atomic number to element symbol
        static const char* symbols[] = {
            "",                                                               // 0
            "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne",          // 1-10
            "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",       // 11-20
            "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",     // 21-30
            "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y", "Zr",     // 31-40
            "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn",    // 41-50
            "Sb", "Te", "I", "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd",     // 51-60
            "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb",    // 61-70
            "Lu", "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt", "Au", "Hg",     // 71-80
            "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr", "Ra", "Ac", "Th",    // 81-90
            "Pa", "U", "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm"      // 91-100
        };

        std::string elem = (basis_atom.type < 101) ? symbols[basis_atom.type] : "X";
        result.atoms.push_back({elem, cart.x, cart.y, cart.z});
    }

    return result;
}

// Handler for atomistic presets (Al, Fe, Cu, Au, NaCl, MgO, CsCl, Si, C, TiO2)
// Returns emission result with lattice preserved (or empty if not atomistic preset)
CrystalEmissionResult emit_atomistic_preset(const std::string& preset_name) {
    using namespace atomistic::crystal;

    // Map preset name to factory function
    if (preset_name == "aluminum_fcc" || preset_name == "al_fcc" || preset_name == "al") {
        return unit_cell_to_emission_result(presets::aluminum_fcc());
    }
    else if (preset_name == "iron_bcc" || preset_name == "fe_bcc" || preset_name == "fe") {
        return unit_cell_to_emission_result(presets::iron_bcc());
    }
    else if (preset_name == "copper_fcc" || preset_name == "cu_fcc" || preset_name == "cu") {
        return unit_cell_to_emission_result(presets::copper_fcc());
    }
    else if (preset_name == "gold_fcc" || preset_name == "au_fcc" || preset_name == "au") {
        return unit_cell_to_emission_result(presets::gold_fcc());
    }
    else if (preset_name == "sodium_chloride" || preset_name == "nacl_rock" || preset_name == "nacl_atomistic") {
        return unit_cell_to_emission_result(presets::sodium_chloride());
    }
    else if (preset_name == "magnesium_oxide" || preset_name == "mgo") {
        return unit_cell_to_emission_result(presets::magnesium_oxide());
    }
    else if (preset_name == "cesium_chloride" || preset_name == "cscl") {
        return unit_cell_to_emission_result(presets::cesium_chloride());
    }
    else if (preset_name == "silicon_diamond" || preset_name == "si") {
        return unit_cell_to_emission_result(presets::silicon_diamond());
    }
    else if (preset_name == "carbon_diamond" || preset_name == "diamond" || preset_name == "c") {
        return unit_cell_to_emission_result(presets::carbon_diamond());
    }
    else if (preset_name == "rutile_tio2" || preset_name == "tio2") {
        return unit_cell_to_emission_result(presets::rutile_tio2());
    }
    // --- Fluorite-type ---
    else if (preset_name == "tho2" || preset_name == "tho2_fluorite") {
        return unit_cell_to_emission_result(presets::tho2_fluorite());
    }
    else if (preset_name == "puo2" || preset_name == "puo2_fluorite") {
        return unit_cell_to_emission_result(presets::puo2_fluorite());
    }
    else if (preset_name == "ceo2" || preset_name == "ceo2_fluorite") {
        return unit_cell_to_emission_result(presets::ceo2_fluorite());
    }
    else if (preset_name == "zro2_cubic" || preset_name == "zro2c") {
        return unit_cell_to_emission_result(presets::zro2_cubic());
    }
    else if (preset_name == "zro2_tetragonal" || preset_name == "zro2t") {
        return unit_cell_to_emission_result(presets::zro2_tetragonal());
    }
    else if (preset_name == "zro2_monoclinic" || preset_name == "zro2m" || preset_name == "zro2") {
        return unit_cell_to_emission_result(presets::zro2_monoclinic());
    }
    else if (preset_name == "hfo2_cubic" || preset_name == "hfo2c") {
        return unit_cell_to_emission_result(presets::hfo2_cubic());
    }
    else if (preset_name == "hfo2_tetragonal" || preset_name == "hfo2t") {
        return unit_cell_to_emission_result(presets::hfo2_tetragonal());
    }
    else if (preset_name == "hfo2_monoclinic" || preset_name == "hfo2m" || preset_name == "hfo2") {
        return unit_cell_to_emission_result(presets::hfo2_monoclinic());
    }
    // --- Spinels ---
    else if (preset_name == "mgal2o4" || preset_name == "spinel") {
        return unit_cell_to_emission_result(presets::mgal2o4_spinel());
    }
    else if (preset_name == "fe3o4" || preset_name == "magnetite") {
        return unit_cell_to_emission_result(presets::fe3o4_spinel());
    }
    else if (preset_name == "co3o4") {
        return unit_cell_to_emission_result(presets::co3o4_spinel());
    }
    // --- Perovskites ---
    else if (preset_name == "srtio3" || preset_name == "strontium_titanate") {
        return unit_cell_to_emission_result(presets::srtio3_perovskite());
    }
    else if (preset_name == "batio3" || preset_name == "batio3_cubic") {
        return unit_cell_to_emission_result(presets::batio3_cubic());
    }
    else if (preset_name == "batio3_tetragonal" || preset_name == "batio3t") {
        return unit_cell_to_emission_result(presets::batio3_tetragonal());
    }
    else if (preset_name == "catio3" || preset_name == "perovskite") {
        return unit_cell_to_emission_result(presets::catio3_orthorhombic());
    }
    else if (preset_name == "laalo3" || preset_name == "laalo3_rhomb") {
        return unit_cell_to_emission_result(presets::laalo3_rhombohedral());
    }
    // --- Garnets ---
    else if (preset_name == "yag" || preset_name == "y3al5o12") {
        return unit_cell_to_emission_result(presets::y3al5o12_garnet());
    }
    else if (preset_name == "ggg" || preset_name == "gd3ga5o12") {
        return unit_cell_to_emission_result(presets::gd3ga5o12_garnet());
    }
    // --- Apatite ---
    else if (preset_name == "apatite" || preset_name == "fluorapatite" || preset_name == "ca5po43f") {
        return unit_cell_to_emission_result(presets::ca5po4_3f_apatite());
    }
    // --- Monazite ---
    else if (preset_name == "lapo4" || preset_name == "monazite") {
        return unit_cell_to_emission_result(presets::lapo4_monazite());
    }
    // --- Pyrochlores ---
    else if (preset_name == "gd2ti2o7" || preset_name == "gd_pyrochlore") {
        return unit_cell_to_emission_result(presets::gd2ti2o7_pyrochlore());
    }
    else if (preset_name == "la2zr2o7" || preset_name == "la_pyrochlore") {
        return unit_cell_to_emission_result(presets::la2zr2o7_pyrochlore());
    }
    else if (preset_name == "bi2ti2o7" || preset_name == "bi_pyrochlore") {
        return unit_cell_to_emission_result(presets::bi2ti2o7_pyrochlore());
    }
    // --- Actinide Oxides ---
    else if (preset_name == "u3o8") {
        return unit_cell_to_emission_result(presets::u3o8_orthorhombic());
    }
    else if (preset_name == "uo3" || preset_name == "uo3_gamma") {
        return unit_cell_to_emission_result(presets::uo3_gamma());
    }

    // Not an atomistic preset — return empty
    return CrystalEmissionResult();
}

} // namespace cli
} // namespace vsepr
