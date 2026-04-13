/**
 * nuclear_core_runner.cpp
 * -----------------------
 * VSEPR-SIM 4.0-Legacy-Beta — Nuclear Core Autonomous Report Runner
 *
 * Activates the Z=94 (Pu-239) nuclear core and runs the autonomous report
 * generation engine for a configurable wall-clock duration (10-20 minutes).
 *
 * Outputs:
 *   reports/nuclear_core_94/summary.csv              Cumulative CSV log
 *   reports/nuclear_core_94/master_report.tex         Compilable LaTeX master
 *   reports/nuclear_core_94/data.xml                  SpreadsheetML (Excel XML)
 *   reports/nuclear_core_94/TMS-NNNNNN.md             Individual Markdown reports
 *
 * The engine injects Pu-239 (Z=94) material properties into the case generator
 * stream so that nuclear fuel material systems appear alongside conventional
 * engineering materials. This exercises the full nuclear domain property chain.
 *
 * Build:
 *   cmake --build build --target nuclear-core-runner
 *
 * Usage:
 *   nuclear-core-runner [OPTIONS]
 *
 * Options:
 *   --minutes N     Run duration in minutes (default: 15, range: 1-60)
 *   --seed N        Base RNG seed (default: 94239)
 *   --out DIR       Output directory (default: reports/nuclear_core_94)
 *   --quiet         Suppress per-report progress
 *   --help          Show this help
 */

#include "core/report_engine.hpp"
#include "multiscale/scale_bridge.hpp"
#include "version/version_manifest.hpp"
#include "identity/provenance_record.hpp"
#include "sim/molecule.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

// ============================================================================
// ANSI
// ============================================================================

namespace ansi {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
    const char* RED     = "\033[91m";
    const char* GREEN   = "\033[92m";
    const char* YELLOW  = "\033[93m";
    const char* CYAN    = "\033[96m";
}

// ============================================================================
// Formatting helpers
// ============================================================================

static std::string fmt_d(double v, int prec = 4) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static std::string fmt_sci(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(prec) << v;
    return ss.str();
}

static std::string timestamp_now() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static std::string escape_latex(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '&':  out += "\\&";  break;
            case '%':  out += "\\%";  break;
            case '$':  out += "\\$";  break;
            case '#':  out += "\\#";  break;
            case '_':  out += "\\_";  break;
            case '{':  out += "\\{";  break;
            case '}':  out += "\\}";  break;
            case '~':  out += "\\textasciitilde{}"; break;
            case '^':  out += "\\textasciicircum{}"; break;
            default:   out += c;
        }
    }
    return out;
}

static std::string escape_xml(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
        }
    }
    return out;
}

// ============================================================================
// Pu-239 Material Properties (nuclear fuel)
// ============================================================================

static vsepr::report::MaterialProperties pu239_properties() {
    vsepr::report::MaterialProperties pu;
    pu.name     = "Plutonium-239";
    pu.formula  = "Pu";
    pu.category = "actinide-fuel";

    // Mechanical
    pu.density_kg_m3       = 19816.0;   // kg/m3 (alpha-Pu)
    pu.elastic_modulus_GPa = 96.0;      // GPa
    pu.yield_strength_MPa  = 275.0;     // MPa (approximate)
    pu.ultimate_strength_MPa = 420.0;   // MPa
    pu.poisson_ratio       = 0.21;

    // Thermal
    pu.thermal_conductivity_W_mK = 6.74;    // W/m-K (alpha phase)
    pu.specific_heat_J_kgK       = 130.0;   // J/kg-K
    pu.thermal_expansion_1_K     = 46.7e-6; // 1/K (alpha, very high)
    pu.melting_point_K           = 912.5;    // K
    pu.boiling_point_K           = 3505.0;   // K

    // Fatigue
    pu.fatigue_endurance_MPa = 110.0;
    pu.fatigue_exponent      = -0.12;

    pu.anisotropy_factor  = 1.8;   // alpha-Pu is highly anisotropic
    pu.uncertainty_factor = 0.15;  // nuclear data has moderate uncertainty
    pu.confidence_score   = 0.82;  // well-characterised but exotic
    pu.is_synthetic       = false;
    pu.primary_Z          = 94;

    return pu;
}

