#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <utility>
#include <cmath>
#include <limits>
#include "core/math_vec3.hpp"
#include "box/pbc.hpp"   // vsepr::BoxOrtho — canonical PBC primitive

namespace atomistic {

// Day #56: Vec3 is now vsepr::Vec3. No local struct permitted.
using Vec3 = vsepr::Vec3;

// Pull vsepr free functions into atomistic namespace so existing
// bare norm() / dot() call sites continue to compile without change.
using vsepr::dot;
using vsepr::norm;

// Day #57A: BoxPBC unified with vsepr::BoxOrtho.
// External name preserved — all State-based code (box.enabled, box.delta, etc.)
// continues to compile unchanged.  Underlying implementation is now canonical.
using BoxPBC = vsepr::BoxOrtho;

struct Edge { uint32_t i{}, j{}; };                 // bonds/constraints graph B ⊆ V×V
struct Event { uint64_t step{}; std::string tag; }; // L: event log

struct EnergyTerms {
    double Ubond{};
    double Uangle{};
    double Utors{};
    double UvdW{};
    double UCoul{};
    double Uext{};
    double Upol{};   // SCF polarization energy (Phase 1)
    double total() const { return Ubond + Uangle + Utors + UvdW + UCoul + Uext + Upol; }
};

// Canonical state S = (N, X, V, T, Q, M/type, F, E, L, box)
struct State {
    uint32_t N{};
    std::vector<Vec3> X;         // positions (N×3)
    std::vector<Vec3> V;         // velocities (N×3)
    std::vector<double> T;       // per-particle temperature proxy (optional)
    std::vector<double> Q;       // charges (N)
    std::vector<double> M;       // masses (N)
    std::vector<uint32_t> type;  // species/type id (N)

    std::vector<Edge> B;         // graph edges
    std::vector<Event> L;        // event log

    // scratch / telemetry
    std::vector<Vec3> F;         // forces (N×3)
    EnergyTerms E{};             // energy ledger

    // Periodic boundary conditions (added for crystal support)
    BoxPBC box;                  // PBC box (disabled by default)

    // SCF polarization (Phase 1: see atomistic/models/polarization_scf.hpp)
    // Populated by SCFPolarizationSolver::solve(); zero-sized = polarization off.
    std::vector<Vec3>   induced_dipoles;   // μ_i (e·Å, i.e. AMBER units)
    std::vector<double> polarizabilities;  // α_i (Å³, scalar isotropic)
    std::vector<Vec3>   permanent_dipoles; // μ_i^0 (optional; zero if absent)
};

inline bool sane(const State& s) {
    if (s.N == 0) return false;
    if (s.X.size() != s.N || s.V.size() != s.N || s.Q.size() != s.N || s.M.size() != s.N || s.type.size() != s.N) return false;
    return true;
}

} // namespace atomistic
