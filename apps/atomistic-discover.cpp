/**
 * atomistic-discover: Deterministic Reaction Discovery System
 * 
 * Systematically explores chemical reaction space using:
 *   - Rule-based reaction generation (not random)
 *   - HSAB principle (hard-soft acid-base matching)
 *   - Fukui function reactivity matching
 *   - Pattern mining and template learning
 * 
 * Usage:
 *   atomistic-discover discover                    # Run discovery loop
 *   atomistic-discover test molA.xyz molB.xyz      # Test specific pair
 *   atomistic-discover analyze reactions.csv       # Mine patterns from data
 *   atomistic-discover generate N                  # Generate N random molecules
 */

#include "atomistic/reaction/engine.hpp"
#include "atomistic/reaction/discovery.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "io/xyz_format.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <random>
#include <filesystem>

using namespace atomistic;
using namespace atomistic::reaction;

// ============================================================================
// COMMAND LINE PARSING
// ============================================================================

struct DiscoverOptions {
    std::string mode;  // discover, test, analyze, generate
    
    // Input files
    std::string input_A;
    std::string input_B;
    std::string database_file;
    
    // Output
    std::string output_dir = "discovery_output";
    
    // Discovery parameters
    uint32_t num_molecules = 100;
    uint32_t num_batches = 10;
    uint32_t min_atoms = 5;
    uint32_t max_atoms = 20;
    
    double min_score = 0.5;
    double max_barrier = 30.0;
    double min_pattern_support = 0.1;
    
    bool verbose = false;
};

void print_usage() {
    std::cout << R"(
atomistic-discover: Deterministic Reaction Discovery System

USAGE:
    atomistic-discover <mode> [options]

MODES:
    discover             Run systematic reaction discovery loop
    test <A> <B>         Test all reaction templates on molecule pair
    analyze <csv>        Mine patterns from existing reaction database
    generate <N>         Generate N random molecules for testing

DISCOVERY MODE OPTIONS:
    --molecules N        Molecules per batch (default: 100)
    --batches N          Number of batches (default: 10)
    --min-atoms N        Minimum atoms per molecule (default: 5)
    --max-atoms N        Maximum atoms per molecule (default: 20)
    --min-score X        Minimum overall score (default: 0.5)
    --max-barrier X      Maximum activation barrier in kcal/mol (default: 30)
    --output DIR         Output directory (default: discovery_output)
    --verbose            Print detailed progress

TEST MODE:
    atomistic-discover test reactant_A.xyz reactant_B.xyz [--output DIR]

    Tests all reaction templates on the given pair of molecules.
    Outputs all feasible reactions ranked by score.

ANALYZE MODE:
    atomistic-discover analyze reactions.csv [--min-support X]

    Mines patterns from an existing reaction database.
    Extracts motifs, clusters reactions, generates new templates.

    --min-support X      Minimum pattern support (default: 0.1)

GENERATE MODE:
    atomistic-discover generate <N> [--output DIR]

    Generates N random chemically-reasonable molecules.
    Saves as XYZ files in output directory.

EXAMPLES:
    # Run discovery with default settings
    atomistic-discover discover

    # Custom discovery with more molecules
    atomistic-discover discover --molecules 200 --batches 20 --verbose

    # Test specific reactants
    atomistic-discover test ethylene.xyz bromine.xyz

    # Analyze existing data
    atomistic-discover analyze old_reactions.csv --min-support 0.15

    # Generate test molecules
    atomistic-discover generate 50 --output test_mols

DISCOVERY METHODOLOGY:
    1. Generate batch of random molecules (valence-constrained)
    2. For each pair, identify reactive sites (Fukui functions)
    3. Match sites using HSAB principle (soft-soft, hard-hard)
    4. Apply reaction templates (SN2, addition, elimination, etc.)
    5. Score by reactivity, geometry, thermodynamics
    6. Log successful reactions
    7. Every 3 batches: mine patterns, generate new templates
    8. Iterate until convergence or max batches

OUTPUT:
    discovery_output/
    ├── reactions.csv           # All proposed reactions with scores
    ├── discovery_report.md     # Summary statistics and patterns
    └── molecules/              # Generated test molecules (if --save-mols)

SCORING:
    Overall = 0.4·Reactivity + 0.3·Geometric + 0.3·Thermodynamic

    Reactivity:    Fukui function matching quality
    Geometric:     Orbital overlap feasibility
    Thermodynamic: Exothermicity + reasonable barrier

REFERENCES:
    - HSAB: Pearson, R. G. J. Am. Chem. Soc. 1963, 85, 3533.
    - Fukui: Parr & Yang, J. Am. Chem. Soc. 1984, 106, 4049.
    - BEP: Evans & Polanyi, Trans. Faraday Soc. 1938, 34, 11.
)";
}