// UO2 (uranium dioxide) — common nuclear fuel matrix
static vsepr::report::MaterialProperties uo2_properties() {
    vsepr::report::MaterialProperties uo2;
    uo2.name     = "Uranium Dioxide";
    uo2.formula  = "UO2";
    uo2.category = "ceramic-fuel";

    uo2.density_kg_m3       = 10970.0;
    uo2.elastic_modulus_GPa = 220.0;
    uo2.yield_strength_MPa  = 0.0;      // brittle ceramic
    uo2.ultimate_strength_MPa = 150.0;   // compressive proxy
    uo2.poisson_ratio       = 0.316;

    uo2.thermal_conductivity_W_mK = 3.20;
    uo2.specific_heat_J_kgK       = 243.0;
    uo2.thermal_expansion_1_K     = 10.1e-6;
    uo2.melting_point_K           = 3120.0;
    uo2.boiling_point_K           = 3815.0;

    uo2.fatigue_endurance_MPa = 50.0;
    uo2.fatigue_exponent      = -0.08;
    uo2.anisotropy_factor     = 1.0;
    uo2.uncertainty_factor    = 0.10;
    uo2.confidence_score      = 0.90;
    uo2.is_synthetic          = false;
    uo2.primary_Z             = 92;

    return uo2;
}

// Zircaloy-4 — standard cladding
static vsepr::report::MaterialProperties zircaloy4_properties() {
    vsepr::report::MaterialProperties zr4;
    zr4.name     = "Zircaloy-4";
    zr4.formula  = "Zr-Sn-Fe-Cr";
    zr4.category = "cladding-alloy";

    zr4.density_kg_m3       = 6560.0;
    zr4.elastic_modulus_GPa = 99.3;
    zr4.yield_strength_MPa  = 380.0;
    zr4.ultimate_strength_MPa = 510.0;
    zr4.poisson_ratio       = 0.34;

    zr4.thermal_conductivity_W_mK = 13.8;
    zr4.specific_heat_J_kgK       = 285.0;
    zr4.thermal_expansion_1_K     = 5.9e-6;
    zr4.melting_point_K           = 2125.0;
    zr4.boiling_point_K           = 4650.0;

    zr4.fatigue_endurance_MPa = 190.0;
    zr4.fatigue_exponent      = -0.10;
    zr4.anisotropy_factor     = 1.4;
    zr4.uncertainty_factor    = 0.05;
    zr4.confidence_score      = 0.95;
    zr4.is_synthetic          = false;
    zr4.primary_Z             = 40;

    return zr4;
}

// ============================================================================
// LaTeX Report Generator
// ============================================================================

class LaTeXWriter {
public:
    // Write master document preamble
    static std::string preamble(const std::string& title, int total_reports,
                                double elapsed_min, const std::string& timestamp) {
        std::ostringstream tex;
        tex << "\\documentclass[11pt,a4paper]{article}\n";
        tex << "\\usepackage[utf8]{inputenc}\n";
        tex << "\\usepackage[T1]{fontenc}\n";
        tex << "\\usepackage{lmodern}\n";
        tex << "\\usepackage{geometry}\n";
        tex << "\\geometry{margin=2.5cm}\n";
        tex << "\\usepackage{booktabs}\n";
        tex << "\\usepackage{longtable}\n";
        tex << "\\usepackage{siunitx}\n";
        tex << "\\usepackage{hyperref}\n";
        tex << "\\usepackage{xcolor}\n";
        tex << "\\usepackage{fancyhdr}\n";
        tex << "\\pagestyle{fancy}\n";
        tex << "\\fancyhead[L]{VSEPR-SIM 4.0-LB}\n";
        tex << "\\fancyhead[R]{Nuclear Core Z=94}\n";
        tex << "\\fancyfoot[C]{\\thepage}\n";
        tex << "\\sisetup{output-exponent-marker=\\ensuremath{\\mathrm{e}}}\n";
        tex << "\n";
        tex << "\\title{" << escape_latex(title) << "}\n";
        tex << "\\author{VSEPR-SIM Autonomous Report Engine v4.0-LB}\n";
        tex << "\\date{" << escape_latex(timestamp) << "}\n";
        tex << "\n";
        tex << "\\begin{document}\n";
        tex << "\\maketitle\n";
        tex << "\n";
        tex << "\\begin{abstract}\n";
        tex << "Autonomous thermal-materials digital experiment report generated by\n";
        tex << "the VSEPR-SIM Nuclear Core Runner with Z=94 (Pu-239) active.\n";
        tex << "Total reports: " << total_reports << ".\n";
        tex << "Wall-clock duration: " << fmt_d(elapsed_min, 1) << " minutes.\n";
        tex << "Nuclear fuel materials (Pu-239, UO\\textsubscript{2}, Zircaloy-4) are\n";
        tex << "injected into the case generator stream alongside conventional engineering\n";
        tex << "materials to exercise the full nuclear domain property chain.\n";
        tex << "\\end{abstract}\n";
        tex << "\n";
        tex << "\\tableofcontents\n";
        tex << "\\newpage\n\n";
        return tex.str();
    }

