#pragma once
/**
 * fragment_view.hpp — Atomistic Fragment View (Scale Boundary Object)
 *
 * Defines the clean interface between the atomistic (source-of-truth)
 * layer and downstream consumers such as coarse_grain::. This is the
 * bridge object that packages all atomistic information needed for
 * descriptor construction, bead generation, or validation.
 *
 * The atomistic layer produces FragmentViews. The coarse-grained layer
 * consumes them. The reverse dependency must never exist.
 *
 * Data flow:
 *   atomistic structure → FragmentView → unified descriptor → bead interactions
 *
 * Anti-black-box:
 *   - Every atom, bond, charge, and frame is explicitly stored
 *   - Validation produces explicit status codes, not silent failures
 *   - The fragment view can be independently inspected and tested
 *
 * Reference: "Atomistic Preparation Layer" section of
 *            section_anisotropic_beads.tex
 */

#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace atomistic {

// ============================================================================
// Fragment Status
// ============================================================================

/**
 * FragmentStatus — explicit failure codes for fragment validation.
 *
 * The atomistic layer must produce either a valid FragmentView or
 * an explicit failure with a reason code. Silent propagation of
 * invalid inputs is prohibited.
 */
enum class FragmentStatus : uint8_t {
    Valid,                  // Fragment is well-defined and ready for consumption
    InvalidGeometry,       // Degenerate or collapsed atom positions
    MissingCharges,        // Incomplete charge assignment
    IllDefinedFrame,       // Insufficient atoms or degeneracy for frame construction
    UnsupportedTopology,   // Unrecognized or unsupported bonding pattern
    EmptyFragment,         // No atoms provided
    DuplicatePositions     // Two or more atoms at the same position
};

/**
 * Return human-readable description of a fragment status.
 */
inline const char* fragment_status_name(FragmentStatus s) {
    switch (s) {
        case FragmentStatus::Valid:               return "Valid";
        case FragmentStatus::InvalidGeometry:     return "Invalid geometry (degenerate or collapsed positions)";
        case FragmentStatus::MissingCharges:      return "Missing charges (incomplete charge assignment)";
        case FragmentStatus::IllDefinedFrame:     return "Ill-defined frame (insufficient atoms or degeneracy)";
        case FragmentStatus::UnsupportedTopology: return "Unsupported topology (unrecognized bonding pattern)";
        case FragmentStatus::EmptyFragment:       return "Empty fragment (no atoms provided)";
        case FragmentStatus::DuplicatePositions:  return "Duplicate positions (atoms at same location)";
        default:                                  return "Unknown status";
    }
}

// ============================================================================
// Atom Record
// ============================================================================

/**
 * AtomRecord — one atom within a fragment view.
 *
 * Contains position, identity, charge, and classification flags.
 * Uses the project's native Vec3 type (no external dependencies).
 */
struct AtomRecord {
    Vec3     position{};        // Cartesian position (Å)
    int      atomic_number{};   // Element (1=H, 6=C, 7=N, 8=O, etc.)
    double   charge{};          // Partial charge (e)
    double   mass{};            // Atomic mass (amu)
    uint32_t flags{};           // Classification flags (see below)
    uint32_t original_index{};  // Index in the source atomistic::State

    // --- Flag constants ---
    static constexpr uint32_t FLAG_NONE           = 0;
    static constexpr uint32_t FLAG_METAL          = 1 << 0;  // Transition metal or lanthanide
    static constexpr uint32_t FLAG_AROMATIC       = 1 << 1;  // Part of aromatic ring
    static constexpr uint32_t FLAG_RING_MEMBER    = 1 << 2;  // Part of any ring
    static constexpr uint32_t FLAG_HYDROGEN       = 1 << 3;  // Hydrogen atom
    static constexpr uint32_t FLAG_HETEROATOM     = 1 << 4;  // Heteroatom (N, O, S, etc.)
    static constexpr uint32_t FLAG_CHARGED        = 1 << 5;  // Formally charged atom

    bool is_metal()      const { return (flags & FLAG_METAL) != 0; }
    bool is_aromatic()   const { return (flags & FLAG_AROMATIC) != 0; }
    bool is_ring_member() const { return (flags & FLAG_RING_MEMBER) != 0; }
    bool is_hydrogen()   const { return (flags & FLAG_HYDROGEN) != 0; }
};

// ============================================================================
// Bond Record
// ============================================================================

/**
 * BondRecord — one bond within a fragment view.
 *
 * Indices are local to the fragment (0-based into the atoms vector).
 */
