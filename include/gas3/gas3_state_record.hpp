/**
 * gas3_state_record.hpp
 * ---------------------
 * GasStateRecord: The Reportable, Sortable, Exportable State Object.
 *
 * Every computed thermodynamic state carries:
 *   - species identity
 *   - model used
 *   - input pair and solved variables
 *   - convergence diagnostics
 *   - physical validity flags
 *   - quality tier (Q0-Q4) and numeric score (0-100)
 *   - full thermodynamic properties
 *   - warning/note annotations
 *   - UTC timestamp
 *
 * This is the single atomic unit of data in the Gas3 pipeline.
 * Every record is self-describing, sortable, and export-ready.
 *
 * Anti-black-box: every field public, every intermediate exposed.
 */

#pragma once

#include "gas2/gas2_engine.hpp"
#include <string>
#include <cmath>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace gas3 {

// ============================================================================
// Quality tiers
// ============================================================================

enum class QualityTier : int {
    Q0 = 0,  // failed / nonphysical / diverged
    Q1 = 1,  // converged weakly, suspicious residual or near-boundary
    Q2 = 2,  // converged and usable, modest residual
    Q3 = 3,  // high confidence production-grade
    Q4 = 4   // reference-grade, cross-validated or near known truth
};

inline const char* tier_name(QualityTier t) {
    switch (t) {
        case QualityTier::Q0: return "Q0";
        case QualityTier::Q1: return "Q1";
        case QualityTier::Q2: return "Q2";
        case QualityTier::Q3: return "Q3";
        case QualityTier::Q4: return "Q4";
    }
    return "Q?";
}

// ============================================================================
// Sample mode (provenance tagging)
// ============================================================================

enum class SampleMode {
    Linear,
    Random_Uniform,
    Random_LogP,
    Boundary_Probe,
    Near_Critical_Probe,
    Adaptive
};

inline const char* sample_mode_name(SampleMode m) {
    switch (m) {
        case SampleMode::Linear:             return "linear";
        case SampleMode::Random_Uniform:     return "random_uniform";
        case SampleMode::Random_LogP:        return "random_logP";
        case SampleMode::Boundary_Probe:     return "boundary_probe";
        case SampleMode::Near_Critical_Probe:return "near_critical_probe";
        case SampleMode::Adaptive:           return "adaptive";
    }
    return "unknown";
}

// ============================================================================
// Species classification
// ============================================================================

enum class SpeciesClass {
    Noble,
    LightDiatomic,
    PolarSmall,
    HeavyPolyatomic,
    Unknown
};

inline const char* species_class_name(SpeciesClass c) {
    switch (c) {
        case SpeciesClass::Noble:           return "noble";
        case SpeciesClass::LightDiatomic:   return "light_diatomic";
        case SpeciesClass::PolarSmall:      return "polar_small";
        case SpeciesClass::HeavyPolyatomic: return "heavy_polyatomic";
        case SpeciesClass::Unknown:         return "unknown";
    }
    return "unknown";
}

inline SpeciesClass classify_species(const std::string& formula) {
    if (formula == "He" || formula == "Ne" || formula == "Ar" ||
        formula == "Kr" || formula == "Xe")
        return SpeciesClass::Noble;
    if (formula == "H2" || formula == "N2" || formula == "O2" || formula == "Cl2")
        return SpeciesClass::LightDiatomic;
    if (formula == "H2O" || formula == "NH3" || formula == "SO2")
        return SpeciesClass::PolarSmall;
    if (formula == "CO2" || formula == "CH4")
        return SpeciesClass::HeavyPolyatomic;
    return SpeciesClass::Unknown;
}

// ============================================================================
// GasStateRecord — the core data object
// ============================================================================

struct GasStateRecord {
    // --- Identity ---
    std::string species;
    std::string model_name;       // ideal, vdW, RK
    std::string input_mode;       // PT, Tv, Pv
    std::string region;           // gas, near-critical, supercritical, questionable

    // --- Primary state variables ---
    double T_K      = NAN;
    double P_Pa     = NAN;
    double V_m3     = NAN;
    double n_mol    = 1.0;
    double rho_kgm3 = NAN;        // mass density

    // --- Compressibility and reduced state ---
    double Z        = NAN;        // PV/(nRT)
    double Tr       = NAN;        // reduced temperature T/Tc
    double Pr       = NAN;        // reduced pressure P/Pc

    // --- Kinetic properties ---
    double v_rms    = NAN;        // m/s
    double v_mean   = NAN;        // m/s
    double v_mp     = NAN;        // m/s
    double mfp_m    = NAN;        // mean free path (m)

    // --- Thermodynamic potentials ---
    double u_Jmol   = NAN;        // internal energy J/mol
    double h_Jmol   = NAN;        // enthalpy J/mol
    double s_JmolK  = NAN;        // entropy J/(mol*K)

    // --- Heat capacity ---
    double Cp_JmolK = NAN;        // isobaric J/(mol*K)
    double Cv_JmolK = NAN;        // isochoric J/(mol*K)
    double gamma    = NAN;        // Cp/Cv

    // --- Derived ---
    double c_sound  = NAN;        // speed of sound (m/s)
    double mu_JT    = NAN;        // Joule-Thomson coefficient (K/Pa)
    double M_gmol   = NAN;        // molar mass (g/mol)