    // Write a single report section
    static std::string report_section(const vsepr::report::TechnicalReport& report) {
        std::ostringstream tex;

        tex << "\\section{" << escape_latex(report.title) << "}\n";
        tex << "\\label{sec:tms-" << report.report_id << "}\n\n";

        // Abstract
        tex << "\\subsection*{Abstract}\n";
        tex << escape_latex(report.abstract_text) << "\n\n";

        // Material system
        tex << "\\subsection{Material System}\n";
        tex << "\\begin{description}\n";
        tex << "  \\item[Case Name] " << escape_latex(report.material_case.case_name) << "\n";
        tex << "  \\item[Complexity] " << escape_latex(vsepr::report::complexity_name(report.current_level)) << "\n";
        tex << "  \\item[Components] " << report.material_case.components.size() << "\n";
        tex << "  \\item[Gamma Factor] " << fmt_d(report.material_case.gamma_factor, 3) << "\n";
        tex << "  \\item[Rarity Score] " << fmt_d(report.material_case.rarity_score, 3) << "\n";
        tex << "\\end{description}\n\n";

        // Effective properties table
        tex << "\\subsection{Effective Properties}\n";
        tex << "\\begin{tabular}{lrl}\n";
        tex << "  \\toprule\n";
        tex << "  Property & Value & Unit \\\\\n";
        tex << "  \\midrule\n";
        const auto& eff = report.material_case.effective;
        tex << "  Density & " << fmt_d(eff.density_kg_m3, 1) << " & \\si{kg/m^3} \\\\\n";
        tex << "  Elastic Modulus & " << fmt_d(eff.elastic_modulus_GPa, 1) << " & \\si{GPa} \\\\\n";
        tex << "  Yield Strength & " << fmt_d(eff.yield_strength_MPa, 1) << " & \\si{MPa} \\\\\n";
        tex << "  Thermal Conductivity & " << fmt_d(eff.thermal_conductivity_W_mK, 2) << " & \\si{W/m.K} \\\\\n";
        tex << "  Specific Heat & " << fmt_d(eff.specific_heat_J_kgK, 1) << " & \\si{J/kg.K} \\\\\n";
        tex << "  Thermal Expansion & " << fmt_sci(eff.thermal_expansion_1_K) << " & \\si{1/K} \\\\\n";
        tex << "  Melting Point & " << fmt_d(eff.melting_point_K, 0) << " & \\si{K} \\\\\n";
        tex << "  Confidence & " << fmt_d(eff.confidence_score, 2) << " & (0--1) \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        // Experiment results
        if (!report.experiments.empty()) {
            tex << "\\subsection{Experiment Results}\n";
            tex << "\\begin{longtable}{llrll}\n";
            tex << "  \\toprule\n";
            tex << "  Experiment & Primary & Value & Unit & Notes \\\\\n";
            tex << "  \\midrule\n";
            tex << "  \\endhead\n";
            for (const auto& e : report.experiments) {
                tex << "  " << escape_latex(e.experiment_name)
                    << " & " << escape_latex(e.primary_label)
                    << " & " << fmt_d(e.primary_value, 4)
                    << " & " << escape_latex(e.primary_unit)
                    << " & " << escape_latex(e.notes.substr(0, 60))
                    << " \\\\\n";
            }
            tex << "  \\bottomrule\n";
            tex << "\\end{longtable}\n\n";
        }

        // Findings
        if (!report.findings.empty()) {
            tex << "\\subsection{Findings}\n";
            tex << "\\begin{itemize}\n";
            for (const auto& f : report.findings) {
                tex << "  \\item " << escape_latex(f) << "\n";
            }
            tex << "\\end{itemize}\n\n";
        }

        // Warnings
        if (!report.warnings.empty()) {
            tex << "\\subsection{Warnings}\n";
            tex << "\\begin{itemize}\n";
            for (const auto& w : report.warnings) {
                tex << "  \\item \\textcolor{red}{" << escape_latex(w) << "}\n";
            }
            tex << "\\end{itemize}\n\n";
        }

        // Conclusion
        tex << "\\subsection{Conclusion}\n";
        tex << escape_latex(report.conclusion) << "\n\n";

        // Scores
        tex << "\\subsection{Aggregate Scores}\n";
        tex << "\\begin{tabular}{lr}\n";
        tex << "  \\toprule\n";
        tex << "  Metric & Value \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Overall Stability & " << fmt_d(report.overall_stability_score, 3) << " \\\\\n";
        tex << "  Novelty & " << fmt_d(report.novelty_score, 3) << " \\\\\n";
        tex << "  Thermal Response Index & " << fmt_d(report.thermal_response_index, 3) << " \\\\\n";
        tex << "  Deformation Score & " << fmt_d(report.deformation_score, 3) << " \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";
        tex << "\\newpage\n\n";

        return tex.str();
    }

