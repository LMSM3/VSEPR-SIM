#ifndef VSEPR_TYPES_H
#define VSEPR_TYPES_H

#include <cstdint>

namespace vsepr {

// ============================================================================
// Core Atom Type
// ============================================================================
struct Atom {
    uint32_t id;      // Unique atom identifier
    uint8_t  Z;       // Atomic number (1 = H, 6 = C, etc.)
    double   mass;    // Atomic mass (amu)
    uint8_t  lone_pairs;  // Number of lone pairs (for VSEPR, optional override)
    uint32_t flags;   // Bit flags for properties (frozen, constrained, etc.)
};

// ============================================================================
// Topology: Connectivity and Geometric Terms
// ============================================================================

// Bond between atoms i and j
struct Bond {
    uint32_t i, j;    // Atom indices
    uint8_t  order;   // Bond order (1 = single, 2 = double, 3 = triple)
};

// Angle term: i-j-k (j is the vertex)
struct Angle {
    uint32_t i, j, k; // Atom indices
};

// Proper torsion (dihedral): i-j-k-l
struct Torsion {
    uint32_t i, j, k, l; // Atom indices
};

// Improper torsion (out-of-plane): i-j-k-l
struct Improper {
    uint32_t i, j, k, l; // Atom indices
};

// ============================================================================
// Periodic Boundary Conditions (for future use)
// ============================================================================

// 3x3 matrix for lattice vectors
struct Mat3 {
    double data[3][3];
    
    Mat3() {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                data[i][j] = (i == j) ? 1.0 : 0.0;
    }
};

// Simulation cell with optional periodicity
struct Cell {
    Mat3 a;               // Lattice vectors (columns or rows - define convention later)
    bool periodic[3];     // Periodic in x, y, z?
    
    Cell() {
        periodic[0] = periodic[1] = periodic[2] = false;
    }
};

} // namespace vsepr

#endif // VSEPR_TYPES_H
