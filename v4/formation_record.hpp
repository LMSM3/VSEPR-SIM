#pragma once
/**
 * formation_record.hpp — Version 4.0.4.04 Formation Record
 * ═══════════════════════════════════════════════════════
 * Complete per-case record matching the Day #47 spreadsheet columns
 * plus new V4 meta-scores (gamma, data quality, compactness).
 *
 * Anti-black-box: every field is public, inspectable, and deterministic.
 *
 * C++26 features:
 *   - CONTRACT_PRE / CONTRACT_POST emulation (GCC 14)
 *   - -ftrivial-auto-var-init=pattern erroneous-behaviour trapping
 *   - Trailing return types throughout
 *   - Structured bindings in consumers
 *
 * Build:  g++ -std=c++23 -O3 -ftrivial-auto-var-init=pattern ...
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace v4 {

// ============================================================================
// Contract Emulation (C++26 [[pre:]] / [[post:]] not yet in GCC 14)
// ============================================================================

#ifndef V4_CONTRACT_PRE
#define V4_CONTRACT_PRE(cond)                                                 \
    do { if (!(cond)) {                                                       \
        __builtin_trap();                                                     \
    }} while(0)
#endif

#ifndef V4_CONTRACT_POST
#define V4_CONTRACT_POST(cond)                                                \
    do { if (!(cond)) {                                                       \
        __builtin_trap();                                                     \
    }} while(0)
#endif

// ============================================================================
// Lattice Class
// ============================================================================

enum class LatticeClass : uint8_t {
    FCC = 0,
    BCC = 1,
    HCP = 2,
    UNKNOWN = 255
};

inline auto lattice_name(LatticeClass lc) -> const char* {
    switch (lc) {
        case LatticeClass::FCC: return "FCC";
        case LatticeClass::BCC: return "BCC";
        case LatticeClass::HCP: return "HCP";
        default:                return "UNKNOWN";
    }
}

inline auto parse_lattice(std::string_view s) -> LatticeClass {
    if (s == "FCC" || s == "fcc") return LatticeClass::FCC;
    if (s == "BCC" || s == "bcc") return LatticeClass::BCC;
    if (s == "HCP" || s == "hcp") return LatticeClass::HCP;
    return LatticeClass::UNKNOWN;
}

// ============================================================================
// Formation Record — Day #47 spreadsheet + V4 meta-scores
// ============================================================================

static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

/**
 * FormationRecord — one complete formation case.
 *
 * Layout mirrors the Day #47 reference spreadsheet:
 *   A: symbol       B: name       C: structure   D: n_beads
 *   E: converged    F: steps      G: rms_force   H: avg_eta
 *   I: avg_rho      J: avg_C      K: final_energy
 *   L: elapsed_ms   M: n_l3_domains
 *   N: macro_rigidity   O: macro_ductility   P: macro_color
 *   Q: Ecoh_eV      R: Tmelt_K    S: a0_ang
 *   T: B_GPa        U: G_GPa      V: E_GPa
 *
 * V4 extensions:
 *   gamma           data_quality   compactness
 */
struct FormationRecord {
    // ── Identity ──────────────────────────────────────────────────────────
    std::string  symbol;          // e.g. "Au", "Fe", "Ti"
    std::string  name;            // e.g. "Gold", "Iron", "Titanium"
    LatticeClass structure{LatticeClass::UNKNOWN};
    int          n_beads{64};

    // ── Convergence ───────────────────────────────────────────────────────
    bool   converged{false};
    int    steps{0};
    double rms_force{NaN};

    // ── Formation averages ────────────────────────────────────────────────
    double avg_eta{NaN};
    double avg_rho{NaN};
    double avg_C{NaN};
    double final_energy{NaN};

    // ── Timing / domain ───────────────────────────────────────────────────
    double elapsed_ms{NaN};
    int    n_l3_domains{0};

    // ── Macro precursors ──────────────────────────────────────────────────
    double macro_rigidity{NaN};
    double macro_ductility{NaN};
    double macro_color{NaN};

    // ── Experimental reference ────────────────────────────────────────────
    double Ecoh_eV{NaN};
    double Tmelt_K{NaN};
    double a0_ang{NaN};
    double B_GPa{NaN};
    double G_GPa{NaN};
    double E_GPa{NaN};

    // ── V4 meta-scores (computed post-formation) ──────────────────────────
    double gamma{NaN};           // exploration richness  ∈ [0,1]
    double data_quality{NaN};    // storage worthiness    ∈ [0,1]
    double compactness{NaN};     // structural tightness  ∈ [0,1]

    // ── Total reportable field count (for S_intensity computation) ────────
    static constexpr int FIELD_COUNT = 22;

    // ── Utility ───────────────────────────────────────────────────────────

    /// Count how many numeric fields are populated (finite and non-zero).
    auto populated_fields() const -> int {
        int count = 0;
        auto check = [&](double v) {
            if (std::isfinite(v) && v != 0.0) ++count;
        };
        check(rms_force);    check(avg_eta);     check(avg_rho);
        check(avg_C);        check(final_energy); check(elapsed_ms);
        check(macro_rigidity); check(macro_ductility); check(macro_color);
        check(Ecoh_eV);     check(Tmelt_K);     check(a0_ang);
        check(B_GPa);       check(G_GPa);       check(E_GPa);
        // n_l3_domains, steps, n_beads are int — check separately
        if (steps > 0)         ++count;
        if (n_beads > 0)       ++count;
        if (n_l3_domains > 0)  ++count;
        // converged is boolean — always "populated"
        ++count;
        // symbol/name populated if non-empty
        if (!symbol.empty()) ++count;
        if (!name.empty())   ++count;
        return count;
    }