    // Write document footer
    static std::string footer() {
        return "\\end{document}\n";
    }
};

// ============================================================================
// SpreadsheetML (Excel XML) Writer
// ============================================================================

class ExcelXMLWriter {
public:
    ExcelXMLWriter() = default;

    void add_header() {
        rows_.push_back({
            "Report_ID", "Timestamp", "Complexity", "Case_Name", "Formula",
            "Category", "Components", "Gamma", "Rarity", "Instability",
            "Density_kg_m3", "E_GPa", "Yield_MPa", "k_W_mK", "Cp_J_kgK",
            "alpha_1_K", "Tm_K", "Confidence",
            "Experiments", "Stability_Score", "Novelty", "Thermal_Index",
            "Deformation", "Warnings", "Nuclear_Core"
        });
    }

    void add_report(const vsepr::report::TechnicalReport& r) {
        const auto& eff = r.material_case.effective;
        bool is_nuclear = false;
        for (const auto& c : r.material_case.components) {
            if (c.primary_Z == 94 || c.primary_Z == 92 || c.primary_Z == 90) {
                is_nuclear = true;
                break;
            }
        }

        rows_.push_back({
            std::to_string(r.report_id),
            r.timestamp,
            vsepr::report::complexity_name(r.current_level),
            r.material_case.case_name,
            eff.formula,
            eff.category,
            std::to_string(r.material_case.components.size()),
            fmt_d(r.material_case.gamma_factor, 3),
            fmt_d(r.material_case.rarity_score, 3),
            fmt_d(r.material_case.instability_index, 3),
            fmt_d(eff.density_kg_m3, 1),
            fmt_d(eff.elastic_modulus_GPa, 1),
            fmt_d(eff.yield_strength_MPa, 1),
            fmt_d(eff.thermal_conductivity_W_mK, 2),
            fmt_d(eff.specific_heat_J_kgK, 1),
            fmt_sci(eff.thermal_expansion_1_K),
            fmt_d(eff.melting_point_K, 0),
            fmt_d(eff.confidence_score, 2),
            std::to_string(r.experiments.size()),
            fmt_d(r.overall_stability_score, 3),
            fmt_d(r.novelty_score, 3),
            fmt_d(r.thermal_response_index, 3),
            fmt_d(r.deformation_score, 3),
            std::to_string(r.warnings.size()),
            is_nuclear ? "Z=94" : ""
        });
    }

