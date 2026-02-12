/**
 * decay_chains.cpp
 * ================
 * Implementation of the four natural radioactive decay series
 * Data sourced from NIST, IAEA Nuclear Data Services, and Brookhaven NuDat
 */

#include "core/decay_chains.hpp"
#include <memory>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>

namespace vsepr {
namespace nuclear {

// ============================================================================
// Constants
// ============================================================================

constexpr double LN2 = 0.693147180559945309417;
constexpr double SECONDS_PER_YEAR = 31557600.0;  // 365.25 days
constexpr double BQ_TO_CI = 2.7027e-11;           // 1 Bq = 2.7e-11 Ci

// ============================================================================
// DecayNuclide Methods
// ============================================================================

double DecayNuclide::decay_constant() const {
    if (is_stable()) return 0.0;
    return LN2 / half_life_seconds;
}

double DecayNuclide::activity_Bq(double N) const {
    return decay_constant() * N;
}

double DecayNuclide::activity_Ci(double N) const {
    return activity_Bq(N) * BQ_TO_CI;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string decay_mode_to_string(DecayMode mode) {
    switch (mode) {
        case DecayMode::Alpha: return "Alpha decay";
        case DecayMode::BetaMinus: return "Beta-minus decay";
        case DecayMode::BetaPlus: return "Beta-plus decay";
        case DecayMode::ElectronCapture: return "Electron capture";
        case DecayMode::Fission: return "Spontaneous fission";
        case DecayMode::Stable: return "Stable";
        default: return "Unknown";
    }
}

std::string decay_mode_to_symbol(DecayMode mode) {
    switch (mode) {
        case DecayMode::Alpha: return "α";
        case DecayMode::BetaMinus: return "β⁻";
        case DecayMode::BetaPlus: return "β⁺";
        case DecayMode::ElectronCapture: return "EC";
        case DecayMode::Fission: return "SF";
        case DecayMode::Stable: return "—";
        default: return "?";
    }
}

double years_to_seconds(double years) {
    return years * SECONDS_PER_YEAR;
}

double seconds_to_years(double seconds) {
    return seconds / SECONDS_PER_YEAR;
}

std::string format_half_life(double seconds) {
    if (seconds < 1e-6) {
        return std::to_string(seconds * 1e9) + " ns";
    } else if (seconds < 1e-3) {
        return std::to_string(seconds * 1e6) + " μs";
    } else if (seconds < 1.0) {
        return std::to_string(seconds * 1e3) + " ms";
    } else if (seconds < 60) {
        return std::to_string(seconds) + " s";
    } else if (seconds < 3600) {
        return std::to_string(seconds / 60.0) + " min";
    } else if (seconds < 86400) {
        return std::to_string(seconds / 3600.0) + " h";
    } else if (seconds < SECONDS_PER_YEAR) {
        return std::to_string(seconds / 86400.0) + " d";
    } else if (seconds < 1e9 * SECONDS_PER_YEAR) {
        return std::to_string(seconds_to_years(seconds)) + " y";
    } else {
        return std::to_string(seconds_to_years(seconds) / 1e9) + " Gy";
    }
}

int series_type_4n(uint16_t A) {
    return A % 4;
}

std::string series_name_for_A(uint16_t A) {
    int n = series_type_4n(A);
    if (n == 0) return "4n (Thorium)";
    if (n == 1) return "4n+1 (Neptunium)";
    if (n == 2) return "4n+2 (Uranium)";
    return "4n+3 (Actinium)";
}

// ============================================================================
// NaturalDecaySeries Constructor
// ============================================================================

NaturalDecaySeries::NaturalDecaySeries() {
    initialize_thorium_series();
    initialize_neptunium_series();
    initialize_uranium_series();
    initialize_actinium_series();
}

DecayNuclide NaturalDecaySeries::make_nuclide(
    uint8_t Z, uint16_t A,
    double half_life, const std::string& unit,
    DecayMode mode, double energy_MeV
) {
    DecayNuclide n;
    n.Z = Z;
    n.A = A;
    n.primary_decay = mode;
    n.decay_energy_MeV = energy_MeV;
    n.branching_ratio = 1.0;
    
    // Convert half-life to seconds
    if (unit == "y" || unit == "years") {
        n.half_life_seconds = years_to_seconds(half_life);
        n.half_life_years = half_life;
    } else if (unit == "d" || unit == "days") {
        n.half_life_seconds = half_life * 86400.0;
        n.half_life_years = seconds_to_years(n.half_life_seconds);
    } else if (unit == "h" || unit == "hours") {
        n.half_life_seconds = half_life * 3600.0;
        n.half_life_years = seconds_to_years(n.half_life_seconds);
    } else if (unit == "m" || unit == "minutes") {
        n.half_life_seconds = half_life * 60.0;
        n.half_life_years = seconds_to_years(n.half_life_seconds);
    } else if (unit == "s" || unit == "seconds") {
        n.half_life_seconds = half_life;
        n.half_life_years = seconds_to_years(half_life);
    } else if (unit == "ms") {
        n.half_life_seconds = half_life * 1e-3;
        n.half_life_years = seconds_to_years(n.half_life_seconds);
    } else if (unit == "μs" || unit == "us") {
        n.half_life_seconds = half_life * 1e-6;
        n.half_life_years = seconds_to_years(n.half_life_seconds);
    }
    
    n.half_life_unit = unit;
    return n;
}

// ============================================================================
// 1. THORIUM SERIES (4n): Th-232 → Pb-208
// ============================================================================

void NaturalDecaySeries::initialize_thorium_series() {
    thorium_.name = "Thorium Series";
    thorium_.series_type = "4n";
    thorium_.parent_Z = 90;
    thorium_.parent_A = 232;
    thorium_.stable_Z = 82;
    thorium_.stable_A = 208;
    
    // Complete decay chain
    thorium_.chain = {
        make_nuclide(90, 232, 14.05e9, "y", DecayMode::Alpha, 4.081),      // Th-232
        make_nuclide(88, 228, 5.75, "y", DecayMode::Alpha, 5.520),          // Ra-228
        make_nuclide(89, 228, 6.15, "h", DecayMode::BetaMinus, 2.124),      // Ac-228
        make_nuclide(90, 228, 1.912, "y", DecayMode::Alpha, 5.520),         // Th-228
        make_nuclide(88, 224, 3.66, "d", DecayMode::Alpha, 5.789),          // Ra-224
        make_nuclide(86, 220, 55.6, "s", DecayMode::Alpha, 6.405),          // Rn-220 (Thoron)
        make_nuclide(84, 216, 0.145, "s", DecayMode::Alpha, 6.906),         // Po-216
        make_nuclide(82, 212, 10.64, "h", DecayMode::BetaMinus, 0.570),     // Pb-212
        make_nuclide(83, 212, 60.55, "m", DecayMode::BetaMinus, 2.254),     // Bi-212 (64% β⁻)
        make_nuclide(84, 212, 299, "ns", DecayMode::Alpha, 8.954),          // Po-212 (from Bi-212 α branch 36%)
        make_nuclide(82, 208, 0.0, "y", DecayMode::Stable, 0.0)             // Pb-208 (stable)
    };
    
    thorium_.total_decays = 10;
    thorium_.alpha_decays = 6;
    thorium_.beta_decays = 4;
    thorium_.total_energy_MeV = 42.66;
    thorium_.longest_half_life_years = 14.05e9;
}

// ============================================================================
// 2. NEPTUNIUM SERIES (4n+1): Np-237 → Bi-209 [EXTINCT]
// ============================================================================

void NaturalDecaySeries::initialize_neptunium_series() {
    neptunium_.name = "Neptunium Series (Extinct)";
    neptunium_.series_type = "4n+1";
    neptunium_.parent_Z = 93;
    neptunium_.parent_A = 237;
    neptunium_.stable_Z = 83;
    neptunium_.stable_A = 209;
    
    // Complete decay chain (extinct in nature, but recreated artificially)
    neptunium_.chain = {
        make_nuclide(93, 237, 2.144e6, "y", DecayMode::Alpha, 4.959),      // Np-237
        make_nuclide(91, 233, 26.975, "d", DecayMode::BetaMinus, 0.571),   // Pa-233
        make_nuclide(92, 233, 1.592e5, "y", DecayMode::Alpha, 4.909),      // U-233
        make_nuclide(90, 229, 7340, "y", DecayMode::Alpha, 5.168),         // Th-229
        make_nuclide(88, 225, 14.9, "d", DecayMode::BetaMinus, 0.360),     // Ra-225
        make_nuclide(89, 225, 10.0, "d", DecayMode::Alpha, 5.935),         // Ac-225
        make_nuclide(87, 221, 4.9, "m", DecayMode::Alpha, 6.457),          // Fr-221
        make_nuclide(85, 217, 32.3, "ms", DecayMode::Alpha, 7.202),        // At-217
        make_nuclide(83, 213, 45.59, "m", DecayMode::BetaMinus, 1.423),    // Bi-213 (97.8% β⁻)
        make_nuclide(84, 213, 4.2, "μs", DecayMode::Alpha, 8.537),         // Po-213 (from Bi-213 α branch 2.2%)
        make_nuclide(82, 209, 3.253, "h", DecayMode::BetaMinus, 0.644),    // Pb-209
        make_nuclide(83, 209, 2.01e19, "y", DecayMode::Alpha, 3.137)       // Bi-209 (quasi-stable)
    };
    
    neptunium_.total_decays = 11;
    neptunium_.alpha_decays = 7;
    neptunium_.beta_decays = 4;
    neptunium_.total_energy_MeV = 52.54;
    neptunium_.longest_half_life_years = 2.144e6;
}

// ============================================================================
// 3. URANIUM SERIES (4n+2): U-238 → Pb-206
// ============================================================================

void NaturalDecaySeries::initialize_uranium_series() {
    uranium_.name = "Uranium Series (Radium Series)";
    uranium_.series_type = "4n+2";
    uranium_.parent_Z = 92;
    uranium_.parent_A = 238;
    uranium_.stable_Z = 82;
    uranium_.stable_A = 206;
    
    // Complete decay chain
    uranium_.chain = {
        make_nuclide(92, 238, 4.468e9, "y", DecayMode::Alpha, 4.270),      // U-238
        make_nuclide(90, 234, 24.10, "d", DecayMode::BetaMinus, 0.273),    // Th-234
        make_nuclide(91, 234, 6.70, "h", DecayMode::BetaMinus, 2.271),     // Pa-234
        make_nuclide(92, 234, 2.455e5, "y", DecayMode::Alpha, 4.859),      // U-234
        make_nuclide(90, 230, 75380, "y", DecayMode::Alpha, 4.770),        // Th-230
        make_nuclide(88, 226, 1600, "y", DecayMode::Alpha, 4.871),         // Ra-226
        make_nuclide(86, 222, 3.8235, "d", DecayMode::Alpha, 5.590),       // Rn-222 (Radon)
        make_nuclide(84, 218, 3.10, "m", DecayMode::Alpha, 6.115),         // Po-218
        make_nuclide(82, 214, 26.8, "m", DecayMode::BetaMinus, 1.024),     // Pb-214
        make_nuclide(83, 214, 19.9, "m", DecayMode::BetaMinus, 3.272),     // Bi-214
        make_nuclide(84, 214, 164.3, "μs", DecayMode::Alpha, 7.834),       // Po-214
        make_nuclide(82, 210, 22.20, "y", DecayMode::BetaMinus, 0.064),    // Pb-210
        make_nuclide(83, 210, 5.012, "d", DecayMode::BetaMinus, 1.426),    // Bi-210
        make_nuclide(84, 210, 138.376, "d", DecayMode::Alpha, 5.407),      // Po-210
        make_nuclide(82, 206, 0.0, "y", DecayMode::Stable, 0.0)            // Pb-206 (stable)
    };
    
    uranium_.total_decays = 14;
    uranium_.alpha_decays = 8;
    uranium_.beta_decays = 6;
    uranium_.total_energy_MeV = 51.70;
    uranium_.longest_half_life_years = 4.468e9;
}

// ============================================================================
// 4. ACTINIUM SERIES (4n+3): U-235 → Pb-207
// ============================================================================

void NaturalDecaySeries::initialize_actinium_series() {
    actinium_.name = "Actinium Series (Uranium-Actinium Series)";
    actinium_.series_type = "4n+3";
    actinium_.parent_Z = 92;
    actinium_.parent_A = 235;
    actinium_.stable_Z = 82;
    actinium_.stable_A = 207;
    
    // Complete decay chain
    actinium_.chain = {
        make_nuclide(92, 235, 7.04e8, "y", DecayMode::Alpha, 4.679),       // U-235
        make_nuclide(90, 231, 25.52, "h", DecayMode::BetaMinus, 0.391),    // Th-231
        make_nuclide(91, 231, 32760, "y", DecayMode::Alpha, 5.150),        // Pa-231
        make_nuclide(89, 227, 21.772, "y", DecayMode::BetaMinus, 0.045),   // Ac-227 (98.6% β⁻)
        make_nuclide(90, 227, 18.68, "d", DecayMode::Alpha, 6.147),        // Th-227 (from Ac-227 α branch 1.4%)
        make_nuclide(88, 223, 11.43, "d", DecayMode::Alpha, 5.979),        // Ra-223
        make_nuclide(86, 219, 3.96, "s", DecayMode::Alpha, 6.946),         // Rn-219 (Actinon)
        make_nuclide(84, 215, 1.781, "ms", DecayMode::Alpha, 7.526),       // Po-215
        make_nuclide(82, 211, 36.1, "m", DecayMode::BetaMinus, 1.367),     // Pb-211
        make_nuclide(83, 211, 2.14, "m", DecayMode::Alpha, 6.751),         // Bi-211
        make_nuclide(81, 207, 4.77, "m", DecayMode::BetaMinus, 1.418),     // Tl-207
        make_nuclide(82, 207, 0.0, "y", DecayMode::Stable, 0.0)            // Pb-207 (stable)
    };
    
    actinium_.total_decays = 11;
    actinium_.alpha_decays = 7;
    actinium_.beta_decays = 4;
    actinium_.total_energy_MeV = 46.40;
    actinium_.longest_half_life_years = 7.04e8;
}

// ============================================================================
// Query Methods
// ============================================================================

const DecayChain* NaturalDecaySeries::get_series(const std::string& series_type) const {
    if (series_type == "4n" || series_type == "Thorium") return &thorium_;
    if (series_type == "4n+1" || series_type == "Neptunium") return &neptunium_;
    if (series_type == "4n+2" || series_type == "Uranium") return &uranium_;
    if (series_type == "4n+3" || series_type == "Actinium") return &actinium_;
    return nullptr;
}

const DecayChain* NaturalDecaySeries::get_series_for_isotope(uint8_t /*Z*/, uint16_t A) const {
    // Note: Z unused - series determined solely by mass number A (4n, 4n+1, 4n+2, 4n+3)
    int n = series_type_4n(A);
    if (n == 0) return &thorium_;
    if (n == 1) return &neptunium_;
    if (n == 2) return &uranium_;
    return &actinium_;
}

std::optional<DecayNuclide> NaturalDecaySeries::find_nuclide(uint8_t Z, uint16_t A) const {
    // Search all chains
    const DecayChain* chains[] = {&thorium_, &neptunium_, &uranium_, &actinium_};
    
    for (const auto* chain : chains) {
        for (const auto& nuclide : chain->chain) {
            if (nuclide.Z == Z && nuclide.A == A) {
                return nuclide;
            }
        }
    }
    
    return std::nullopt;
}

std::vector<DecayNuclide> NaturalDecaySeries::trace_decay_path(uint8_t Z_start, uint16_t A_start) const {
    std::vector<DecayNuclide> path;
    
    const DecayChain* chain = get_series_for_isotope(Z_start, A_start);
    if (!chain) return path;
    
    bool found_start = false;
    for (const auto& nuclide : chain->chain) {
        if (nuclide.Z == Z_start && nuclide.A == A_start) {
            found_start = true;
        }
        if (found_start) {
            path.push_back(nuclide);
            if (nuclide.is_stable()) break;
        }
    }
    
    return path;
}

// ============================================================================
// Singleton
// ============================================================================

static std::unique_ptr<NaturalDecaySeries> g_decay_series;

const NaturalDecaySeries& get_decay_series() {
    if (!g_decay_series) {
        g_decay_series = std::make_unique<NaturalDecaySeries>();
    }
    return *g_decay_series;
}

void init_decay_series() {
    g_decay_series = std::make_unique<NaturalDecaySeries>();
}

// ============================================================================
// Decay Calculator
// ============================================================================

double DecayCalculator::atoms_remaining(double N0, double lambda, double t) {
    return N0 * std::exp(-lambda * t);
}

double DecayCalculator::activity_at_time(double A0, double lambda, double t) {
    return A0 * std::exp(-lambda * t);
}

double DecayCalculator::secular_equilibrium_ratio(
    double lambda_parent,
    double lambda_daughter
) {
    // When parent >> daughter half-life, N_daughter / N_parent = λ_parent / λ_daughter
    return lambda_parent / lambda_daughter;
}

} // namespace nuclear
} // namespace vsepr