bool parse_args(int argc, char** argv, DiscoverOptions& opts) {
    if (argc < 2) {
        print_usage();
        return false;
    }
    
    opts.mode = argv[1];
    
    // Mode-specific parsing
    if (opts.mode == "test") {
        if (argc < 4) {
            std::cerr << "Error: test mode requires two input files\n";
            std::cerr << "Usage: atomistic-discover test <molA.xyz> <molB.xyz>\n";
            return false;
        }
        opts.input_A = argv[2];
        opts.input_B = argv[3];
        
        for (int i = 4; i < argc; ++i) {
            if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
                opts.output_dir = argv[++i];
            }
        }
        
    } else if (opts.mode == "analyze") {
        if (argc < 3) {
            std::cerr << "Error: analyze mode requires database file\n";
            std::cerr << "Usage: atomistic-discover analyze <reactions.csv>\n";
            return false;
        }
        opts.database_file = argv[2];
        
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--min-support") == 0 && i + 1 < argc) {
                opts.min_pattern_support = std::stod(argv[++i]);
            }
        }
        
    } else if (opts.mode == "generate") {
        if (argc < 3) {
            std::cerr << "Error: generate mode requires number of molecules\n";
            std::cerr << "Usage: atomistic-discover generate <N>\n";
            return false;
        }
        opts.num_molecules = std::stoi(argv[2]);
        
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
                opts.output_dir = argv[++i];
            }
        }
        
    } else if (opts.mode == "discover") {
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--molecules") == 0 && i + 1 < argc) {
                opts.num_molecules = std::stoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--batches") == 0 && i + 1 < argc) {
                opts.num_batches = std::stoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--min-atoms") == 0 && i + 1 < argc) {
                opts.min_atoms = std::stoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--max-atoms") == 0 && i + 1 < argc) {
                opts.max_atoms = std::stoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--min-score") == 0 && i + 1 < argc) {
                opts.min_score = std::stod(argv[++i]);
            } else if (std::strcmp(argv[i], "--max-barrier") == 0 && i + 1 < argc) {
                opts.max_barrier = std::stod(argv[++i]);
            } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
                opts.output_dir = argv[++i];
            } else if (std::strcmp(argv[i], "--verbose") == 0) {
                opts.verbose = true;
            }
        }
        
    } else {
        std::cerr << "Error: unknown mode '" << opts.mode << "'\n";
        std::cerr << "Valid modes: discover, test, analyze, generate\n";
        return false;
    }
    
    return true;
}

// ============================================================================
// MODE IMPLEMENTATIONS
// ============================================================================

int mode_discover(const DiscoverOptions& opts) {
    std::cout << "═══ DISCOVERY MODE ═══\n\n";
    
    DiscoveryConfig config;
    config.molecules_per_batch = opts.num_molecules;
    config.max_batches = opts.num_batches;
    config.min_atoms = opts.min_atoms;
    config.max_atoms = opts.max_atoms;
    config.min_score = opts.min_score;
    config.max_barrier = opts.max_barrier;
    config.min_pattern_support = opts.min_pattern_support;
    config.output_dir = opts.output_dir;
    config.verbose = opts.verbose;
    
    DiscoveryEngine engine(config);
    auto stats = engine.run_discovery_loop();
    
    std::cout << "\nDiscovery complete!\n";
    std::cout << "Results saved to: " << opts.output_dir << "/\n";
    
    return 0;
}

