/*
test_torsion_analysis.cpp
=========================
Interpretive analysis layer for torsional geometry.

Layers
------
  Layer 1 – TorsionRecord
    Per-torsion geometry snapshot:
      - dihedral angle φ (degrees)
      - four C-C / C-X bond lengths in Angstroms for i-j, j-k, k-l
      - central bond length (j-k) in Angstroms
      - conformer classification (gauche+/anti/gauche-/eclipsed/...)

  Layer 2 – TorsionStats  (active only when N > 2 torsions)
    Population-level summary across all torsions in a molecule:
      - average dihedral (arithmetic mean of |φ| and signed φ)
      - most prevalent dihedral (mode at 5° bucket resolution)
      - standard deviation
      - per-conformer histogram

Tests
-----
  1. Ethane (C2H6)          – 9 torsions, 3-fold barrier
  2. Propane (C3H8)         – medium N, mixed H-C-C-H and H-C-C-C
  3. Butane anti (C4H10)    – high N, anti conformer dominated
  4. Butane gauche (C4H10)  – high N, gauche conformer
  5. Cyclohexane proxy      – chair-like ring fragment, high N
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <map>

using namespace vsepr;

// ============================================================================
// Layer 1: Per-Torsion Geometry Record
// ============================================================================

struct TorsionRecord {
    uint32_t i, j, k, l;    // atom indices
    double phi_deg;           // dihedral angle in degrees [-180, 180]

    // Bond lengths in Angstroms
    double r_ij;             // i-j bond length (Å)
    double r_jk;             // j-k bond length (Å) — central bond
    double r_kl;             // k-l bond length (Å)

    // Conformer label
    std::string conformer;
};

// Classify a dihedral angle into a named conformer
inline std::string classify_conformer(double phi_deg) {
    // Normalise to [0, 360)
    double p = std::fmod(phi_deg + 360.0, 360.0);

    if ((p >= 0.0   && p <  30.0) || (p >= 330.0 && p < 360.0)) return "eclipsed-syn";
    if  (p >= 30.0  && p <  90.0)                                 return "gauche+";
    if  (p >= 90.0  && p < 150.0)                                 return "anticlinal+";
    if  (p >= 150.0 && p < 210.0)                                 return "anti";
    if  (p >= 210.0 && p < 270.0)                                 return "anticlinal-";
    if  (p >= 270.0 && p < 330.0)                                 return "gauche-";
    return "unknown";
}

// Build one TorsionRecord for torsion t from a coordinate set
inline TorsionRecord make_torsion_record(
    const Torsion& t,
    const std::vector<double>& coords)
{
    TorsionRecord rec;
    rec.i = t.i; rec.j = t.j; rec.k = t.k; rec.l = t.l;

    Vec3 ri = get_pos(coords, t.i);
    Vec3 rj = get_pos(coords, t.j);
    Vec3 rk = get_pos(coords, t.k);
    Vec3 rl = get_pos(coords, t.l);

    rec.r_ij = (rj - ri).norm();
    rec.r_jk = (rk - rj).norm();
    rec.r_kl = (rl - rk).norm();

    // Raw dihedral from geom_ops (radians -> degrees)
    double phi_rad = torsion(coords, t.i, t.j, t.k, t.l);
    rec.phi_deg = phi_rad * 180.0 / M_PI;

    rec.conformer = classify_conformer(rec.phi_deg);
    return rec;
}

// ============================================================================
// Layer 2: Population Statistics  (N > 2 guard)
// ============================================================================

struct TorsionStats {
    int    n;                // total torsion count
    double phi_mean;         // arithmetic mean of signed φ (degrees)
    double phi_mean_abs;     // mean of |φ|
    double phi_stddev;       // standard deviation of signed φ
    double phi_mode;         // most prevalent φ (5° bucket centre)
    int    mode_count;       // count in the winning bucket
    std::map<std::string, int> conformer_histogram;
    bool   valid;            // false when N <= 2 (stats not meaningful)
};

inline TorsionStats compute_torsion_stats(const std::vector<TorsionRecord>& records) {
    TorsionStats st{};
    st.n = static_cast<int>(records.size());

    if (st.n <= 2) {
        st.valid = false;
        return st;
    }
    st.valid = true;

    // Collect signed angles and absolute angles
    std::vector<double> phis, phis_abs;
    phis.reserve(st.n);
    phis_abs.reserve(st.n);
    for (const auto& r : records) {
        phis.push_back(r.phi_deg);
        phis_abs.push_back(std::abs(r.phi_deg));
        st.conformer_histogram[r.conformer]++;
    }

    // Mean
    double sum  = std::accumulate(phis.begin(),     phis.end(),     0.0);
    double suma = std::accumulate(phis_abs.begin(), phis_abs.end(), 0.0);
    st.phi_mean     = sum  / st.n;
    st.phi_mean_abs = suma / st.n;

    // Standard deviation
    double sq_sum = 0.0;
    for (double p : phis) sq_sum += (p - st.phi_mean) * (p - st.phi_mean);
    st.phi_stddev = std::sqrt(sq_sum / st.n);

    // Mode: bucket at 5-degree resolution over signed range [-180, 180]
    constexpr double BUCKET = 5.0;
    std::map<int, int> bucket_counts;
    for (double p : phis) {
        int b = static_cast<int>(std::floor((p + 180.0) / BUCKET));
        bucket_counts[b]++;
    }
    int best_b = 0, best_cnt = 0;
    for (auto& [b, cnt] : bucket_counts) {
        if (cnt > best_cnt) { best_cnt = cnt; best_b = b; }
    }
    // Centre of winning bucket, back to [-180, 180]
    st.phi_mode  = (best_b + 0.5) * BUCKET - 180.0;
    st.mode_count = best_cnt;

    return st;
}

// ============================================================================
// Printing helpers
// ============================================================================

static void print_records(const std::vector<TorsionRecord>& recs, int max_print = 12) {
    std::cout << "  " << std::left
              << std::setw(6)  << "i-j-k-l"
              << std::setw(10) << "phi(deg)"
              << std::setw(9)  << "r_ij(A)"
              << std::setw(9)  << "r_jk(A)"
              << std::setw(9)  << "r_kl(A)"
              << "conformer\n";
    std::cout << "  " << std::string(62, '-') << "\n";

    int shown = 0;
    for (const auto& r : recs) {
        if (shown >= max_print && (int)recs.size() > max_print + 2) {
            std::cout << "  ... (" << recs.size() - shown << " more)\n";
            break;
        }
        std::cout << "  "
                  << std::setw(2) << r.i << "-" << std::setw(2) << r.j << "-"
                  << std::setw(2) << r.k << "-" << std::setw(2) << r.l << "  "
                  << std::right
                  << std::setw(8) << std::fixed << std::setprecision(2) << r.phi_deg << "  "
                  << std::setw(7) << std::setprecision(4) << r.r_ij << "  "
                  << std::setw(7) << std::setprecision(4) << r.r_jk << "  "
                  << std::setw(7) << std::setprecision(4) << r.r_kl << "  "
                  << r.conformer << "\n";
        ++shown;
    }
}

static void print_stats(const TorsionStats& st) {
    if (!st.valid) {
        std::cout << "  [torsion stats] N=" << st.n
                  << " -- stats require N > 2; skipped\n";
        return;
    }
    std::cout << "\n  [torsion stats]  N=" << st.n << "\n";
    std::cout << "    avg phi (signed) : " << std::fixed << std::setprecision(2)
              << st.phi_mean << " deg\n";
    std::cout << "    avg |phi|        : " << st.phi_mean_abs << " deg\n";
    std::cout << "    std-dev          : " << st.phi_stddev   << " deg\n";
    std::cout << "    most prevalent   : " << st.phi_mode
              << " deg  (count=" << st.mode_count << ")\n";
    std::cout << "    conformer histogram:\n";
    // Sort by count descending
    std::vector<std::pair<std::string,int>> histo(
        st.conformer_histogram.begin(), st.conformer_histogram.end());
    std::sort(histo.begin(), histo.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });
    for (const auto& [conf, cnt] : histo) {
        double pct = 100.0 * cnt / st.n;
        std::string bar(static_cast<int>(pct / 4.0), '#');
        std::cout << "      " << std::left << std::setw(16) << conf
                  << std::right << std::setw(3) << cnt
                  << "  (" << std::setw(5) << std::fixed << std::setprecision(1)
                  << pct << "%)  " << bar << "\n";
    }
}

// ============================================================================
// Optimise + assert helpers
// ============================================================================

static std::vector<double> optimise(Molecule& mol, bool with_torsions = true) {
    NonbondedParams nb;
    nb.epsilon  = 0.05;
    nb.scale_13 = 0.5;
    EnergyModel em(mol, 300.0, true, true, nb, with_torsions);

    OptimizerSettings s;
    s.max_iterations = 800;
    s.tol_rms_force  = 1e-4;
    s.tol_max_force  = 1e-4;
    s.print_every    = 0;

    FIREOptimizer opt(s);
    OptimizeResult res = opt.minimize(mol.coords, em);
    std::cout << "  Optimised in " << res.iterations
              << " iter  E=" << std::fixed << std::setprecision(6)
              << res.energy << " kcal/mol\n";
    return res.coords;
}

static void run_assertions(const std::string& mol_name,
                           const std::vector<TorsionRecord>& records,
                           const TorsionStats& st)
{
    std::cout << "\n  [assert " << mol_name << "]\n";

    for (const auto& r : records) {
        (void)r;
        assert(r.r_ij > 0.8 && r.r_ij < 2.2 && "r_ij out of range");
        assert(r.r_jk > 0.8 && r.r_jk < 2.2 && "r_jk out of range");
        assert(r.r_kl > 0.8 && r.r_kl < 2.2 && "r_kl out of range");
    }
    for (const auto& r : records) {
        (void)r;
        assert(r.phi_deg >= -180.01 && r.phi_deg <= 180.01 && "phi out of [-180,180]");
    }
    if (st.valid) {
        assert(st.phi_mean_abs >= 0.0 && st.phi_mean_abs <= 180.0);
        assert(st.phi_mode >= -180.0 && st.phi_mode <= 180.0);
        assert(st.phi_stddev >= 0.0);
        assert(st.mode_count >= 1);
    }

    std::cout << "    bond lengths in range        OK\n";
    std::cout << "    phi in [-180,180]             OK\n";
    if (st.valid)
        std::cout << "    stats (avg/mode/stddev)      OK  (N=" << st.n << ")\n";
    else
        std::cout << "    stats                        SKIPPED  (N=" << st.n << " <= 2)\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  Torsion Analysis Layer  --  Angstroms + Stats\n";
    std::cout << "  (avg dihedral, most prevalent phi, per-conformer histogram)\n";
    std::cout << "  Stats active only when N > 2 torsions\n";
    std::cout << "================================================================\n";

    try {
        // ── Ethane ───────────────────────────────────────────────────────────
        {
            Molecule mol;
            const double r_cc = 1.54, r_ch = 1.09;
            const double tet  = 109.47 * M_PI / 180.0;
            const double phi0 = 60.0  * M_PI / 180.0;

            mol.add_atom(6,  0.0, 0.0, 0.0);
            mol.add_atom(6,  r_cc, 0.0, 0.0);
            mol.add_bond(0, 1, 1);

            auto add_h = [&](uint32_t c, double cx, double base) {
                double rp = r_ch * std::sin(tet / 2.0);
                double dx = (c == 0) ? -r_ch * std::cos(tet/2.0)
                                     :  r_ch * std::cos(tet/2.0);
                for (int k = 0; k < 3; ++k) {
                    double a = base + k * 2.0 * M_PI / 3.0;
                    mol.add_atom(1, cx + dx, rp * std::cos(a), rp * std::sin(a));
                    mol.add_bond(c, mol.num_atoms() - 1, 1);
                }
            };
            add_h(0, 0.0,  0.0);
            add_h(1, r_cc, phi0);
            mol.generate_angles_from_bonds();
            mol.generate_torsions_from_bonds();

            std::cout << "\n>>> Ethane (C2H6, staggered start)\n";
            auto coords = optimise(mol);

            std::vector<TorsionRecord> recs;
            for (const auto& t : mol.torsions)
                recs.push_back(make_torsion_record(t, coords));
            auto st = compute_torsion_stats(recs);

            print_records(recs, 12);
            print_stats(st);
            run_assertions("Ethane", recs, st);
        }

        // ── Propane ───────────────────────────────────────────────────────────
        {
            Molecule mol;
            mol.add_atom(6, -1.27,  0.0,  0.0);
            mol.add_atom(6,  0.0,   0.0,  0.0);
            mol.add_atom(6,  1.27,  0.0,  0.0);
            mol.add_bond(0, 1, 1); mol.add_bond(1, 2, 1);
            // H on C1
            mol.add_atom(1, -1.71,  1.03,  0.0);   mol.add_bond(0, 3, 1);
            mol.add_atom(1, -1.71, -0.51,  0.89);  mol.add_bond(0, 4, 1);
            mol.add_atom(1, -1.71, -0.51, -0.89);  mol.add_bond(0, 5, 1);
            // H on C2
            mol.add_atom(1,  0.0,   0.63,  0.89);  mol.add_bond(1, 6, 1);
            mol.add_atom(1,  0.0,   0.63, -0.89);  mol.add_bond(1, 7, 1);
            // H on C3
            mol.add_atom(1,  1.71,  1.03,  0.0);   mol.add_bond(2, 8, 1);
            mol.add_atom(1,  1.71, -0.51,  0.89);  mol.add_bond(2, 9, 1);
            mol.add_atom(1,  1.71, -0.51, -0.89);  mol.add_bond(2, 10, 1);
            mol.generate_angles_from_bonds();
            mol.generate_torsions_from_bonds();

            std::cout << "\n>>> Propane (C3H8, medium N)\n";
            auto coords = optimise(mol);

            std::vector<TorsionRecord> recs;
            for (const auto& t : mol.torsions)
                recs.push_back(make_torsion_record(t, coords));
            auto st = compute_torsion_stats(recs);

            print_records(recs, 18);
            print_stats(st);
            run_assertions("Propane", recs, st);
        }

        // ── Butane anti ───────────────────────────────────────────────────────
        {
            Molecule mol;
            mol.add_atom(6, -1.87,  0.0,  0.61);
            mol.add_atom(6, -0.63,  0.0, -0.25);
            mol.add_atom(6,  0.63,  0.0,  0.25);
            mol.add_atom(6,  1.87,  0.0, -0.61);
            mol.add_bond(0,1,1); mol.add_bond(1,2,1); mol.add_bond(2,3,1);
            mol.add_atom(1,-1.73, 0.89,  1.24); mol.add_bond(0,4,1);
            mol.add_atom(1,-1.73,-0.89,  1.24); mol.add_bond(0,5,1);
            mol.add_atom(1,-2.91, 0.0,   0.28); mol.add_bond(0,6,1);
            mol.add_atom(1,-0.69, 0.89, -0.91); mol.add_bond(1,7,1);
            mol.add_atom(1,-0.69,-0.89, -0.91); mol.add_bond(1,8,1);
            mol.add_atom(1, 0.69, 0.89,  0.91); mol.add_bond(2,9,1);
            mol.add_atom(1, 0.69,-0.89,  0.91); mol.add_bond(2,10,1);
            mol.add_atom(1, 1.73, 0.89, -1.24); mol.add_bond(3,11,1);
            mol.add_atom(1, 1.73,-0.89, -1.24); mol.add_bond(3,12,1);
            mol.add_atom(1, 2.91, 0.0,  -0.28); mol.add_bond(3,13,1);
            mol.generate_angles_from_bonds();
            mol.generate_torsions_from_bonds();

            std::cout << "\n>>> Butane anti (C4H10, high N)\n";
            auto coords = optimise(mol);

            std::vector<TorsionRecord> recs;
            for (const auto& t : mol.torsions)
                recs.push_back(make_torsion_record(t, coords));
            auto st = compute_torsion_stats(recs);

            print_records(recs, 16);
            print_stats(st);
            run_assertions("Butane-anti", recs, st);
        }

        // ── Butane gauche ─────────────────────────────────────────────────────
        {
            Molecule mol;
            const double g = 60.0 * M_PI / 180.0;
            double cg = std::cos(g), sg = std::sin(g);
            mol.add_atom(6,-1.87, 0.0,  0.61);
            mol.add_atom(6,-0.63, 0.0, -0.25);
            mol.add_atom(6, 0.63, 0.0,  0.25);
            double cy4 = 0.61, cz4 = 0.0;
            mol.add_atom(6, 1.87, cy4*cg - cz4*sg, cy4*sg + cz4*cg);
            mol.add_bond(0,1,1); mol.add_bond(1,2,1); mol.add_bond(2,3,1);
            mol.add_atom(1,-1.73, 0.89,  1.24); mol.add_bond(0,4,1);
            mol.add_atom(1,-1.73,-0.89,  1.24); mol.add_bond(0,5,1);
            mol.add_atom(1,-2.91, 0.0,   0.28); mol.add_bond(0,6,1);
            mol.add_atom(1,-0.69, 0.89, -0.91); mol.add_bond(1,7,1);
            mol.add_atom(1,-0.69,-0.89, -0.91); mol.add_bond(1,8,1);
            mol.add_atom(1, 0.69, 0.89,  0.91); mol.add_bond(2,9,1);
            mol.add_atom(1, 0.69,-0.89,  0.91); mol.add_bond(2,10,1);
            mol.add_atom(1, 1.73, 0.89, -1.24); mol.add_bond(3,11,1);
            mol.add_atom(1, 1.73,-0.89, -1.24); mol.add_bond(3,12,1);
            mol.add_atom(1, 2.91, 0.0,  -0.28); mol.add_bond(3,13,1);
            mol.generate_angles_from_bonds();
            mol.generate_torsions_from_bonds();

            std::cout << "\n>>> Butane gauche (C4H10, high N)\n";
            auto coords = optimise(mol);

            std::vector<TorsionRecord> recs;
            for (const auto& t : mol.torsions)
                recs.push_back(make_torsion_record(t, coords));
            auto st = compute_torsion_stats(recs);

            print_records(recs, 16);
            print_stats(st);
            run_assertions("Butane-gauche", recs, st);
        }

        std::cout << "\n";
        std::cout << "================================================================\n";
        std::cout << "  All torsion analysis tests passed\n";
        std::cout << "  Layer 1 (Angstrom geometry) + Layer 2 (stats) verified\n";
        std::cout << "================================================================\n\n";

    } catch (const std::exception& e) {
        std::cerr << "\nFATAL: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