struct BondRecord {
    uint32_t i{};       // First atom index (local to fragment)
    uint32_t j{};       // Second atom index (local to fragment)
    uint8_t  order{1};  // Bond order (1=single, 2=double, 3=triple, 4=aromatic)
};

// ============================================================================
// Local Frame
// ============================================================================

/**
 * LocalFrame — local coordinate frame derived from structure.
 *
 * Constructed from the inertia tensor principal axes. Provides the
 * orientation reference for all directional properties.
 */
struct LocalFrame {
    Vec3 origin{};      // Frame origin (typically center of mass)
    Vec3 axis1{};       // Primary axis (smallest moment of inertia)
    Vec3 axis2{};       // Secondary axis
    Vec3 axis3{};       // Tertiary axis (largest moment of inertia)
    bool well_defined{false};  // True if frame construction succeeded
};

// ============================================================================
// Fragment View
// ============================================================================

/**
 * FragmentView — the scale boundary object.
 *
 * Packages all atomistic information needed for coarse-grained
 * descriptor construction into a single, validated interface.
 *
 * The coarse-grained layer receives a FragmentView and constructs
 * a bead and its unified descriptor from it.
 */
struct FragmentView {
    // --- Atom-level data ---
    std::vector<AtomRecord> atoms;
    std::vector<BondRecord> bonds;

    // --- Aggregate properties ---
    double total_mass{};
    double total_charge{};

    // --- Local frame ---
    LocalFrame frame;

    // --- Structural classification ---
    bool aromatic{false};
    bool cyclic{false};
    bool organometallic{false};

    // --- Validation ---
    FragmentStatus status{FragmentStatus::EmptyFragment};

    // ====================================================================
    // Convenience accessors
    // ====================================================================

    /** Number of atoms in the fragment. */
    uint32_t num_atoms() const { return static_cast<uint32_t>(atoms.size()); }

    /** Number of bonds in the fragment. */
    uint32_t num_bonds() const { return static_cast<uint32_t>(bonds.size()); }

    /** Whether the fragment is valid for downstream use. */
    bool is_valid() const { return status == FragmentStatus::Valid; }

    /** Whether any atom in the fragment is a metal center. */
    bool has_metal_center() const {
        for (const auto& a : atoms) {
            if (a.is_metal()) return true;
        }
        return false;
    }

    /** Center of mass. */
    Vec3 center_of_mass() const {
        if (atoms.empty() || total_mass < 1e-20) return {};
        Vec3 com{};
        for (const auto& a : atoms) {
            com.x += a.mass * a.position.x;
            com.y += a.mass * a.position.y;
            com.z += a.mass * a.position.z;
        }
        return {com.x / total_mass, com.y / total_mass, com.z / total_mass};
    }

    /** Center of geometry (unweighted). */
    Vec3 center_of_geometry() const {
        if (atoms.empty()) return {};
        Vec3 cog{};
        for (const auto& a : atoms) {
            cog.x += a.position.x;
            cog.y += a.position.y;
            cog.z += a.position.z;
        }
        double inv_n = 1.0 / atoms.size();
        return {cog.x * inv_n, cog.y * inv_n, cog.z * inv_n};
    }
};

// ============================================================================
// Fragment Construction
// ============================================================================

/**
 * Build a FragmentView from a subset of atoms in an atomistic::State.
 *
 * This is the primary entry point for the atomistic preparation layer.
 * It extracts atom records, computes aggregates, and validates the result.
 *
 * @param state    Source atomistic state
 * @param indices  Atom indices to include in the fragment
 * @return FragmentView with validation status
 */