int mode_test(const DiscoverOptions& opts) {
    std::cout << "═══ TEST MODE ═══\n\n";
    
    // Load molecules
    std::cout << "Loading molecules...\n";
    std::cout << "  A: " << opts.input_A << "\n";
    std::cout << "  B: " << opts.input_B << "\n\n";
    
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule mol_A_xyz, mol_B_xyz;
    if (!reader.read(opts.input_A, mol_A_xyz) || !reader.read(opts.input_B, mol_B_xyz)) {
        std::cerr << "Error: " << reader.get_error() << "\n";
        return 1;
    }
    
    State state_A = parsers::from_xyz(mol_A_xyz);
    State state_B = parsers::from_xyz(mol_B_xyz);
    
    std::cout << "Molecule A: " << state_A.N << " atoms\n";
    std::cout << "Molecule B: " << state_B.N << " atoms\n\n";
    
    // Create engine and test all templates
    ReactionEngine engine;
    
    auto sites_A = engine.identify_reactive_sites(state_A);
    auto sites_B = engine.identify_reactive_sites(state_B);
    
    std::cout << "Reactive sites:\n";
    std::cout << "  A: " << sites_A.size() << " sites\n";
    std::cout << "  B: " << sites_B.size() << " sites\n\n";
    
    std::cout << "Testing reaction templates...\n\n";
    
    std::vector<ProposedReaction> all_proposals;
    
    for (const auto& tmpl : engine.get_templates()) {
        std::cout << "  Template: " << tmpl.name << "\n";
        
        auto proposals = engine.match_reactive_sites(state_A, state_B, 
                                                     sites_A, sites_B, tmpl);
        
        std::cout << "    Found " << proposals.size() << " feasible reactions\n";
        
        all_proposals.insert(all_proposals.end(), proposals.begin(), proposals.end());
    }
    
    std::cout << "\n═══ RESULTS ═══\n\n";
    std::cout << "Total feasible reactions: " << all_proposals.size() << "\n\n";
    
    if (all_proposals.empty()) {
        std::cout << "No feasible reactions found with current templates.\n";
        std::cout << "Try:\n";
        std::cout << "  - Different molecules with higher reactivity\n";
        std::cout << "  - Adjusting template constraints\n";
        std::cout << "  - Running discovery mode to learn new templates\n";
        return 0;
    }
    
    // Sort by score
    std::sort(all_proposals.begin(), all_proposals.end(),
              [](const ProposedReaction& a, const ProposedReaction& b) {
                  return a.overall_score > b.overall_score;
              });
    
    std::cout << "Top 10 reactions:\n\n";
    std::cout << "| Rank | Mechanism | Ea (kcal/mol) | ΔE (kcal/mol) | k (s⁻¹) | Score |\n";
    std::cout << "|------|-----------|---------------|---------------|---------|-------|\n";
    
    for (size_t i = 0; i < std::min(size_t(10), all_proposals.size()); ++i) {
        const auto& r = all_proposals[i];
        std::cout << "| " << (i + 1) << " | ";
        std::cout << static_cast<int>(r.mechanism) << " | ";
        std::cout << std::fixed << std::setprecision(2) << r.activation_barrier << " | ";
        std::cout << r.reaction_energy << " | ";
        std::cout << std::scientific << std::setprecision(2) << r.rate_constant << " | ";
        std::cout << std::fixed << std::setprecision(3) << r.overall_score << " |\n";
    }
    
    std::cout << "\nBest reaction:\n";
    const auto& best = all_proposals[0];
    std::cout << "  Mechanism: " << best.description << "\n";
    std::cout << "  Activation barrier: " << best.activation_barrier << " kcal/mol\n";
    std::cout << "  Reaction energy: " << best.reaction_energy << " kcal/mol\n";
    std::cout << "  Rate constant (298 K): " << best.rate_constant << " s⁻¹\n";
    std::cout << "  Overall score: " << best.overall_score << "\n\n";
    
    std::cout << "  Attacking site: atom " << best.attacking_site.atom_index << "\n";
    std::cout << "    f⁺ = " << best.attacking_site.fukui_plus << "\n";
    std::cout << "    f⁻ = " << best.attacking_site.fukui_minus << "\n\n";
    
    std::cout << "  Attacked site: atom " << best.attacked_site.atom_index << "\n";
    std::cout << "    f⁺ = " << best.attacked_site.fukui_plus << "\n";
    std::cout << "    f⁻ = " << best.attacked_site.fukui_minus << "\n\n";
    
    return 0;
}

