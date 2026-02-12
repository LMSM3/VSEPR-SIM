#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <utility>
#include <cmath>
#include <limits>

namespace atomistic {

struct Vec3 {
    double x{}, y{}, z{};
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
};

inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }

/**
 * Periodic boundary conditions (orthogonal box)
 * Minimal implementation for force evaluation with MIC.
 * 
 * Math: Δr_ij = r_j - r_i
 *       Δr_ij ← Δr_ij - L * round(Δr_ij / L)  [component-wise]
 * 
 * This wraps displacements into (-L/2, L/2] for minimum image convention.
 */
struct BoxPBC {
    Vec3 L;       // Box lengths (Lx, Ly, Lz)
    Vec3 invL;    // Cached 1/L for performance
    bool enabled; // PBC on/off flag

    BoxPBC() : L{0,0,0}, invL{0,0,0}, enabled(false) {}

    explicit BoxPBC(double Lx, double Ly, double Lz)
        : L{Lx, Ly, Lz}
        , invL{(Lx > 0 ? 1.0/Lx : 0), (Ly > 0 ? 1.0/Ly : 0), (Lz > 0 ? 1.0/Lz : 0)}
        , enabled(Lx > 0 && Ly > 0 && Lz > 0)
    {}

    // Minimum image displacement: dr = rj - ri, wrapped into (-L/2, L/2]
    Vec3 delta(const Vec3& ri, const Vec3& rj) const {
        Vec3 dr = rj - ri;
        if (!enabled) return dr;

        // Component-wise: dr -= L * round(dr/L)
        dr.x -= L.x * std::round(dr.x * invL.x);
        dr.y -= L.y * std::round(dr.y * invL.y);
        dr.z -= L.z * std::round(dr.z * invL.z);

        return dr;
    }

    // Wrap position into primary cell [0, L)
    Vec3 wrap(const Vec3& r) const {
        if (!enabled) return r;

        return {
            r.x - L.x * std::floor(r.x * invL.x),
            r.y - L.y * std::floor(r.y * invL.y),
            r.z - L.z * std::floor(r.z * invL.z)
        };
    }
};

struct Edge { uint32_t i{}, j{}; };                 // bonds/constraints graph B ⊆ V×V
struct Event { uint64_t step{}; std::string tag; }; // L: event log

struct EnergyTerms {
    double Ubond{};
    double Uangle{};
    double Utors{};
    double UvdW{};
    double UCoul{};
    double Uext{};
    double total() const { return Ubond + Uangle + Utors + UvdW + UCoul + Uext; }
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
};

inline bool sane(const State& s) {
    if (s.N == 0) return false;
    if (s.X.size() != s.N || s.V.size() != s.N || s.Q.size() != s.N || s.M.size() != s.N || s.type.size() != s.N) return false;
    return true;
}

} // namespace atomistic