inline FragmentView build_fragment_view(
    const State& state,
    const std::vector<uint32_t>& indices)
{
    FragmentView frag;

    // --- Empty check ---
    if (indices.empty()) {
        frag.status = FragmentStatus::EmptyFragment;
        return frag;
    }

    // --- Build atom records ---
    frag.atoms.reserve(indices.size());
    frag.total_mass = 0.0;
    frag.total_charge = 0.0;

    for (uint32_t idx : indices) {
        if (idx >= state.N) {
            frag.status = FragmentStatus::InvalidGeometry;
            return frag;
        }

        AtomRecord rec;
        rec.position = state.X[idx];
        rec.charge = state.Q[idx];
        rec.mass = state.M[idx];
        rec.original_index = idx;

        // Derive atomic number from type (convention: type = atomic number)
        rec.atomic_number = static_cast<int>(state.type[idx]);

        // Set flags
        rec.flags = AtomRecord::FLAG_NONE;
        if (rec.atomic_number == 1) {
            rec.flags |= AtomRecord::FLAG_HYDROGEN;
        }
        // Transition metals: Z = 21-30, 39-48, 57-80, 89-112
        if ((rec.atomic_number >= 21 && rec.atomic_number <= 30) ||
            (rec.atomic_number >= 39 && rec.atomic_number <= 48) ||
            (rec.atomic_number >= 57 && rec.atomic_number <= 80) ||
            (rec.atomic_number >= 89 && rec.atomic_number <= 112)) {
            rec.flags |= AtomRecord::FLAG_METAL;
            frag.organometallic = true;
        }
        // Heteroatoms: N, O, S, P, F, Cl, Br, I
        if (rec.atomic_number == 7 || rec.atomic_number == 8 ||
            rec.atomic_number == 9 || rec.atomic_number == 15 ||
            rec.atomic_number == 16 || rec.atomic_number == 17 ||
            rec.atomic_number == 35 || rec.atomic_number == 53) {
            rec.flags |= AtomRecord::FLAG_HETEROATOM;
        }
        if (std::abs(rec.charge) > 0.5) {
            rec.flags |= AtomRecord::FLAG_CHARGED;
        }

        frag.total_mass += rec.mass;
        frag.total_charge += rec.charge;
        frag.atoms.push_back(rec);
    }

    // --- Extract bonds from state edge list ---
    // Build a local index map: original_index → local_index
    // Only bonds where both endpoints are in the fragment are included
    std::vector<int> local_map(state.N, -1);
    for (uint32_t i = 0; i < static_cast<uint32_t>(indices.size()); ++i) {
        local_map[indices[i]] = static_cast<int>(i);
    }

    for (const auto& edge : state.B) {
        int li = (edge.i < state.N) ? local_map[edge.i] : -1;
        int lj = (edge.j < state.N) ? local_map[edge.j] : -1;
        if (li >= 0 && lj >= 0) {
            BondRecord br;
            br.i = static_cast<uint32_t>(li);
            br.j = static_cast<uint32_t>(lj);
            br.order = 1;  // Default single bond (topology does not store order yet)
            frag.bonds.push_back(br);
            frag.cyclic = true;  // Rough heuristic: presence of internal bonds
        }
    }

    // Cyclic detection: use union-find to count connected components.
    // A graph contains a cycle iff edges > vertices - components.
    // This handles disconnected fragments (e.g. isolated atoms inflate the
    // naive vertex count and incorrectly suppress cyclic detection).
    {
        const int UF_N = static_cast<int>(frag.num_atoms());
        std::vector<int> uf_parent(UF_N);
        for (int k = 0; k < UF_N; ++k) uf_parent[k] = k;

        auto uf_find = [&uf_parent](int x) {
            while (uf_parent[x] != x) {
                uf_parent[x] = uf_parent[uf_parent[x]];
                x = uf_parent[x];
            }
            return x;
        };

        int components = UF_N;
        for (const auto& b : frag.bonds) {
            int ri = uf_find(static_cast<int>(b.i));
            int rj = uf_find(static_cast<int>(b.j));
            if (ri != rj) {
                uf_parent[ri] = rj;
                --components;
            }
        }
        frag.cyclic = (frag.num_bonds() > frag.num_atoms() - components)
                      && (frag.num_atoms() > 2);
    }

    // --- Validate geometry: check for duplicate positions ---
    for (uint32_t i = 0; i < frag.num_atoms(); ++i) {
        for (uint32_t j = i + 1; j < frag.num_atoms(); ++j) {
            Vec3 d = frag.atoms[i].position - frag.atoms[j].position;
            double dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
            if (dist2 < 1e-10) {
                frag.status = FragmentStatus::DuplicatePositions;
                return frag;
            }
        }
    }

    // --- Compute local frame ---
    if (frag.num_atoms() < 2) {
        frag.frame.well_defined = false;
        if (frag.num_atoms() == 1) {
            frag.frame.origin = frag.atoms[0].position;
            frag.status = FragmentStatus::Valid;  // Single atom is valid but frameless
        }
        return frag;
    }

    Vec3 com = frag.center_of_mass();
    frag.frame.origin = com;

    // Build 3×3 inertia tensor (symmetric)
    double I[6] = {};  // xx, xy, xz, yy, yz, zz
    for (const auto& a : frag.atoms) {
        double dx = a.position.x - com.x;
        double dy = a.position.y - com.y;
        double dz = a.position.z - com.z;
        double m = a.mass;
        I[0] += m * (dy * dy + dz * dz);  // Ixx
        I[1] -= m * dx * dy;               // Ixy
        I[2] -= m * dx * dz;               // Ixz
        I[3] += m * (dx * dx + dz * dz);  // Iyy
        I[4] -= m * dy * dz;               // Iyz
        I[5] += m * (dx * dx + dy * dy);  // Izz
    }

    // Simple eigendecomposition for 3×3 symmetric matrix
    // Use Jacobi iteration (same approach as inertia_frame.hpp)
    double A[9] = {
        I[0], I[1], I[2],
        I[1], I[3], I[4],
        I[2], I[4], I[5]
    };
    double V[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // Eigenvector matrix (identity)

    // Jacobi rotation
    for (int iter = 0; iter < 100; ++iter) {
        // Find largest off-diagonal element
        int p = 0, q = 1;
        double max_off = std::abs(A[1]);
        if (std::abs(A[2]) > max_off) { p = 0; q = 2; max_off = std::abs(A[2]); }
        if (std::abs(A[5]) > max_off) { p = 1; q = 2; max_off = std::abs(A[5]); }
        if (max_off < 1e-15) break;

        double app = A[p * 3 + p], aqq = A[q * 3 + q], apq = A[p * 3 + q];
        double tau = (aqq - app) / (2.0 * apq);
        double t = (tau >= 0 ? 1.0 : -1.0) / (std::abs(tau) + std::sqrt(1.0 + tau * tau));
        double c = 1.0 / std::sqrt(1.0 + t * t);
        double s = t * c;

        // Update A
        double new_A[9];
        for (int i = 0; i < 9; ++i) new_A[i] = A[i];
        new_A[p * 3 + p] = c * c * app - 2 * s * c * apq + s * s * aqq;
        new_A[q * 3 + q] = s * s * app + 2 * s * c * apq + c * c * aqq;
        new_A[p * 3 + q] = 0.0;
        new_A[q * 3 + p] = 0.0;
        for (int i = 0; i < 3; ++i) {
            if (i != p && i != q) {
                double aip = A[i * 3 + p], aiq = A[i * 3 + q];
                new_A[i * 3 + p] = c * aip - s * aiq;
                new_A[p * 3 + i] = new_A[i * 3 + p];
                new_A[i * 3 + q] = s * aip + c * aiq;
                new_A[q * 3 + i] = new_A[i * 3 + q];
            }
        }
        for (int i = 0; i < 9; ++i) A[i] = new_A[i];

        // Update V
        for (int i = 0; i < 3; ++i) {
            double vip = V[i * 3 + p], viq = V[i * 3 + q];
            V[i * 3 + p] = c * vip - s * viq;
            V[i * 3 + q] = s * vip + c * viq;
        }
    }

    // Sort eigenvalues (ascending) and corresponding eigenvectors
    double eig[3] = {A[0], A[4], A[8]};
    int order[3] = {0, 1, 2};
    if (eig[order[0]] > eig[order[1]]) std::swap(order[0], order[1]);
    if (eig[order[1]] > eig[order[2]]) std::swap(order[1], order[2]);
    if (eig[order[0]] > eig[order[1]]) std::swap(order[0], order[1]);

    frag.frame.axis1 = {V[0 * 3 + order[0]], V[1 * 3 + order[0]], V[2 * 3 + order[0]]};
    frag.frame.axis2 = {V[0 * 3 + order[1]], V[1 * 3 + order[1]], V[2 * 3 + order[1]]};
    frag.frame.axis3 = {V[0 * 3 + order[2]], V[1 * 3 + order[2]], V[2 * 3 + order[2]]};

    // Ensure right-handedness: axis3 = axis1 × axis2
    Vec3 cross{
        frag.frame.axis1.y * frag.frame.axis2.z - frag.frame.axis1.z * frag.frame.axis2.y,
        frag.frame.axis1.z * frag.frame.axis2.x - frag.frame.axis1.x * frag.frame.axis2.z,
        frag.frame.axis1.x * frag.frame.axis2.y - frag.frame.axis1.y * frag.frame.axis2.x
    };
    double d = dot(cross, frag.frame.axis3);
    if (d < 0) {
        frag.frame.axis3.x = -frag.frame.axis3.x;
        frag.frame.axis3.y = -frag.frame.axis3.y;
        frag.frame.axis3.z = -frag.frame.axis3.z;
    }

    frag.frame.well_defined = true;
    frag.status = FragmentStatus::Valid;

    return frag;
}

} // namespace atomistic