    /// Whether this record is minimally valid for scoring.
    auto is_scoreable() const -> bool {
        return !symbol.empty()
            && structure != LatticeClass::UNKNOWN
            && n_beads > 0
            && steps > 0;
    }
};

// ============================================================================
// Day #47 Reference Dataset — 12 elements
// ============================================================================

/**
 * Embeds the 12-element reference data from the Day #47 spreadsheet.
 * Used for validation, correlation testing, and initial calibration.
 */
inline auto reference_dataset() -> std::array<FormationRecord, 12> {
    std::array<FormationRecord, 12> d{};

    auto make = [](const char* sym, const char* nm, LatticeClass lc,
                   int nb, bool conv, int st, double rms, double eta,
                   double rho, double C, double energy, double ms,
                   int l3, double mr, double md, double mc,
                   double ecoh, double tmelt, double a0,
                   double B, double G, double E) -> FormationRecord
    {
        FormationRecord r;
        r.symbol = sym;   r.name = nm;   r.structure = lc;   r.n_beads = nb;
        r.converged = conv; r.steps = st;  r.rms_force = rms;
        r.avg_eta = eta;  r.avg_rho = rho; r.avg_C = C;
        r.final_energy = energy;  r.elapsed_ms = ms;  r.n_l3_domains = l3;
        r.macro_rigidity = mr;  r.macro_ductility = md;  r.macro_color = mc;
        r.Ecoh_eV = ecoh; r.Tmelt_K = tmelt; r.a0_ang = a0;
        r.B_GPa = B;      r.G_GPa = G;      r.E_GPa = E;
        return r;
    };

    using LC = LatticeClass;
    //                  sym  name        lc       nb  conv   steps   rms_f    eta       rho       C         energy      ms     l3  mr     md     mc     ecoh  tmelt  a0     B     G     E
    d[ 0] = make("Au", "Gold",       LC::FCC, 64, true,  1698, 0.00745, 0.279298, 5.200771, 25.65625, -10367.9,  104.7,  1, 0.165, 0.4542, 0.4737, -3.81, 1337.3, 4.078, 180, 27,  79);
    d[ 1] = make("Ag", "Silver",     LC::FCC, 64, true,     6, 0.0,    0.000701, 0.0,      0.0,       0.0,       0.26,  0, 0.0,   0.0,    0.0,    -2.95, 1234.9, 4.086, 103, 30,  83);
    d[ 2] = make("Cu", "Copper",     LC::FCC, 64, true,     5, 0.0,    0.000931, 0.0,      0.0,       0.0,       0.23,  0, 0.0,   0.0,    0.0,    -3.49, 1357.8, 3.615, 142, 48, 130);
    d[ 3] = make("Pt", "Platinum",   LC::FCC, 64, false, 6000, 0.0,    0.0,      0.0,      0.0,       0.0,     263.32,  7, 0.1779,0.4327, 0.3967, -5.84, 2041.4, 3.924, 278, 61, 168);
    d[ 4] = make("Ni", "Nickel",     LC::FCC, 64, false, 5000, 0.0,    0.0,      0.0,      0.0,       0.0,     216.6,   7, 0.1776,0.3981, 0.4516, -4.44, 1728.0, 3.524, 180, 76, 200);
    d[ 5] = make("Al", "Aluminium",  LC::FCC, 64, true,     5, 0.000017,0.000637, 0.0,     0.0,      -0.00077,  0.21,   0, 0.0,   0.0,    0.0,    -3.39, 933.5,  4.05,  76,  26,  70);
    d[ 6] = make("Fe", "Iron",       LC::BCC, 64, true,  2668, 0.011844,0.469197,11.80109, 45.34375, -15735.7, 179.15,  1, 0.238, 0.5048, 0.6086, -4.28, 1811.0, 2.87,  170, 82, 211);
    d[ 7] = make("W",  "Tungsten",   LC::BCC, 64, true,  2954, 0.011784,0.494255, 9.393497,40.09375, -33206.8, 198.85,  1, 0.246, 0.504,  0.6159, -8.9,  3695.0, 3.165, 310,161, 411);
    d[ 8] = make("Mo", "Molybdenum", LC::BCC, 64, true,  2811, 0.011542,0.454102, 9.521529,40.3125,  -25012.7, 195.27,  1, 0.2292,0.4949, 0.59,   -6.82, 2896.0, 3.147, 230,120, 329);
    d[ 9] = make("Cr", "Chromium",   LC::BCC, 64, true,  2684, 0.011993,0.467368,11.66662, 45.0,     -15065.4, 178.89,  1, 0.237, 0.504,  0.6069, -4.1,  2180.0, 2.885, 160,115, 279);
    d[10] = make("Ti", "Titanium",   LC::HCP, 64, true,    62, 0.000013,0.000697, 0.0,     0.0,      -0.00012,  2.45,   0, 0.0,   0.0,    0.0,    -4.85, 1941.0, 2.95,  110, 44, 116);
    d[11] = make("Co", "Cobalt",     LC::HCP, 64, false, 6000, 0.0,    0.0,      0.0,      0.0,       0.0,     253.53,  5, 0.0914,0.3787, 0.3609, -4.39, 1768.0, 2.507, 180, 75, 211);

    return d;
}

} // namespace v4