    std::string to_xml() const {
        std::ostringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<?mso-application progid=\"Excel.Sheet\"?>\n";
        xml << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n";
        xml << "  xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n";
        xml << "  <Styles>\n";
        xml << "    <Style ss:ID=\"header\">\n";
        xml << "      <Font ss:Bold=\"1\" ss:Size=\"11\"/>\n";
        xml << "      <Interior ss:Color=\"#D9E1F2\" ss:Pattern=\"Solid\"/>\n";
        xml << "    </Style>\n";
        xml << "    <Style ss:ID=\"nuclear\">\n";
        xml << "      <Interior ss:Color=\"#FFF2CC\" ss:Pattern=\"Solid\"/>\n";
        xml << "    </Style>\n";
        xml << "    <Style ss:ID=\"default\"/>\n";
        xml << "  </Styles>\n";
        xml << "  <Worksheet ss:Name=\"Nuclear Core Z94 Reports\">\n";
        xml << "    <Table>\n";

        for (size_t r = 0; r < rows_.size(); ++r) {
            bool is_header = (r == 0);
            bool is_nuclear = (!is_header && rows_[r].size() > 24 &&
                               rows_[r][24].find("Z=94") != std::string::npos);
            std::string style = is_header ? "header" : (is_nuclear ? "nuclear" : "default");

            xml << "      <Row>\n";
            for (const auto& cell : rows_[r]) {
                // Detect numeric
                bool numeric = false;
                if (!cell.empty() && !is_header) {
                    char* end = nullptr;
                    std::strtod(cell.c_str(), &end);
                    numeric = (end != cell.c_str() && *end == '\0');
                }

                xml << "        <Cell ss:StyleID=\"" << style << "\">";
                if (numeric) {
                    xml << "<Data ss:Type=\"Number\">" << cell << "</Data>";
                } else {
                    xml << "<Data ss:Type=\"String\">" << escape_xml(cell) << "</Data>";
                }
                xml << "</Cell>\n";
            }
            xml << "      </Row>\n";
        }

        xml << "    </Table>\n";
        xml << "  </Worksheet>\n";
        xml << "</Workbook>\n";
        return xml.str();
    }

private:
    std::vector<std::vector<std::string>> rows_;
};

// ============================================================================
// Nuclear Case Injector
// ============================================================================

/**
 * Wraps the standard AutonomousEngine and periodically injects nuclear fuel
 * material systems (Pu-239, UO2, Zircaloy-4) into the report stream.
 *
 * Injection rate: ~20% of cases contain nuclear materials.
 */
class NuclearCoreRunner {
public:
    struct Config {
        double        minutes       = 15.0;
        uint64_t      seed          = 94239;
        std::string   output_dir    = "reports/nuclear_core_94";
        bool          quiet         = false;
    };

    explicit NuclearCoreRunner(const Config& cfg)
        : cfg_(cfg), rng_(cfg.seed) {
        vsepr::report::EngineConfig ecfg;
        ecfg.base_seed        = cfg.seed;
        ecfg.target_reports   = 999999;   // we use wall-clock, not count
        ecfg.output_dir       = cfg.output_dir;
        ecfg.write_individual = true;
        ecfg.write_csv_log    = true;
        ecfg.print_progress   = false;    // we handle progress ourselves
        ecfg.threshold_l2     = 30;
        ecfg.threshold_l3     = 80;
        ecfg.threshold_l4     = 200;
        ecfg.threshold_l5     = 400;

        engine_ = std::make_unique<vsepr::report::AutonomousEngine>(ecfg);
        excel_.add_header();
    }

