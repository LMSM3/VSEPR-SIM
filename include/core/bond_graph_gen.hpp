#pragma once
/**
 * bond_graph_gen.hpp
 * ------------------
 * C++ backend for bond-graph frame generation and serialisation.
 *
 * Replaces tools/bond_graph_randomizer.py entirely.
 * Called by cmd_run_vsim when show_bond_graph = true.
 *
 * Responsibilities:
 *   - Random molecule generation (element pool + valence rules)
 *   - Covalent-radius bond detection from position arrays
 *   - Deterministic XYZ -> bond-graph conversion
 *   - JSON serialisation to NDJSON frame format consumed by viz_web.py /api/bond-graph
 *   - Launcher helper: spawn viz_web.py and open /graph in default browser
 *
 * VSEPR-SIM v5.1.4  |  WO-AUTO-01
 */

#include <cstdint>
#include <string>
#include <vector>

namespace vsepr {
namespace bond_graph {

// ---------------------------------------------------------------------------
// Atom  —  one node in the bond graph
// ---------------------------------------------------------------------------
struct BondAtom {
    std::string symbol;   // Element symbol ("C", "H", "O", ...)
    int         Z = 0;    // Atomic number
    double      x = 0.0; // Angstrom
    double      y = 0.0;
    double      z = 0.0;
};

// ---------------------------------------------------------------------------
// Bond  —  one edge in the bond graph
// ---------------------------------------------------------------------------
struct BondEdge {
    int    a = 0;           // Index into BondGraphFrame::atoms
    int    b = 0;
    double length_ang = 0.0;
    int    order = 1;       // 1=single, 2=double, 3=triple (heuristic)
};

// ---------------------------------------------------------------------------
// BondGraphFrame  —  one complete molecule snapshot
// ---------------------------------------------------------------------------
struct BondGraphFrame {
    uint64_t    frame_id  = 0;
    std::string formula;
    std::string source;    // "random" | "xyz:<path>" | "vsim:<script>"
    std::vector<BondAtom> atoms;
    std::vector<BondEdge> bonds;

    // Serialise to the JSON format consumed by viz_web.py /api/bond-graph
    std::string to_json() const;

    // Number of atoms
    int n_atoms() const { return static_cast<int>(atoms.size()); }
};

// ---------------------------------------------------------------------------
// BondGraphGenerator  —  produces frames
// ---------------------------------------------------------------------------
class BondGraphGenerator {
public:
    explicit BondGraphGenerator(uint64_t seed = 0);

    // Generate a random chemically-plausible molecule frame
    BondGraphFrame random_frame(uint64_t frame_id = 0);

    // Build a frame from a flat atom list (positions must be in Angstrom)
    BondGraphFrame from_atoms(const std::vector<BondAtom>& atoms,
                              uint64_t frame_id = 0,
                              const std::string& source = "vsim");

    // Parse the first molecule out of an XYZ string and return a frame
    BondGraphFrame from_xyz_string(const std::string& xyz,
                                   uint64_t frame_id = 0);

    // Detect bonds by covalent-radius sum (tolerance 1.2)
    static std::vector<BondEdge> detect_bonds(const std::vector<BondAtom>& atoms,
                                               double tolerance = 1.2);

    // Derive a molecular formula string from an atom list
    static std::string derive_formula(const std::vector<BondAtom>& atoms);

private:
    uint64_t seed_;
    uint64_t counter_ = 0;

    uint64_t next_rand();
};

// ---------------------------------------------------------------------------
// Launch helpers (called by cmd_run_vsim)
// ---------------------------------------------------------------------------

// Spawn tools/viz_web.py on the given port (non-blocking).
// Returns the process id (>0) on success, 0 on failure.
// tools_dir  — absolute path to the tools/ directory
// port       — HTTP port for viz_web.py
uint32_t launch_viz_web(const std::string& tools_dir, int port);

// Open the /graph page in the system default browser.
// Waits up to timeout_ms for the server to answer before opening.
void open_graph_browser(const std::string& host, int port,
                        int timeout_ms = 5000);

} // namespace bond_graph
} // namespace vsepr