    // --- Fugacity ---
    double phi      = NAN;        // fugacity coefficient
    double f_Pa     = NAN;        // fugacity (Pa)

    // --- Convergence diagnostics ---
    double residual   = NAN;
    int    iterations = 0;
    bool   converged  = false;
    bool   physically_valid = false;

    // --- Quality ---
    QualityTier quality_tier = QualityTier::Q0;
    double quality_score = 0.0;   // 0-100
    std::string warning_flags;

    // --- Provenance ---
    SampleMode sample_mode = SampleMode::Linear;
    std::string timestamp_utc;
    uint64_t    run_id = 0;
    uint64_t    record_index = 0;

    // --- Helpers ---
    double P_atm() const { return P_Pa / gas2::atm_to_Pa; }

    // Machine-readable log line
    std::string to_log_line() const {
        std::ostringstream ss;
        ss << std::fixed;
        ss << "[" << timestamp_utc << "] "
           << "species=" << species
           << " model=" << model_name
           << " T_K=" << std::setprecision(1) << T_K
           << " P_atm=" << std::setprecision(1) << P_atm()
           << " v_rms_mps=" << std::setprecision(0) << v_rms
           << " Z=" << std::setprecision(6) << Z
           << " residual=" << std::scientific << std::setprecision(1) << residual
           << std::fixed
           << " quality=" << tier_name(quality_tier)
           << " converged=" << converged;
        return ss.str();
    }

    // CSV header
    static std::string csv_header() {
        return "species,model,input_mode,region,sample_mode,"
               "T_K,P_Pa,P_atm,V_m3,n_mol,rho_kgm3,"
               "Z,Tr,Pr,"
               "v_rms,v_mean,v_mp,mfp_m,"
               "u_Jmol,h_Jmol,s_JmolK,"
               "Cp_JmolK,Cv_JmolK,gamma,c_sound,mu_JT,"
               "M_gmol,phi,f_Pa,"
               "residual,iterations,converged,physically_valid,"
               "quality_tier,quality_score,warning_flags,"
               "timestamp_utc,run_id,record_index";
    }

    // CSV row
    std::string to_csv_row() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << species << "," << model_name << "," << input_mode << ","
           << region << "," << sample_mode_name(sample_mode) << ",";
        ss << T_K << "," << P_Pa << "," << P_atm() << ","
           << V_m3 << "," << n_mol << "," << rho_kgm3 << ",";
        ss << Z << "," << Tr << "," << Pr << ",";
        ss << v_rms << "," << v_mean << "," << v_mp << "," << mfp_m << ",";
        ss << u_Jmol << "," << h_Jmol << "," << s_JmolK << ",";
        ss << Cp_JmolK << "," << Cv_JmolK << "," << gamma << ","
           << c_sound << "," << mu_JT << ",";
        ss << M_gmol << "," << phi << "," << f_Pa << ",";
        ss << std::scientific << residual << std::fixed << ","
           << iterations << "," << (converged ? 1 : 0) << ","
           << (physically_valid ? 1 : 0) << ",";
        ss << tier_name(quality_tier) << "," << std::setprecision(1) << quality_score << ","
           << "\"" << warning_flags << "\","
           << timestamp_utc << "," << run_id << "," << record_index;
        return ss.str();
    }

    // JSON (compact, one-line)
    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << "{\"species\":\"" << species << "\","
           << "\"model\":\"" << model_name << "\","
           << "\"input_mode\":\"" << input_mode << "\","
           << "\"region\":\"" << region << "\","
           << "\"sample_mode\":\"" << sample_mode_name(sample_mode) << "\","
           << "\"T_K\":" << T_K << ","
           << "\"P_Pa\":" << P_Pa << ","
           << "\"P_atm\":" << P_atm() << ","
           << "\"V_m3\":" << V_m3 << ","
           << "\"rho_kgm3\":" << rho_kgm3 << ","
           << "\"Z\":" << Z << ","
           << "\"Tr\":" << Tr << ","
           << "\"Pr\":" << Pr << ","
           << "\"v_rms\":" << v_rms << ","
           << "\"Cp\":" << Cp_JmolK << ","
           << "\"Cv\":" << Cv_JmolK << ","
           << "\"gamma\":" << gamma << ","
           << "\"c_sound\":" << c_sound << ","
           << "\"h_Jmol\":" << h_Jmol << ","
           << "\"s_JmolK\":" << s_JmolK << ","
           << "\"phi\":" << phi << ","
           << "\"residual\":" << std::scientific << residual << std::fixed << ","
           << "\"iterations\":" << iterations << ","
           << "\"converged\":" << (converged ? "true" : "false") << ","
           << "\"quality_tier\":\"" << tier_name(quality_tier) << "\","
           << "\"quality_score\":" << std::setprecision(1) << quality_score << "}";
        return ss.str();
    }
};

// ============================================================================
// UTC timestamp helper
// ============================================================================

inline std::string utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm buf{};
#ifdef _WIN32
    gmtime_s(&buf, &t);
#else
    gmtime_r(&t, &buf);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &buf);
    return std::string(ts);
}

} // namespace gas3
} // namespace vsepr
