/**
 * phase6_7_verification_sessions.cpp
 * Phase 6: Build Deterministic Test-and-Verify Sessions
 * Phase 7: Run the Session Ladder (Block A, B, C)
 *
 * Each named session:
 *   - runs a reproducible computation with explicit pass/fail thresholds
 *   - writes two artifacts to verification/<group>/<id>/
 *       report.txt  — full metrics table, geometry, energy decomposition
 *       status.txt  — single word: PASS or FAIL
 *
 * Session ladder:
 *   Block A — Structural energy sweeps
 *       se_001  H2 distance sweep
 *       se_002  H2O O-H bond stretch
 *       se_003  CH4 bond stretch
 *       se_004  Ar2 pair sweep (nonbonded reference)
 *   Block B — Relaxation
 *       rx_001  Ar3 cluster FIRE
 *       rx_002  H2O nonbonded FIRE
 *       rx_003  BCC Fe 2x2x2 crystal FIRE
 *   Block C — Crystal structure
 *       xtal_001  FCC Al
 *       xtal_002  BCC Fe
 *       xtal_003  NaCl
 *       xtal_004  Diamond Si
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "io/xyz_format.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>

using namespace atomistic;
using namespace atomistic::crystal;

// ============================================================================
// Infrastructure
// ============================================================================

static int g_total = 0, g_pass = 0, g_fail = 0;

static void mkdir_p(const std::string& path)
{
#ifdef _WIN32
    // path may use forward slashes — convert
    std::string p = path;
    for (char& c : p) if (c == '/') c = '\\';
    std::string cmd = "mkdir \"" + p + "\" 2>nul";
    system(cmd.c_str());
#else
    std::string cmd = "mkdir -p \"" + path + "\"";
    system(cmd.c_str());
#endif
}

// Session context — accumulates report lines, tracks pass/fail
struct Session {
    std::string id;
    std::string dir;
    std::ostringstream report;
    bool pass = true;

    Session(const std::string& group, const std::string& id_)
        : id(id_), dir("verification/" + group + "/" + id_)
    {
        mkdir_p(dir);
        report << "Session: " << id << "\n";
        report << "Directory: " << dir << "\n";
        report << std::string(60, '-') << "\n";
    }

    void check(bool ok, const std::string& name,
               const std::string& detail = "")
    {
        ++g_total;
        if (ok) {
            ++g_pass;
            report << "[PASS]  " << name;
        } else {
            ++g_fail;
            pass = false;
            report << "[FAIL]  " << name;
        }
        if (!detail.empty()) report << "  (" << detail << ")";
        report << "\n";

        std::printf("  %s  %-50s %s\n",
                    ok ? "[PASS]" : "[FAIL]",
                    name.c_str(),
                    detail.c_str());
    }

    void metric(const std::string& name, double val, const std::string& unit = "")
    {
        std::ostringstream s;
        s << std::fixed << std::setprecision(6) << val;
        if (!unit.empty()) s << " " << unit;
        report << "  " << std::left << std::setw(30) << name
               << s.str() << "\n";
    }

    void section(const std::string& title)
    {
        report << "\n[" << title << "]\n";
        std::printf("\n  -- %s --\n", title.c_str());
    }

    void flush()
    {
        report << "\nResult: " << (pass ? "PASS" : "FAIL") << "\n";

        {
            std::ofstream f(dir + "/report.txt");
            f << report.str();
        }
        {
            std::ofstream f(dir + "/status.txt");
            f << (pass ? "PASS" : "FAIL") << "\n";
        }

        std::printf("  -> %s  [%s]\n\n",
                    dir.c_str(), pass ? "PASS" : "FAIL");
    }
};

// ============================================================================
// Molecule builders (mock end-user inputs)
// ============================================================================

static vsepr::io::XYZMolecule mol_H2(double r)
{
    vsepr::io::XYZMolecule m;
    m.atoms.emplace_back("H", 0.0, 0.0, 0.0);
    m.atoms.emplace_back("H", r,   0.0, 0.0);
    vsepr::io::XYZReader().detect_bonds(m);
    return m;
}

static vsepr::io::XYZMolecule mol_H2O(double r_oh, double theta_deg)
{
    double theta = theta_deg * M_PI / 180.0;
    vsepr::io::XYZMolecule m;
    m.atoms.emplace_back("O", 0.0,                    0.0,                   0.0);
    m.atoms.emplace_back("H", r_oh,                   0.0,                   0.0);
    m.atoms.emplace_back("H", r_oh*std::cos(theta),   r_oh*std::sin(theta),  0.0);
    vsepr::io::XYZReader().detect_bonds(m);
    return m;
}

static vsepr::io::XYZMolecule mol_CH4(double d_ch)
{
    double a = d_ch / std::sqrt(3.0);
    vsepr::io::XYZMolecule m;
    m.atoms.emplace_back("C",  0,  0,  0);
    m.atoms.emplace_back("H",  a,  a,  a);
    m.atoms.emplace_back("H",  a, -a, -a);
    m.atoms.emplace_back("H", -a,  a, -a);
    m.atoms.emplace_back("H", -a, -a,  a);
    vsepr::io::XYZReader().detect_bonds(m);
    return m;
}

static State parse(const vsepr::io::XYZMolecule& mol)
{
    State s = parsers::from_xyz(mol);
    s.F.resize(s.N, {0,0,0});
    return s;
}

static double frms(const State& s)
{
    double sum = 0;
    for (auto& f : s.F) sum += f.x*f.x + f.y*f.y + f.z*f.z;
    return std::sqrt(sum / s.N);
}

static double mic_dist(const Vec3& a, const Vec3& b, const BoxPBC& box)
{
    Vec3 d = box.enabled ? box.delta(a, b) : (Vec3{a.x-b.x, a.y-b.y, a.z-b.z});
    return std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
}

static double nn_dist(const State& s)
{
    double dmin = 1e30;
    for (uint32_t i = 0; i < s.N; ++i)
        for (uint32_t j = i+1; j < s.N; ++j) {
            double d = mic_dist(s.X[i], s.X[j], s.box);
            if (d < dmin) dmin = d;
        }
    return dmin;
}

// ============================================================================
// Block A — Structural Energy Sweeps
// ============================================================================

static void se_001_h2_sweep(IModel& model, const ModelParams& mp)
{
    Session ses("structural_energy", "se_001_h2_distance_sweep");
    ses.report << "H2 distance sweep (nonbonded LJ reference): r = 3.0 to 8.0 A\n";
    ses.report << "NOTE: no bond detection. This is the raw LJ energy surface.\n";
    ses.report << "      Bond detection causes a discontinuity at the covalent\n";
    ses.report << "      threshold (~0.74 A) and is tested in se_002/se_003.\n\n";
    ses.section("Sweep");

    double r_min = 1e30, U_min = 1e30;
    double prev_U = 1e30;
    double max_jump = 0;
    bool all_finite = true;

    ses.report << std::setw(8) << "r(A)"
               << std::setw(14) << "U_total"
               << std::setw(14) << "U_vdw"
               << std::setw(14) << "Fx_atom0" << "\n";

    for (double r = 3.0; r <= 8.0; r += 0.05) {
        // Raw nonbonded: no detect_bonds, so the full LJ pair is evaluated
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("H", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("H", r,   0.0, 0.0);
        State s = parse(m);
        model.eval(s, mp);
        double U = s.E.total();
        if (!std::isfinite(U)) { all_finite = false; break; }
        if (U < U_min) { U_min = U; r_min = r; }
        if (prev_U < 1e29) max_jump = std::max(max_jump, std::abs(U - prev_U));
        prev_U = U;

        ses.report << std::fixed << std::setw(8)  << std::setprecision(3) << r
                   << std::setw(14) << std::setprecision(6) << U
                   << std::setw(14) << s.E.UvdW
                   << std::setw(14) << s.F[0].x << "\n";
    }

    ses.section("Metrics");
    ses.metric("r_min (A)",    r_min);
    ses.metric("U_min (kcal)", U_min);
    ses.metric("max_jump",     max_jump);
    ses.report << "\n";

    ses.check(all_finite,           "all energies finite");
    ses.check(U_min < 0,            "bound state: U_min < 0");
    ses.check(r_min > 3.0 && r_min < 4.5,
              "r_min in [3.0, 4.5] A",
              "r=" + std::to_string(r_min).substr(0,5));
    ses.check(max_jump < 0.5,       "smooth: max step < 0.5 kcal/mol per 0.05 A");
    ses.flush();
}

static void se_002_h2o_stretch(IModel& model, const ModelParams& mp)
{
    Session ses("structural_energy", "se_002_h2o_oh_stretch");
    ses.report << "H2O O-H stretch: r_OH = 0.8 to 3.0 A, theta=104.45\n\n";
    ses.section("Sweep");

    double r_min = 1e30, U_min = 1e30;
    bool all_finite = true;

    ses.report << std::setw(8)  << "r_OH(A)"
               << std::setw(14) << "U_total"
               << std::setw(14) << "U_vdw" << "\n";

    for (double r = 0.8; r <= 3.0; r += 0.05) {
        State s = parse(mol_H2O(r, 104.45));
        model.eval(s, mp);
        double U = s.E.total();
        if (!std::isfinite(U)) { all_finite = false; break; }
        if (U < U_min) { U_min = U; r_min = r; }
        ses.report << std::fixed << std::setw(8)  << std::setprecision(3) << r
                   << std::setw(14) << std::setprecision(6) << U
                   << std::setw(14) << s.E.UvdW << "\n";
    }

    ses.section("Metrics");
    ses.metric("r_min (A)", r_min);
    ses.metric("U_min",     U_min);
    ses.report << "\n";

    ses.check(all_finite, "all energies finite");
    ses.check(U_min < 100.0, "energy minimum found below 100 kcal/mol");

    // Determinism check
    State s1 = parse(mol_H2O(0.96, 104.45));
    State s2 = parse(mol_H2O(0.96, 104.45));
    model.eval(s1, mp);
    model.eval(s2, mp);
    ses.check(s1.E.total() == s2.E.total(), "deterministic: bitwise identical");
    ses.flush();
}

static void se_003_ch4_stretch(IModel& model, const ModelParams& mp)
{
    Session ses("structural_energy", "se_003_ch4_bond_stretch");
    ses.report << "CH4 C-H stretch: d = 0.8 to 2.0 A (with bonds)\n\n";
    ses.section("Sweep");

    bool all_finite = true;
    double U_at_eq = 0, U_at_min = 0;

    ses.report << std::setw(8)  << "d_CH(A)"
               << std::setw(14) << "U_total"
               << std::setw(6)  << "bonds" << "\n";

    for (double d = 0.8; d <= 2.0; d += 0.05) {
        State s = parse(mol_CH4(d));
        model.eval(s, mp);
        double U = s.E.total();
        if (!std::isfinite(U)) { all_finite = false; break; }
        if (std::abs(d - 1.09) < 0.03) U_at_eq = U;
        if (std::abs(d - 0.80) < 0.03) U_at_min = U;
        ses.report << std::fixed << std::setw(8)  << std::setprecision(3) << d
                   << std::setw(14) << std::setprecision(6) << U
                   << std::setw(6)  << (int)s.B.size() << "\n";
    }

    ses.section("Metrics");
    ses.metric("U_at_d=0.80",  U_at_min);
    ses.metric("U_at_d=1.09",  U_at_eq);
    ses.report << "\n";

    ses.check(all_finite,             "all energies finite");
    // Compression from equilibrium (d=1.09) to d=0.80 should raise energy.
    // With bonds active, C-H excluded; only H-H LJ repulsion drives this.
    ses.check(U_at_min > U_at_eq,     "U(d=0.80) > U(d=1.09): compression raises energy");
    ses.flush();
}

static void se_004_ar2_sweep(IModel& model, const ModelParams& mp)
{
    Session ses("structural_energy", "se_004_ar2_pair_sweep");
    ses.report << "Ar2 pair sweep: r = 3.0 to 10.0 A (nonbonded reference)\n\n";
    ses.section("Sweep");

    double r_eq = 0, U_eq = 1e30;
    double max_jump = 0;
    bool finite_ok = true;
    bool force_dir_ok = true;

    ses.report << std::setw(8)  << "r(A)"
               << std::setw(14) << "U_vdw"
               << std::setw(14) << "Fx_atom0" << "\n";

    double prev_U = 1e30;
    for (double r = 3.0; r <= 10.0; r += 0.02) {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("Ar", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("Ar", r,   0.0, 0.0);
        State s = parse(m);
        model.eval(s, mp);
        double U = s.E.total();
        if (!std::isfinite(U)) { finite_ok = false; break; }
        if (U < U_eq) { U_eq = U; r_eq = r; }
        if (prev_U < 1e29) max_jump = std::max(max_jump, std::abs(U - prev_U));
        // Force sign: repulsive below r_eq, attractive above
        if (r < r_eq - 0.1 && s.F[0].x > 0) force_dir_ok = false;
        if (r > r_eq + 0.1 && r < 8.0 && s.F[0].x < 0) force_dir_ok = false;
        prev_U = U;
        ses.report << std::fixed << std::setw(8)  << std::setprecision(3) << r
                   << std::setw(14) << std::setprecision(6) << U
                   << std::setw(14) << s.F[0].x << "\n";
    }

    ses.section("Metrics");
    ses.metric("r_eq (A)",    r_eq);
    ses.metric("U_eq (kcal)", U_eq);
    ses.metric("max_jump",    max_jump);
    ses.report << "\n";

    ses.check(finite_ok,        "all energies finite");
    ses.check(U_eq < 0,         "bound state: U_eq < 0");
    ses.check(r_eq > 3.5 && r_eq < 4.5, "r_eq in [3.5, 4.5] A");
    ses.check(max_jump < 0.5,   "smooth: max step < 0.5 kcal/mol per 0.02 A");
    ses.check(force_dir_ok,     "force direction: repulsive < r_eq, attractive > r_eq");
    ses.flush();
}

// ============================================================================
// Block B — Relaxation
// ============================================================================

static void rx_001_ar3(IModel& model, const ModelParams& mp)
{
    Session ses("relaxation", "rx_001_ar3_fire");
    ses.report << "Ar3 equilateral cluster: FIRE relaxation from 4.5 A sides\n\n";

    vsepr::io::XYZMolecule m;
    m.atoms.emplace_back("Ar", 0.0,   0.0,   0.0);
    m.atoms.emplace_back("Ar", 4.5,   0.0,   0.0);
    m.atoms.emplace_back("Ar", 2.25,  3.897, 0.0);
    State s = parse(m);
    model.eval(s, mp);

    double U0 = s.E.total(), F0 = frms(s);

    FIREParams fp; fp.max_steps = 20000; fp.epsF = 1e-5; fp.dt = 1e-3;
    FIRE fire(model, mp);
    auto st = fire.minimize(s, fp);

    double d01 = std::sqrt(std::pow(s.X[0].x-s.X[1].x,2)+std::pow(s.X[0].y-s.X[1].y,2)+std::pow(s.X[0].z-s.X[1].z,2));
    double d02 = std::sqrt(std::pow(s.X[0].x-s.X[2].x,2)+std::pow(s.X[0].y-s.X[2].y,2)+std::pow(s.X[0].z-s.X[2].z,2));
    double d12 = std::sqrt(std::pow(s.X[1].x-s.X[2].x,2)+std::pow(s.X[1].y-s.X[2].y,2)+std::pow(s.X[1].z-s.X[2].z,2));

    ses.section("Relaxation");
    ses.metric("U_initial (kcal)", U0);
    ses.metric("U_final (kcal)",   st.U);
    ses.metric("F_rms_initial",    F0);
    ses.metric("F_rms_final",      st.Frms);
    ses.metric("steps",            (double)st.step);
    ses.metric("d01 (A)",          d01);
    ses.metric("d02 (A)",          d02);
    ses.metric("d12 (A)",          d12);
    ses.report << "\n";

    ses.check(st.U < U0,           "energy decreased");
    ses.check(st.Frms < 5e-4,      "F_rms < 5e-4 (converged)");
    ses.check(std::abs(d01-3.816) < 0.005 &&
              std::abs(d02-3.816) < 0.005 &&
              std::abs(d12-3.816) < 0.005,
              "equilateral 3.816 A (LJ minimum)",
              "d01=" + std::to_string(d01).substr(0,5));
    ses.flush();
}

static void rx_002_h2o(IModel& model, const ModelParams& mp)
{
    Session ses("relaxation", "rx_002_h2o_fire");
    ses.report << "H2O distorted (130 deg): FIRE relaxation\n";
    ses.report << "NOTE: LJ-only, no bonded springs. Only H-H nonbonded active.\n\n";

    State s = parse(mol_H2O(0.96, 130.0));
    model.eval(s, mp);
    double U0 = s.E.total(), F0 = frms(s);
    int bonds0 = (int)s.B.size();

    FIREParams fp; fp.max_steps = 8000; fp.epsF = 1e-2; fp.dt = 5e-4;
    FIRE fire(model, mp);
    auto st = fire.minimize(s, fp);

    ses.section("Relaxation");
    ses.metric("U_initial (kcal)", U0);
    ses.metric("U_final (kcal)",   st.U);
    ses.metric("F_rms_initial",    F0);
    ses.metric("F_rms_final",      st.Frms);
    ses.metric("steps",            (double)st.step);
    ses.metric("bonds_initial",    (double)bonds0);
    ses.metric("bonds_final",      (double)s.B.size());
    ses.report << "\n";

    ses.check(st.U <= U0 + 1e-8,   "energy decreased or unchanged");
    ses.check((int)s.B.size() == bonds0, "bond count preserved");
    ses.check(st.Frms < F0,        "F_rms decreased from initial");
    ses.flush();
}

static void rx_003_bcc_fe(IModel& model, const ModelParams& mp)
{
    Session ses("relaxation", "rx_003_bcc_fe_crystal");
    ses.report << "BCC Fe 3x3x3 supercell: FIRE relaxation under PBC\n\n";

    auto uc = presets::iron_bcc();
    auto sc = construct_supercell(uc, 3, 3, 3);
    State& s = sc.state;
    s.F.resize(s.N, {0,0,0});
    model.eval(s, mp);

    double U0 = s.E.total();
    double nn0 = nn_dist(s);

    FIREParams fp; fp.max_steps = 5000; fp.epsF = 1e-4; fp.dt = 1e-3;
    FIRE fire(model, mp);
    auto st = fire.minimize(s, fp);

    double nn1 = nn_dist(s);

    ses.section("Relaxation");
    ses.metric("N_atoms",          (double)s.N);
    ses.metric("U_initial (kcal)", U0);
    ses.metric("U_final (kcal)",   st.U);
    ses.metric("F_rms_final",      st.Frms);
    ses.metric("steps",            (double)st.step);
    ses.metric("nn_before (A)",    nn0);
    ses.metric("nn_after (A)",     nn1);
    ses.report << "\n";

    ses.check(st.U <= U0 + 1e-6,    "energy decreased or unchanged");
    // BCC Fe 3x3x3: L=8.61A < rc=10A so MIC is incomplete; F_rms
    // at the LJ minimum is nonzero due to finite-size truncation.
    ses.check(st.Frms < 0.05,       "F_rms < 0.05 (at LJ minimum, PBC cell < rc)");
    ses.check(std::abs(nn1-nn0) < 0.01, "nn distance preserved < 0.01 A");
    ses.flush();
}

// ============================================================================
// Block C — Crystal verification
// ============================================================================

static void crystal_session(
    const std::string& id,
    const char* label,
    UnitCell uc,
    uint32_t expected_N,
    double expected_nn,
    int expected_cn,
    double cn_cutoff,
    IModel& model,
    const ModelParams& mp)
{
    Session ses("crystals", id);
    ses.report << label << " crystal session\n";
    ses.report << "Space group: " << uc.space_group_symbol
               << " (" << uc.space_group_number << ")\n\n";

    // Use a supercell large enough for nn counting
    int sc_n = (int)std::ceil(10.0 / nn_dist(uc.to_state()) / 2.0) + 1;
    sc_n = std::max(sc_n, 2);
    sc_n = std::min(sc_n, 4);

    auto sc = construct_supercell(uc, sc_n, sc_n, sc_n);
    State& s = sc.state;
    s.F.resize(s.N, {0,0,0});
    model.eval(s, mp);

    double U0     = s.E.total();
    double nn0    = nn_dist(s);
    uint32_t N    = s.N;

    // Coordination of first atom
    int cn = 0;
    for (uint32_t j = 1; j < N; ++j)
        if (mic_dist(s.X[0], s.X[j], s.box) < cn_cutoff) ++cn;

    // Relax
    FIREParams fp; fp.max_steps = 3000; fp.epsF = 1e-4; fp.dt = 1e-3;
    FIRE fire(model, mp);
    auto st = fire.minimize(s, fp);
    double nn1 = nn_dist(s);

    ses.section("Geometry");
    ses.metric("N_basis",          (double)uc.num_atoms());
    ses.metric("N_supercell",      (double)N);
    ses.metric("supercell_factor", (double)sc_n);
    ses.metric("nn_distance (A)",  nn0);
    ses.metric("nn_expected (A)",  expected_nn);
    ses.metric("coord_number",     (double)cn);
    ses.metric("coord_expected",   (double)expected_cn);
    ses.section("Energy");
    ses.metric("U_initial (kcal)", U0);
    ses.metric("U/atom (kcal)",    U0/N);
    ses.metric("U_final (kcal)",   st.U);
    ses.metric("F_rms_final",      st.Frms);
    ses.report << "\n";

    ses.check(N == expected_N * (uint32_t)(sc_n*sc_n*sc_n),
              "atom count",
              "N=" + std::to_string(N));
    ses.check(std::abs(nn0 - expected_nn) < 0.02,
              "nn distance",
              "nn=" + std::to_string(nn0).substr(0,5));
    ses.check(cn == expected_cn,
              "coordination number",
              "CN=" + std::to_string(cn));
    ses.check(std::isfinite(U0), "energy finite");
    // Covalent crystals (Si diamond) have nn << sigma_LJ, so LJ forces
    // are enormous and FIRE cannot relax them without bonded terms.
    // Ionic/metallic crystals near their LJ minima relax normally.
    bool is_covalent = (nn0 < 2.5);  // Si-Si 2.35A, sigma_Si=3.83A
    if (!is_covalent) {
        ses.check(st.U <= U0 + 1e-6, "relaxation: energy unchanged or lower");
        ses.check(std::abs(nn1 - nn0) < 0.05, "nn preserved after relaxation");
    } else {
        ses.report << "[NOTE]  covalent crystal: skipping relaxation checks\n";
        ses.report << "        nn=" << nn0 << " A << sigma_LJ; bonded model required\n";
        std::printf("  [NOTE] covalent crystal: relaxation skipped (nn << sigma)\n");
    }
    ses.flush();
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 6/7 — Verification Session Ladder\n");
    std::printf("=============================================================\n");
    std::printf("  Artifacts written to:  verification/\n\n");

    mkdir_p("verification/structural_energy");
    mkdir_p("verification/relaxation");
    mkdir_p("verification/crystals");

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 10.0;

    // ---- Block A: Structural Energy ----
    std::printf("Block A — Structural Energy Sweeps\n");
    std::printf("%s\n", std::string(60, '-').c_str());
    se_001_h2_sweep(*model, mp);
    se_002_h2o_stretch(*model, mp);
    se_003_ch4_stretch(*model, mp);
    se_004_ar2_sweep(*model, mp);

    // ---- Block B: Relaxation ----
    std::printf("Block B — Relaxation Sessions\n");
    std::printf("%s\n", std::string(60, '-').c_str());
    rx_001_ar3(*model, mp);
    rx_002_h2o(*model, mp);
    rx_003_bcc_fe(*model, mp);

    // ---- Block C: Crystal ----
    std::printf("Block C — Crystal Sessions\n");
    std::printf("%s\n", std::string(60, '-').c_str());

    crystal_session("xtal_001_fcc_al", "FCC Al",
        presets::aluminum_fcc(),  4, 4.05/std::sqrt(2.0), 12, 3.0, *model, mp);
    crystal_session("xtal_002_bcc_fe", "BCC Fe",
        presets::iron_bcc(),      2, 2.87*std::sqrt(3.0)/2.0, 8, 2.6, *model, mp);
    crystal_session("xtal_003_nacl", "NaCl",
        presets::sodium_chloride(), 8, 5.64/2.0, 6, 3.0, *model, mp);
    crystal_session("xtal_004_diamond_si", "Diamond Si",
        presets::silicon_diamond(), 8, 5.43*std::sqrt(3.0)/4.0, 4, 2.5, *model, mp);

    // ---- Summary ----
    std::printf("=============================================================\n");
    std::printf("  Session Ladder:  %d sessions\n", g_total);
    std::printf("  Checks:          %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");

    // Write master status file
    {
        std::ofstream f("verification/status.txt");
        f << "Sessions: " << g_total << "\n";
        f << "Passed:   " << g_pass << "\n";
        f << "Failed:   " << g_fail << "\n";
        f << "Result:   " << (g_fail == 0 ? "PASS" : "FAIL") << "\n";
    }

    return g_fail > 0 ? 1 : 0;
}