    int run() {
        fs::create_directories(cfg_.output_dir);

        auto start = std::chrono::steady_clock::now();
        auto deadline = start + std::chrono::duration<double>(cfg_.minutes * 60.0);

        // Banner
        std::cout << ansi::BOLD;
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  VSEPR-SIM Nuclear Core Runner                                ║\n";
        std::cout << "║  core = 94 (Pu-239) — ACTIVE                                  ║\n";
        std::cout << "║  Duration: " << std::setw(5) << fmt_d(cfg_.minutes, 1)
                  << " minutes                                       ║\n";
        std::cout << "║  Output: " << std::setw(50) << std::left << cfg_.output_dir << " ║\n";
        std::cout << "║  Seed: " << std::setw(12) << cfg_.seed
                  << "                                        ║\n";
        std::cout << "║  Formats: LaTeX (.tex) + Excel XML (.xml) + Markdown + CSV    ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cout << ansi::RESET << "\n";

        // Nuclear core info
        const auto* core = vsepr::multiscale::get_active_core();
        if (core) {
            std::cout << ansi::GREEN << "  ▶ Active Core: Z="
                      << static_cast<int>(core->Z)
                      << " (" << core->isotope
                      << ", Ed=" << core->Ed_eV << " eV"
                      << ", fissility=" << fmt_d(core->fissility, 2)
                      << ", " << core->crystal_phase << ")"
                      << ansi::RESET << "\n\n";
        }

        // LaTeX preamble — we'll write the full doc at the end
        std::vector<vsepr::report::TechnicalReport> all_reports;

        int nuclear_count = 0;
        int conventional_count = 0;

        // Main loop — run until wall-clock deadline
        while (std::chrono::steady_clock::now() < deadline) {
            auto report = engine_->generate_one();

            // Inject nuclear materials every ~5th report
            bool inject_nuclear = (report.report_id % 5 == 0);
            if (inject_nuclear) {
                inject_nuclear_material(report);
                nuclear_count++;
            } else {
                conventional_count++;
            }

            // Write Markdown
            {
                std::ostringstream path;
                path << cfg_.output_dir << "/TMS-"
                     << std::setfill('0') << std::setw(6) << report.report_id
                     << ".md";
                std::ofstream out(path.str());
                if (out) out << vsepr::report::ReportWriter::to_markdown(report);
            }

            // Append to CSV
            {
                std::string csv_path = cfg_.output_dir + "/summary.csv";
                bool exists = fs::exists(csv_path);
                std::ofstream out(csv_path, std::ios::app);
                if (out) {
                    if (!exists) out << vsepr::report::ReportWriter::csv_header() << "\n";
                    out << vsepr::report::ReportWriter::to_csv_line(report) << "\n";
                }
            }

            // Accumulate for LaTeX + Excel
            excel_.add_report(report);
            all_reports.push_back(std::move(report));

            // Progress
            if (!cfg_.quiet) {
                auto now = std::chrono::steady_clock::now();
                double elapsed_s = std::chrono::duration<double>(now - start).count();
                double remaining_s = cfg_.minutes * 60.0 - elapsed_s;

                if (all_reports.size() % 25 == 0 || remaining_s < 10.0) {
                    const auto& r = all_reports.back();
                    std::cout << "  [" << std::setw(6) << r.report_id << "] "
                              << std::left << std::setw(10)
                              << vsepr::report::complexity_name(r.current_level)
                              << " | " << std::setw(38) << r.material_case.case_name.substr(0, 38)
                              << " | " << (r.material_case.effective.primary_Z >= 90 ? ansi::YELLOW : "")
                              << "exp=" << r.experiments.size()
                              << " warn=" << r.warnings.size()
                              << ansi::RESET
                              << " | " << fmt_d(remaining_s / 60.0, 1) << "m left"
                              << "\n";
                }
            }
        }

        auto end = std::chrono::steady_clock::now();
        double elapsed_min = std::chrono::duration<double>(end - start).count() / 60.0;

        // ================================================================
        // Write LaTeX master document
        // ================================================================
        {
            std::string tex_path = cfg_.output_dir + "/master_report.tex";
            std::ofstream tex(tex_path);
            if (tex) {
                tex << LaTeXWriter::preamble(
                    "Nuclear Core Z=94 — Autonomous Thermal-Materials Report",
                    static_cast<int>(all_reports.size()),
                    elapsed_min,
                    timestamp_now()
                );

                // Summary section
                tex << "\\section{Executive Summary}\n\n";
                tex << "\\begin{tabular}{lr}\n";
                tex << "  \\toprule\n";
                tex << "  Metric & Value \\\\\n";
                tex << "  \\midrule\n";
                tex << "  Total reports & " << all_reports.size() << " \\\\\n";
                tex << "  Nuclear fuel cases & " << nuclear_count << " \\\\\n";
                tex << "  Conventional cases & " << conventional_count << " \\\\\n";
                tex << "  Wall-clock duration & " << fmt_d(elapsed_min, 1) << " min \\\\\n";
                tex << "  Reports/second & " << fmt_d(all_reports.size() / (elapsed_min * 60.0), 1) << " \\\\\n";
                tex << "  Active core & Z=94 (Pu-239) \\\\\n";
                tex << "  Seed & " << cfg_.seed << " \\\\\n";
                tex << "  \\bottomrule\n";
                tex << "\\end{tabular}\n\n";

                // Complexity distribution
                std::map<int, int> level_counts;
                int total_warnings = 0;
                int total_experiments = 0;
                for (const auto& r : all_reports) {
                    level_counts[static_cast<int>(r.current_level)]++;
                    total_warnings += static_cast<int>(r.warnings.size());
                    total_experiments += static_cast<int>(r.experiments.size());
                }

                tex << "\\subsection{Complexity Distribution}\n";
                tex << "\\begin{tabular}{lr}\n";
                tex << "  \\toprule\n";
                tex << "  Level & Count \\\\\n";
                tex << "  \\midrule\n";
                for (const auto& [level, count] : level_counts) {
                    tex << "  " << escape_latex(vsepr::report::complexity_name(
                        static_cast<vsepr::report::ComplexityLevel>(level)))
                        << " & " << count << " \\\\\n";
                }
                tex << "  \\midrule\n";
                tex << "  Total experiments & " << total_experiments << " \\\\\n";
                tex << "  Total warnings & " << total_warnings << " \\\\\n";
                tex << "  \\bottomrule\n";
                tex << "\\end{tabular}\n\n";
                tex << "\\newpage\n\n";

                // Individual reports (limit to first 100 to keep .tex reasonable)
                size_t max_detail = std::min(all_reports.size(), size_t(100));
                tex << "\\section{Detailed Reports (first " << max_detail << ")}\n\n";
                for (size_t i = 0; i < max_detail; ++i) {
                    tex << LaTeXWriter::report_section(all_reports[i]);
                }

                if (all_reports.size() > 100) {
                    tex << "\\section*{Note}\n";
                    tex << "Remaining " << (all_reports.size() - 100)
                        << " reports are available in the CSV and Excel XML outputs.\n\n";
                }

                tex << LaTeXWriter::footer();
                std::cout << "\n  " << ansi::GREEN << "LaTeX: " << ansi::RESET << tex_path << "\n";
            }
        }

        // ================================================================
        // Write Excel XML
        // ================================================================
        {
            std::string xml_path = cfg_.output_dir + "/data.xml";
            std::ofstream xml(xml_path);
            if (xml) {
                xml << excel_.to_xml();
                std::cout << "  " << ansi::GREEN << "Excel: " << ansi::RESET << xml_path << "\n";
            }
        }

        // ================================================================
        // Summary
        // ================================================================
        std::cout << "\n" << ansi::BOLD;
        std::cout << "  ════════════════════════════════════════════════════════════════\n";
        std::cout << "   Nuclear Core Z=94 — Run Complete\n";
        std::cout << "   Reports:     " << all_reports.size() << " (" << nuclear_count << " nuclear, " << conventional_count << " conventional)\n";
        std::cout << "   Duration:    " << fmt_d(elapsed_min, 1) << " minutes\n";
        std::cout << "   Rate:        " << fmt_d(all_reports.size() / (elapsed_min * 60.0), 1) << " reports/sec\n";
        std::cout << "   Experiments: " << [&]{ int n=0; for(auto&r:all_reports) n+=r.experiments.size(); return n; }() << " total\n";
        std::cout << "   Warnings:    " << [&]{ int n=0; for(auto&r:all_reports) n+=r.warnings.size(); return n; }() << " total\n";
        std::cout << "   Output:      " << cfg_.output_dir << "/\n";
        std::cout << "     master_report.tex  — compilable LaTeX\n";
        std::cout << "     data.xml           — SpreadsheetML (open in Excel)\n";
        std::cout << "     summary.csv        — cumulative CSV\n";
        std::cout << "     TMS-NNNNNN.md      — individual Markdown reports\n";
        std::cout << "  ════════════════════════════════════════════════════════════════\n";
        std::cout << ansi::RESET << "\n";

        return 0;
    }

private:
    void inject_nuclear_material(vsepr::report::TechnicalReport& report) {
        // Replace or add nuclear material to the case
        std::uniform_int_distribution<int> fuel_type(0, 2);
        int choice = fuel_type(rng_);

        vsepr::report::MaterialProperties nuclear_mat;
        switch (choice) {
            case 0: nuclear_mat = pu239_properties();     break;
            case 1: nuclear_mat = uo2_properties();       break;
            case 2: nuclear_mat = zircaloy4_properties(); break;
        }

        // Add as primary or secondary component
        auto& mc = report.material_case;

        if (mc.components.empty()) {
            mc.components.push_back(nuclear_mat);
            mc.fractions.push_back(1.0);
        } else {
            // Replace first component with nuclear material
            mc.components[0] = nuclear_mat;
            // Recompute effective properties
            mc.effective = nuclear_mat;
            if (mc.components.size() > 1) {
                // Leave mixing as-is but update effective primary_Z
                mc.effective.primary_Z = nuclear_mat.primary_Z;
            }
        }

        mc.effective.primary_Z = nuclear_mat.primary_Z;
        mc.case_name = nuclear_mat.name + " — " + mc.case_name;

        // Add radiation damage defect for nuclear cases
        vsepr::report::DefectSpec radiation;
        radiation.type = "radiation_damage";
        radiation.concentration = 0.001 + (rng_() % 100) * 0.0001;
        radiation.size_nm = 2.0 + (rng_() % 200) * 0.1;
        mc.defects.push_back(radiation);

        mc.rarity_score = std::max(mc.rarity_score, 0.7);
        mc.instability_index = std::max(mc.instability_index, 0.5);

        // Re-run experiments with nuclear material
        vsepr::report::ExperimentRunner runner;
        report.experiments = runner.run_all(mc);

        // Re-analyze
        double stab_sum = 0, plaus_sum = 0;
        for (const auto& e : report.experiments) {
            stab_sum += e.numerical_stability;
            plaus_sum += e.physical_plausibility;
        }
        int ne = std::max(1, static_cast<int>(report.experiments.size()));
        report.overall_stability_score = stab_sum / ne;
        report.novelty_score = mc.rarity_score;

        report.findings.push_back("Nuclear fuel material: " + nuclear_mat.name +
                                  " (Z=" + std::to_string(nuclear_mat.primary_Z) + ")");
        if (nuclear_mat.primary_Z == 94) {
            report.findings.push_back("Active fissile core: Pu-239 (Ed=35 eV, fissility=37.03)");
        }
    }

