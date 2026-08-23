#include "atomistic/core/state.hpp"
#include "atomistic/models/eam_sutton_chen.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace atomistic;

namespace {

constexpr double KCAL_PER_EV = 23.0609;

struct MetalCase {
    const char* symbol;
    int Z;
    double a_exp_ang;
    double ecoh_exp_ev;
};

struct ScanResult {
    double a_best = 0.0;
    double ecoh_best_ev = std::numeric_limits<double>::infinity();
};

State build_fcc_supercell(int cells, double a_ang) {
    State s;
    const double basis[4][3] = {
        {0.0, 0.0, 0.0},
        {0.5, 0.5, 0.0},
        {0.5, 0.0, 0.5},
        {0.0, 0.5, 0.5},
    };

    for (int ix = 0; ix < cells; ++ix) {
        for (int iy = 0; iy < cells; ++iy) {
            for (int iz = 0; iz < cells; ++iz) {
                for (const auto& b : basis) {
                    s.X.push_back(Vec3{
                        (static_cast<double>(ix) + b[0]) * a_ang,
                        (static_cast<double>(iy) + b[1]) * a_ang,
                        (static_cast<double>(iz) + b[2]) * a_ang});
                }
            }
        }
    }

    s.N = static_cast<uint32_t>(s.X.size());
    s.V.assign(s.N, Vec3{0.0, 0.0, 0.0});
    s.F.assign(s.N, Vec3{0.0, 0.0, 0.0});
    s.Q.assign(s.N, 0.0);
    s.M.assign(s.N, 1.0);
    s.type.assign(s.N, 0);
    s.box.set_dimensions(cells * a_ang, cells * a_ang, cells * a_ang);
    return s;
}

double energy_per_atom_ev(int Z, double a_ang, int cells) {
    State s = build_fcc_supercell(cells, a_ang);

    SuttonChenEAM model;
    model.set_z_map({{0, Z}});

    ModelParams params;
    s.E = EnergyTerms{};
    model.eval(s, params);
    return s.E.total() / static_cast<double>(s.N) / KCAL_PER_EV;
}

ScanResult scan_fcc_minimum(const MetalCase& metal) {
    ScanResult best;
    const double a_min = metal.a_exp_ang * 0.90;
    const double a_max = metal.a_exp_ang * 1.10;

    for (double a = a_min; a <= a_max + 1.0e-12; a += 0.005) {
        const double e = energy_per_atom_ev(metal.Z, a, 3);
        if (std::isfinite(e) && e < best.ecoh_best_ev) {
            best.ecoh_best_ev = e;
            best.a_best = a;
        }
    }

    return best;
}

bool check_close(double actual, double expected, double tolerance, const std::string& label) {
    const double err = std::abs(actual - expected);
    if (err <= tolerance) {
        return true;
    }

    std::cerr << "FAIL " << label << ": actual=" << actual
              << " expected=" << expected
              << " tolerance=" << tolerance << "\n";
    return false;
}

bool check_relative(double actual, double expected, double tolerance, const std::string& label) {
    const double denom = std::max(std::abs(expected), 1.0e-12);
    const double err = std::abs(actual - expected) / denom;
    if (err <= tolerance) {
        return true;
    }

    std::cerr << "FAIL " << label << ": actual=" << actual
              << " expected=" << expected
              << " rel_err=" << err
              << " tolerance=" << tolerance << "\n";
    return false;
}

} // namespace

int main() {
    const std::vector<MetalCase> metals = {
        {"Al", 13, 4.050, -3.39},
        {"Ni", 28, 3.520, -4.44},
        {"Cu", 29, 3.615, -3.49},
        {"Pd", 46, 3.890, -3.89},
        {"Ag", 47, 4.090, -2.95},
        {"Au", 79, 4.080, -3.81},
    };

    int failures = 0;

    std::cout << "Sutton-Chen EAM FCC metal-host validation\n";
    std::cout << std::left << std::setw(4) << "el"
              << std::right << std::setw(10) << "a_scan"
              << std::setw(12) << "a_exp"
              << std::setw(14) << "Ecoh_scan"
              << std::setw(12) << "Ecoh_exp"
              << std::setw(10) << "ratio" << "\n";

    for (const auto& metal : metals) {
        const auto scan = scan_fcc_minimum(metal);
        const double ratio = scan.ecoh_best_ev / metal.ecoh_exp_ev;

        std::cout << std::left << std::setw(4) << metal.symbol
                  << std::right << std::fixed << std::setprecision(3)
                  << std::setw(10) << scan.a_best
                  << std::setw(12) << metal.a_exp_ang
                  << std::setprecision(4)
                  << std::setw(14) << scan.ecoh_best_ev
                  << std::setw(12) << metal.ecoh_exp_ev
                  << std::setprecision(3)
                  << std::setw(10) << ratio << "\n";

        if (!check_close(scan.a_best, metal.a_exp_ang, 0.020,
                         std::string(metal.symbol) + " lattice constant")) {
            ++failures;
        }
        if (!check_relative(scan.ecoh_best_ev, metal.ecoh_exp_ev, 0.040,
                            std::string(metal.symbol) + " cohesive energy")) {
            ++failures;
        }
    }

    if (failures == 0) {
        std::cout << "All FCC EAM metal-host checks passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