int mode_analyze(const DiscoverOptions& opts) {
    std::cout << "═══ ANALYZE MODE ═══\n\n";
    
    std::cout << "Loading reaction database: " << opts.database_file << "\n\n";
    
    DiscoveryDatabase db;
    db.load(opts.database_file);
    
    auto stats = db.get_stats();
    
    std::cout << "Database statistics:\n";
    std::cout << "  Total reactions: " << stats.reactions_proposed << "\n";
    std::cout << "  Validated: " << stats.reactions_validated << "\n";
    std::cout << "  Feasible: " << stats.reactions_feasible << "\n\n";
    
    std::cout << "Mining patterns (min support: " << opts.min_pattern_support << ")...\n";
    
    auto patterns = db.mine_patterns(opts.min_pattern_support);
    
    std::cout << "Found " << patterns.size() << " patterns:\n\n";
    
    for (const auto& pattern : patterns) {
        std::cout << "  • " << pattern.name << "\n";
        std::cout << "    Observations: " << pattern.observation_count << "\n";
        std::cout << "    Success rate: " << (pattern.success_rate * 100) << "%\n";
        std::cout << "    Avg barrier: " << pattern.avg_barrier << " ± " 
                  << pattern.std_barrier << " kcal/mol\n";
        std::cout << "    Avg ΔE: " << pattern.avg_exothermicity << " kcal/mol\n\n";
    }
    
    std::cout << "Generating templates from high-success patterns...\n";
    
    for (const auto& pattern : patterns) {
        if (pattern.success_rate > 0.5) {
            auto tmpl = db.generate_template_from_pattern(pattern);
            std::cout << "  ✓ " << tmpl.name << " (max barrier: " 
                     << tmpl.max_barrier << " kcal/mol)\n";
        }
    }
    
    std::cout << "\nGenerating report...\n";
    generate_discovery_report(db, opts.output_dir + "/analysis_report.md");
    
    std::cout << "Report saved to: " << opts.output_dir << "/analysis_report.md\n";
    
    return 0;
}

int mode_generate(const DiscoverOptions& opts) {
    std::cout << "═══ GENERATE MODE ═══\n\n";
    
    std::cout << "Generating " << opts.num_molecules << " random molecules...\n\n";
    
    DiscoveryEngine engine;
    std::mt19937 rng(std::random_device{}());
    
    std::filesystem::create_directories(opts.output_dir);
    
    for (uint32_t i = 0; i < opts.num_molecules; ++i) {
        std::uniform_int_distribution<uint32_t> atom_dist(opts.min_atoms, opts.max_atoms);
        uint32_t num_atoms = atom_dist(rng);
        
        State mol = engine.generate_random_molecule(num_atoms);
        
        // Save as XYZ
        vsepr::io::XYZMolecule xyz_mol;
        xyz_mol.comment = "Generated molecule " + std::to_string(i + 1);
        
        for (uint32_t j = 0; j < mol.N; ++j) {
            vsepr::io::XYZAtom atom("C", mol.X[j].x, mol.X[j].y, mol.X[j].z);
            xyz_mol.atoms.push_back(atom);
        }
        
        std::string filename = opts.output_dir + "/molecule_" + 
                              std::to_string(i + 1) + ".xyz";
        vsepr::io::XYZWriter writer;
        writer.write(filename, xyz_mol);
        
        if ((i + 1) % 10 == 0) {
            std::cout << "  Generated " << (i + 1) << " molecules\r" << std::flush;
        }
    }
    
    std::cout << "\n\nSaved " << opts.num_molecules << " molecules to " 
              << opts.output_dir << "/\n";
    
    return 0;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    DiscoverOptions opts;
    
    if (!parse_args(argc, argv, opts)) {
        return 1;
    }
    
    try {
        if (opts.mode == "discover") {
            return mode_discover(opts);
        } else if (opts.mode == "test") {
            return mode_test(opts);
        } else if (opts.mode == "analyze") {
            return mode_analyze(opts);
        } else if (opts.mode == "generate") {
            return mode_generate(opts);
        } else {
            std::cerr << "Error: unknown mode '" << opts.mode << "'\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