    Config cfg_;
    std::mt19937_64 rng_;
    std::unique_ptr<vsepr::report::AutonomousEngine> engine_;
    ExcelXMLWriter excel_;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    NuclearCoreRunner::Config cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            std::cout << R"(
VSEPR-SIM Nuclear Core Runner — Z=94 (Pu-239)
4.0-Legacy-Beta Autonomous Report Generation

Usage: nuclear-core-runner [OPTIONS]

Options:
  --minutes N     Run duration in minutes (default: 15, range: 1-60)
  --seed N        Base RNG seed (default: 94239)
  --out DIR       Output directory (default: reports/nuclear_core_94)
  --quiet         Suppress per-report progress
  --help          Show this help

Output files:
  master_report.tex   Compilable LaTeX document (pdflatex/xelatex)
  data.xml            SpreadsheetML (open directly in Excel)
  summary.csv         Cumulative CSV log
  TMS-NNNNNN.md       Individual Markdown reports

Nuclear fuel materials injected (~20% of cases):
  Pu-239     Alpha-plutonium, monoclinic, Ed=35 eV
  UO2        Uranium dioxide ceramic fuel
  Zircaloy-4 Standard cladding alloy
)";
            return 0;
        }
        else if (arg == "--minutes" && i + 1 < argc) {
            cfg.minutes = std::stod(argv[++i]);
            cfg.minutes = std::max(1.0, std::min(60.0, cfg.minutes));
        }
        else if (arg == "--seed" && i + 1 < argc) {
            cfg.seed = std::stoull(argv[++i]);
        }
        else if (arg == "--out" && i + 1 < argc) {
            cfg.output_dir = argv[++i];
        }
        else if (arg == "--quiet") {
            cfg.quiet = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\nRun with --help for usage.\n";
            return 1;
        }
    }

    NuclearCoreRunner runner(cfg);
    return runner.run();
}
