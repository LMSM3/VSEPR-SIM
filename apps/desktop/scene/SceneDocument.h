#pragma once
/**
 * SceneDocument.h — Canonical exchange layer
 *
 * This is the ONLY data model the desktop UI ever touches.
 * It does not know about atomistic::State, FIRE, Langevin, VSEPR,
 * crystal lattices, reaction engines, or any future kernel feature.
 *
 * Any computational kernel that produces structures, trajectories,
 * or properties emits into this model. The desktop consumes it.
 *
 * Hierarchy:
 *
 *   SceneDocument
 *     ├── frames[]          — ordered snapshots (≥1)
 *     │     ├── atoms[]     — Z, position
 *     │     ├── bonds[]     — i, j, order
 *     │     ├── velocities  — optional per-atom
 *     │     ├── forces      — optional per-atom
 *     │     ├── charges     — optional per-atom
 *     │     ├── box         — optional PBC lattice vectors
 *     │     ├── properties  — key→double scalars
 *     │     └── time / step — optional provenance
 *     ├── properties        — document-level key→variant
 *     └── provenance        — source mode, parameters, hash
 */

#include <cstdint>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scene {

// ============================================================================
// Primitives
// ============================================================================

struct Vec3d {
    double x{}, y{}, z{};
    Vec3d() = default;
    Vec3d(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
};

inline double distance(const Vec3d& a, const Vec3d& b) {
    double dx = b.x-a.x, dy = b.y-a.y, dz = b.z-a.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// ============================================================================
// Per-atom record (one frame)
// ============================================================================

struct AtomRecord {
    int          Z;      // atomic number (identity)
    Vec3d        pos;    // Cartesian position (Å)
    std::string  symbol; // optional element symbol cache
};

struct BondRecord {
    int    i, j;         // atom indices (0-based)
    double order{1.0};   // bond order (1 = single, 1.5 = aromatic, 2 = double …)
};

// ============================================================================
// Periodic box (optional)
// ============================================================================

struct BoxInfo {
    Vec3d a, b, c;       // lattice vectors (Å)
    bool  enabled{false};
};

// ============================================================================
// FrameData — one snapshot
// ============================================================================

struct FrameData {
    // Required
    std::vector<AtomRecord> atoms;
    std::vector<BondRecord> bonds;

    // Optional per-atom fields (size == atoms.size() when present)
    std::vector<Vec3d>  velocities;   // Å/fs
    std::vector<Vec3d>  forces;       // kcal/(mol·Å)
    std::vector<double> charges;      // elementary charge (e)

    // Optional box
    BoxInfo box;

    // Frame-level scalar properties (key → value)
    // E.g. "energy_total", "energy_vdw", "temperature", "pressure",
    //      "force_rms", "dipole_moment", "polarizability", …
    std::map<std::string, double> properties;

    // Provenance
    int         step{-1};
    double      time{-1.0};     // fs, or -1 if not time-resolved
    std::string source_mode;    // "fire", "md_nvt", "emit", "crystal", …

    // Convenience
    int atom_count() const { return (int)atoms.size(); }
    int bond_count() const { return (int)bonds.size(); }

    Vec3d centroid() const {
        Vec3d c{};
        if (atoms.empty()) return c;
        for (const auto& a : atoms) { c.x += a.pos.x; c.y += a.pos.y; c.z += a.pos.z; }
        double n = (double)atoms.size();
        return {c.x/n, c.y/n, c.z/n};
    }

    double bounding_radius() const {
        Vec3d c = centroid();
        double maxR = 0;
        for (const auto& a : atoms) {
            double r = distance(a.pos, c);
            if (r > maxR) maxR = r;
        }
        return maxR;
    }
};

// ============================================================================
// PropertyValue — variant for document-level metadata
// ============================================================================

using PropertyValue = std::variant<double, int, std::string, bool>;

// ============================================================================
// Provenance — where this document came from
// ============================================================================

struct Provenance {
    std::string mode;            // "fire", "md_nvt", "emit", "crystal", "import"
    std::string source_file;     // file path if loaded from disk
    std::string formula;         // chemical formula if known
    std::string hash;            // SHA-256 if available
    std::map<std::string, std::string> parameters;  // arbitrary key→value
};

// ============================================================================
// SceneDocument — top-level container
// ============================================================================

struct SceneDocument {
    // Core data
    std::vector<FrameData> frames;

    // Document-level metadata
    std::map<std::string, PropertyValue> properties;

    // Provenance
    Provenance provenance;

    // Convenience
    bool empty() const { return frames.empty(); }
    int  frame_count() const { return (int)frames.size(); }

    const FrameData& current_frame() const { return frames.back(); }
    FrameData&       current_frame()       { return frames.back(); }

    const FrameData& frame(int i) const { return frames.at(i); }
    FrameData&       frame(int i)       { return frames.at(i); }

    bool is_trajectory() const { return frames.size() > 1; }
};

} // namespace scene
