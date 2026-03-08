#include "supercell.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <numeric>

namespace atomistic {
namespace crystal {

// ============================================================================
// Approximate covalent radii for bond inference (Å)
// ============================================================================

namespace {

double covalent_radius(uint32_t Z) {
    // Cordero et al. 2008, Dalton Trans. 2832-2838
    static const double radii[] = {
        0.00,                                               // 0
        0.31, 0.28,                                         // H, He
        1.28, 0.96, 0.84, 0.76, 0.71, 0.66, 0.57, 0.58,   // Li-Ne
        1.66, 1.41, 1.21, 1.11, 1.07, 1.05, 1.02, 1.06,   // Na-Ar
        2.03, 1.76, 1.70, 1.60, 1.53, 1.39, 1.39, 1.32,   // K-Fe
        1.26, 1.24, 1.32, 1.22, 1.22, 1.20, 1.19, 1.20,   // Co-Se
        1.20, 1.16,                                         // Br, Kr
        2.20, 1.95, 1.90, 1.75, 1.64, 1.54, 1.47, 1.46,   // Rb-Ru
        1.42, 1.39, 1.45, 1.44, 1.42, 1.39, 1.39, 1.38,   // Rh-Te
        1.39, 1.40,                                         // I, Xe
        2.44, 2.15, 2.07, 2.04, 2.03, 2.01, 1.99, 1.98,   // Cs-Gd
        1.94, 1.92, 1.92, 1.89, 1.90, 1.87, 1.87, 1.75,   // Tb-Hf
        1.70, 1.62, 1.51, 1.44, 1.41, 1.36, 1.36, 1.32,   // Ta-Pt
        1.36, 1.45, 1.46, 1.48, 1.40, 1.50, 1.50, 1.60    // Au-Rn
    };
    if (Z < sizeof(radii)/sizeof(radii[0])) return radii[Z];
    return 1.50;  // Fallback for heavy elements
}

} // anonymous namespace

// ============================================================================
// Supercell Construction
// ============================================================================

SupercellResult construct_supercell(const UnitCell& uc, int na, int nb, int nc) {
    State unit_state = uc.to_state();
    auto result = construct_supercell(unit_state, uc.lattice, na, nb, nc);

    result.recipe.add("load", "unit_cell: " + uc.name +
                      " (" + std::to_string(uc.num_atoms()) + " atoms)");

    std::ostringstream oss;
    oss << "supercell(" << na << "," << nb << "," << nc << ") -> "
        << result.total_atoms << " atoms";
    result.recipe.add("supercell", oss.str());

    return result;
}

SupercellResult construct_supercell(const State& unit_state,
                                    const Lattice& lattice,
                                    int na, int nb, int nc) {
    SupercellResult result;
    result.na = na;
    result.nb = nb;
    result.nc = nc;

    uint32_t N_unit = unit_state.N;
    uint32_t N_total = N_unit * static_cast<uint32_t>(na * nb * nc);
    result.total_atoms = N_total;

    // Build expanded lattice
    Vec3 a_vec = lattice.A.col(0);
    Vec3 b_vec = lattice.A.col(1);
    Vec3 c_vec = lattice.A.col(2);

    Vec3 sa = a_vec * static_cast<double>(na);
    Vec3 sb = b_vec * static_cast<double>(nb);
    Vec3 sc = c_vec * static_cast<double>(nc);
    result.lattice = Lattice(sa, sb, sc);

    // Build state
    State& s = result.state;
    s.N = N_total;
    s.X.reserve(N_total);
    s.V.resize(N_total, {0, 0, 0});
    s.F.resize(N_total, {0, 0, 0});
    s.M.reserve(N_total);
    s.Q.reserve(N_total);
    s.type.reserve(N_total);
    s.T.resize(N_total, 0.0);

    // Replicate: r_{i,pqr} = r_i + p·a + q·b + r·c
    for (int p = 0; p < na; ++p) {
        for (int q = 0; q < nb; ++q) {
            for (int r = 0; r < nc; ++r) {
                Vec3 shift = a_vec * static_cast<double>(p)
                           + b_vec * static_cast<double>(q)
                           + c_vec * static_cast<double>(r);

                for (uint32_t i = 0; i < N_unit; ++i) {
                    s.X.push_back(unit_state.X[i] + shift);
                    s.M.push_back(unit_state.M[i]);
                    s.Q.push_back(unit_state.Q[i]);
                    s.type.push_back(unit_state.type[i]);
                }
            }
        }
    }

    // Set PBC box from expanded lattice
    s.box = result.lattice.to_box_pbc();

    return result;
}

// ============================================================================
// Bond Inference
// ============================================================================

InferredBonds infer_bonds_from_distances(const State& s,
                                         const Lattice& lattice,
                                         double f) {
    InferredBonds result;
    result.count = 0;

    // Convert all positions to fractional for MIC
    std::vector<Vec3> frac(s.N);
    for (uint32_t i = 0; i < s.N; ++i) {
        frac[i] = lattice.to_fractional(s.X[i]);
    }

    for (uint32_t i = 0; i < s.N; ++i) {
        for (uint32_t j = i + 1; j < s.N; ++j) {
            double r = lattice.distance(frac[i], frac[j]);
            double r_cov_sum = covalent_radius(s.type[i]) + covalent_radius(s.type[j]);
            double threshold = f * r_cov_sum;

            if (r < threshold && r > 0.1) {
                result.edges.push_back({i, j});
                ++result.count;
            }
        }
    }

    return result;
}

std::vector<uint32_t> coordination_numbers(const State& s,
                                           const Lattice& lattice,
                                           double f) {
    auto bonds = infer_bonds_from_distances(s, lattice, f);
    std::vector<uint32_t> coord(s.N, 0);

    for (const auto& e : bonds.edges) {
        ++coord[e.i];
        ++coord[e.j];
    }

    return coord;
}

// ============================================================================
// Validation
// ============================================================================

ConstructionReport validate_construction(
    double E_unit_per_atom,
    double E_super_per_atom,
    uint32_t n_bonds_unit,
    uint32_t n_bonds_super,
    int replication_product)
{
    ConstructionReport report;

    // Strain detection
    if (std::abs(E_unit_per_atom) > 1e-15) {
        report.strain = std::abs(E_super_per_atom - E_unit_per_atom)
                      / std::abs(E_unit_per_atom);
    } else {
        report.strain = std::abs(E_super_per_atom - E_unit_per_atom);
    }
    report.strain_pass = (report.strain < 0.01);

    // Bond count consistency
    uint32_t expected_bonds = n_bonds_unit * static_cast<uint32_t>(replication_product);
    if (expected_bonds > 0) {
        report.bond_count_error = std::abs(
            static_cast<double>(n_bonds_super) - static_cast<double>(expected_bonds))
            / static_cast<double>(expected_bonds);
    } else {
        report.bond_count_error = (n_bonds_super > 0) ? 1.0 : 0.0;
    }
    report.bond_count_pass = (report.bond_count_error < 0.05);

    report.mean_coordination = 0.0;
    report.coord_stddev = 0.0;

    return report;
}

// ============================================================================
// Coordinate Wrapping
// ============================================================================

void wrap_positions(State& s) {
    if (!s.box.enabled) return;
    for (uint32_t i = 0; i < s.N; ++i) {
        s.X[i] = s.box.wrap(s.X[i]);
    }
}

void wrap_positions(State& s, const Lattice& lattice) {
    for (uint32_t i = 0; i < s.N; ++i) {
        Vec3 f = lattice.to_fractional(s.X[i]);
        f = Lattice::wrap_frac(f);
        s.X[i] = lattice.to_cartesian(f);
    }
}

} // namespace crystal
} // namespace atomistic
