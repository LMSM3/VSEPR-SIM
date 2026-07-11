/**
 * gas3_engine.cpp
 * ---------------
 * Gas3 Module Implementation — CLI dispatch, pipeline execution.
 */

#include "gas3/gas3_engine.hpp"
#include "gas2/gas2_tui.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace vsepr {
namespace gas3 {

// ============================================================================
// Output directory creation
// ============================================================================

static std::string create_run_dir(const std::string& base_dir,
                                   const std::string& run_name) {
    std::string name = run_name;
    if (name.empty()) {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        struct tm buf{};
#ifdef _WIN32
        localtime_s(&buf, &t);
#else
        localtime_r(&t, &buf);
#endif
        char ts[64];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d_%H%M%S", &buf);
        name = std::string(ts);
    }

    std::string run_dir = base_dir + "/" + name;
    fs::create_directories(run_dir + "/csv");
    fs::create_directories(run_dir + "/html");
    fs::create_directories(run_dir + "/fits");
    return run_dir;
}

// ============================================================================
// Print stats summary to stdout
// ============================================================================

static void print_stats(const std::string& label, const SweepStats& s) {
    std::cout << std::fixed;
    std::cout << "\n  " << label << ":\n";
    std::cout << "    Total:      " << s.total_points << "\n";
    std::cout << "    Converged:  " << s.converged
              << " (" << std::setprecision(1) << s.convergence_rate() << "%)\n";
    std::cout << "    Failed:     " << s.failed << "\n";
    std::cout << "    Q4: " << s.Q4_count
              << "  Q3: " << s.Q3_count
              << "  Q2: " << s.Q2_count
              << "  Q1: " << s.Q1_count
              << "  Q0: " << s.Q0_count << "\n";
    std::cout << "    Avg quality: " << std::setprecision(1) << s.avg_quality << "\n";
    if (s.worst_quality < 100.0) {
        std::cout << "    Worst: " << std::setprecision(1) << s.worst_quality
                  << " (" << s.worst_species << " at "
                  << std::setprecision(0) << s.worst_T << "K, "
                  << std::setprecision(1) << s.worst_P << "atm)\n";
    }
}

// ============================================================================
// Quick check: one analysis per species, all models
// ============================================================================

static int run_quick_check(const PipelineConfig& cfg) {
    std::cout << "\n[gas3] Quick check: all species, all models, STP\n\n";
    std::cout << std::fixed;
    std::cout << "  " << std::setw(8) << std::left << "Species"
              << std::setw(8) << "Model"
              << std::setw(10) << "T(K)"
              << std::setw(10) << "P(atm)"
              << std::setw(12) << "Z"
              << std::setw(8) << "Iters"
              << std::setw(8) << "Conv"
              << std::setw(14) << "Residual"
              << std::setw(8) << "Score"
              << std::setw(6) << "Tier" << "\n";
    std::cout << "  " << std::string(92, '-') << "\n";

    SweepStats stats;
    for (const auto& [formula, _] : gas2::species_database()) {
        for (auto eos : {gas2::EOSType::Ideal, gas2::EOSType::VanDerWaals,
                          gas2::EOSType::RedlichKwong}) {
            auto r = generate_state(formula, 298.15, 1.0, eos, 1.0,
                                     SampleMode::Linear, 0, 0);
            stats.update(r);

            std::cout << "  " << std::setw(8) << std::left << r.species
                      << std::setw(8) << r.model_name
                      << std::setw(10) << std::setprecision(1) << r.T_K
                      << std::setw(10) << r.P_atm()
                      << std::setw(12) << std::setprecision(6) << r.Z
                      << std::setw(8) << r.iterations
                      << std::setw(8) << (r.converged ? "yes" : "NO")
                      << std::setw(14) << std::scientific << std::setprecision(2) << r.residual
                      << std::fixed
                      << std::setw(8) << std::setprecision(0) << r.quality_score
                      << std::setw(6) << tier_name(r.quality_tier) << "\n";
        }
    }
    stats.finalize();
    print_stats("Quick Check Summary", stats);
    return 0;
}

// ============================================================================
// Full pipeline
// ============================================================================

static int run_full_pipeline(const PipelineConfig& cfg) {
    std::cout << "\n[gas3] Full Quality Pipeline\n";
    std::cout << "  T range: " << cfg.sweep.T_min_K << " - "
              << cfg.sweep.T_max_K << " K (step " << cfg.sweep.T_step_K << ")\n";
    std::cout << "  P grid: " << cfg.sweep.P_grid_atm.size() << " points\n";
    std::cout << "  Random: " << cfg.sweep.random_count << " samples\n";
    std::cout << "  Models: " << cfg.sweep.models.size() << "\n";

    // Create output directory
    std::string run_dir = create_run_dir(cfg.output_dir, cfg.run_name);
    std::cout << "  Output: " << run_dir << "\n";

    // Stage A: Linear sweep
    std::cout << "\n  Stage A: Linear deterministic sweep...\n";
    SweepStats linear_stats;
    auto linear_records = linear_sweep(cfg.sweep, linear_stats,
        [&](const GasStateRecord& r) {
            if (cfg.verbose) std::cout << "    " << r.to_log_line() << "\n";
        });
    print_stats("Linear Sweep", linear_stats);

    // Stage B: Random sweep
    std::cout << "\n  Stage B: Random sampling...\n";
    SweepStats random_stats;
    auto random_records = random_sweep(cfg.sweep, random_stats,
        [&](const GasStateRecord& r) {
            if (cfg.verbose) std::cout << "    " << r.to_log_line() << "\n";
        });
    print_stats("Random Sweep", random_stats);

    // Stage C: Adaptive refinement
    std::cout << "\n  Stage C: Adaptive refinement...\n";
    SweepStats adaptive_stats;
    // Combine linear + random for adaptive analysis
    auto combined = linear_records;
    combined.insert(combined.end(), random_records.begin(), random_records.end());
    auto adaptive_records = adaptive_sweep(combined, cfg.sweep, adaptive_stats,
        [&](const GasStateRecord& r) {
            if (cfg.verbose) std::cout << "    " << r.to_log_line() << "\n";
        });
    print_stats("Adaptive Sweep", adaptive_stats);

    // Merge all records
    auto all_records = linear_records;
    all_records.insert(all_records.end(), random_records.begin(), random_records.end());
    all_records.insert(all_records.end(), adaptive_records.begin(), adaptive_records.end());

    // CSV exports
    if (cfg.csv_output) {
        std::cout << "\n  Exporting CSV...\n";
        write_csv(run_dir + "/csv/linear_sweep.csv", linear_records);
        write_csv(run_dir + "/csv/random_sweep.csv", random_records);
        write_csv(run_dir + "/csv/adaptive_sweep.csv", adaptive_records);
        write_failures_csv(run_dir + "/csv/failures.csv", all_records);
        auto fit_ready = filter_by_tier(all_records, cfg.sweep.fit_min_tier);
        write_csv(run_dir + "/csv/fit_ready.csv", fit_ready);
        std::cout << "    " << fit_ready.size() << " fit-ready records (>="
                  << tier_name(cfg.sweep.fit_min_tier) << ")\n";
    }

    // Curve fitting
    std::cout << "\n  Curve fitting...\n";
    auto fit_data = filter_by_tier(all_records, QualityTier::Q3);
    // Filter for VdW model only for surface fitting
    std::vector<double> fit_T, fit_P, fit_Z;
    for (const auto& r : fit_data) {
        if (r.model_name == "vdW" && !std::isnan(r.Z)) {
            fit_T.push_back(r.T_K);
            fit_P.push_back(r.P_atm());
            fit_Z.push_back(r.Z);
        }
    }

    ReportFitData report_fits;
    if (fit_T.size() >= 10) {
        auto fit2d = poly_fit_2d(fit_T, fit_P, fit_Z, cfg.fit_degree_2d,
                                  "Z(T,P) VdW surface", "T_K", "P_atm", "Z");
        std::cout << "    Z(T,P) surface fit: " << fit2d.n_terms << " terms, "
                  << fit2d.n_train << " train, " << fit2d.n_valid << " valid\n";
        if (!std::isnan(fit2d.r_squared))
            std::cout << "    R^2=" << std::fixed << std::setprecision(6) << fit2d.r_squared
                      << "  MAPE=" << std::setprecision(2) << fit2d.mape << "%\n";

        report_fits.z_surface = fit2d;
        report_fits.has_z_surface = true;

        // Write fit params
        std::ofstream ff(run_dir + "/fits/fit_params.csv");
        if (ff.is_open()) {
            ff << "fit_name,term,i,j,coefficient\n";
            for (size_t k = 0; k < fit2d.coeffs.size(); ++k) {
                ff << "Z_TP_surface," << k << ","
                   << fit2d.powers[k].first << "," << fit2d.powers[k].second
                   << "," << std::scientific << std::setprecision(10) << fit2d.coeffs[k] << "\n";
            }
        }
    } else {
        std::cout << "    Insufficient Q3+ data for surface fit ("
                  << fit_T.size() << " points)\n";
    }

    // Build manifest
    RunManifest manifest;
    manifest.run_id = fs::path(run_dir).filename().string();
    manifest.timestamp = utc_timestamp();
    manifest.description = "Gas3 full quality pipeline";
    manifest.total_records = all_records.size();
    manifest.linear_records = linear_records.size();
    manifest.random_records = random_records.size();
    manifest.adaptive_records = adaptive_records.size();
    manifest.linear_stats = linear_stats;
    manifest.random_stats = random_stats;
    manifest.adaptive_stats = adaptive_stats;
    for (const auto& [key, _] : gas2::species_database())
        manifest.species_list.push_back(key);
    manifest.models_used = {"ideal", "vdW", "RK"};

    // HTML report
    if (cfg.html_output) {
        std::cout << "\n  Generating HTML report...\n";
        write_html_report(run_dir + "/html/index.html", manifest, all_records, report_fits);
        std::cout << "    " << run_dir << "/html/index.html\n";
    }

    // Manifest
    if (cfg.json_manifest) {
        write_manifest(run_dir + "/run_manifest.json", manifest);
    }

    // Final summary
    std::cout << "\n  ==============================\n";
    std::cout << "  PIPELINE COMPLETE\n";
    std::cout << "  Total records: " << all_records.size() << "\n";
    std::cout << "  Output dir:    " << run_dir << "\n";
    std::cout << "  ==============================\n\n";

    return 0;
}

// ============================================================================
// CLI dispatch: vsepr gas3 <subcommand>
// ============================================================================

static void show_gas3_help() {
    std::cout << R"(
gas3 MODULE -- Thermodynamic Data Quality, Fitting, and Reporting
=================================================================

USAGE:
    vsepr gas3 <command> [options]

COMMANDS:
    quick         Quick sanity check: all species, all models, STP
    sweep         Linear deterministic sweep with CSV/HTML output
    random        Random sampling stress test
    pipeline      Full quality pipeline (linear + random + adaptive + fit + report)
    tui [FORMULA] Interactive terminal UI — species browser, analysis, live sweep
    help          Show this help

OPTIONS:
    --T-min <K>        Minimum temperature (default: 100)
    --T-max <K>        Maximum temperature (default: 2000)
    --T-step <K>       Temperature step (default: 50)
    --P-grid <list>    Pressure grid (comma-separated atm, default: 0.5,1,2,5,10,20,50,100)
    --random <N>       Number of random samples (default: 500)
    --seed <S>         RNG seed (default: 42)
    --species <list>   Comma-separated species (default: all)
    --output <dir>     Output directory (default: output/gas_runs)
    --name <name>      Run name (default: timestamp)
    --fit-degree <N>   Polynomial fit degree (default: 2 for 2D)
    --min-tier <N>     Minimum quality tier for fitting: 0-4 (default: 2)
    --verbose          Print every state to stdout
    --no-csv           Skip CSV export
    --no-html          Skip HTML report

QUALITY TIERS:
    Q4 (90-100)   Reference-grade, cross-validated
    Q3 (75-89)    Production-grade, high confidence
    Q2 (55-74)    Usable, modest residual
    Q1 (25-54)    Weak, suspicious
    Q0 (<25)      Failed, nonphysical, or diverged

EXAMPLES:
    vsepr gas3 quick
    vsepr gas3 sweep --T-min 200 --T-max 1000 --T-step 25
    vsepr gas3 pipeline --verbose --name co2_study --species CO2,N2,Ar
    vsepr gas3 pipeline --random 2000 --seed 777

OUTPUT STRUCTURE:
    output/gas_runs/<run_name>/
        run_manifest.json
        csv/
            linear_sweep.csv
            random_sweep.csv
            adaptive_sweep.csv
            failures.csv
            fit_ready.csv
        html/
            index.html
        fits/
            fit_params.csv

)";
}

int gas3_dispatch(int argc, char** argv) {
    if (argc < 3) {
        show_gas3_help();
        return 0;
    }

    std::string subcmd = argv[2];

    if (subcmd == "help" || subcmd == "--help" || subcmd == "-h") {
        show_gas3_help();
        return 0;
    }

    // Parse common options
    PipelineConfig cfg;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--T-min" && i + 1 < argc) cfg.sweep.T_min_K = std::stod(argv[++i]);
        else if (arg == "--T-max" && i + 1 < argc) cfg.sweep.T_max_K = std::stod(argv[++i]);
        else if (arg == "--T-step" && i + 1 < argc) cfg.sweep.T_step_K = std::stod(argv[++i]);
        else if (arg == "--random" && i + 1 < argc) cfg.sweep.random_count = std::stoull(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) cfg.sweep.seed = std::stoull(argv[++i]);
        else if (arg == "--output" && i + 1 < argc) cfg.output_dir = argv[++i];
        else if (arg == "--name" && i + 1 < argc) cfg.run_name = argv[++i];
        else if (arg == "--fit-degree" && i + 1 < argc) {
            cfg.fit_degree_2d = std::stoi(argv[++i]);
            cfg.fit_degree_1d = cfg.fit_degree_2d + 1;
        }
        else if (arg == "--min-tier" && i + 1 < argc) {
            int t = std::stoi(argv[++i]);
            cfg.sweep.fit_min_tier = static_cast<QualityTier>(std::min(std::max(t, 0), 4));
        }
        else if (arg == "--verbose" || arg == "-v") cfg.verbose = true;
        else if (arg == "--no-csv") cfg.csv_output = false;
        else if (arg == "--no-html") cfg.html_output = false;
        else if (arg == "--species" && i + 1 < argc) {
            std::string sp_list = argv[++i];
            std::istringstream iss(sp_list);
            std::string tok;
            while (std::getline(iss, tok, ',')) {
                if (!tok.empty()) cfg.sweep.species_list.push_back(tok);
            }
        }
        else if (arg == "--P-grid" && i + 1 < argc) {
            cfg.sweep.P_grid_atm.clear();
            std::string p_list = argv[++i];
            std::istringstream iss(p_list);
            std::string tok;
            while (std::getline(iss, tok, ',')) {
                if (!tok.empty()) cfg.sweep.P_grid_atm.push_back(std::stod(tok));
            }
        }
    }

    // --- quick ---
    if (subcmd == "quick") {
        return run_quick_check(cfg);
    }

    // --- sweep ---
    if (subcmd == "sweep") {
        std::string run_dir = create_run_dir(cfg.output_dir, cfg.run_name);
        std::cout << "\n[gas3] Linear sweep -> " << run_dir << "\n";
        SweepStats stats;
        auto records = linear_sweep(cfg.sweep, stats,
            [&](const GasStateRecord& r) {
                if (cfg.verbose) std::cout << "  " << r.to_log_line() << "\n";
            });
        print_stats("Linear Sweep", stats);
        if (cfg.csv_output) {
            write_csv(run_dir + "/csv/linear_sweep.csv", records);
            std::cout << "  CSV: " << run_dir << "/csv/linear_sweep.csv\n";
        }
        RunManifest m;
        m.run_id = fs::path(run_dir).filename().string();
        m.timestamp = utc_timestamp();
        m.description = "Linear sweep only";
        m.total_records = m.linear_records = records.size();
        m.linear_stats = stats;
        for (const auto& [key, _] : gas2::species_database())
            m.species_list.push_back(key);
        m.models_used = {"ideal", "vdW", "RK"};
        if (cfg.html_output) {
            write_html_report(run_dir + "/html/index.html", m, records);
            std::cout << "  HTML: " << run_dir << "/html/index.html\n";
        }
        write_manifest(run_dir + "/run_manifest.json", m);
        return 0;
    }

    // --- random ---
    if (subcmd == "random") {
        std::string run_dir = create_run_dir(cfg.output_dir, cfg.run_name);
        std::cout << "\n[gas3] Random sweep (" << cfg.sweep.random_count
                  << " samples) -> " << run_dir << "\n";
        SweepStats stats;
        auto records = random_sweep(cfg.sweep, stats,
            [&](const GasStateRecord& r) {
                if (cfg.verbose) std::cout << "  " << r.to_log_line() << "\n";
            });
        print_stats("Random Sweep", stats);
        if (cfg.csv_output)
            write_csv(run_dir + "/csv/random_sweep.csv", records);
        return 0;
    }

    // --- pipeline ---
    if (subcmd == "pipeline") {
        return run_full_pipeline(cfg);
    }

    // --- tui ---
    if (subcmd == "tui") {
        std::string tui_formula = (argc >= 4 && argv[3][0] != '-') ? argv[3] : "Ar";
        double tui_T = cfg.sweep.T_min_K > 0 ? 298.15 : 298.15;
        double tui_P = 1.0;
        // Parse optional -T/-P overrides
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if ((a == "-T" || a == "--temperature") && i + 1 < argc) tui_T = std::stod(argv[++i]);
            else if ((a == "-P" || a == "--pressure")  && i + 1 < argc) tui_P = std::stod(argv[++i]);
        }
        return vsepr::gas2::gas2_tui_run(tui_formula, tui_T, tui_P, 1.0);
    }

    std::cerr << "Unknown gas3 subcommand: " << subcmd << "\n";
    std::cerr << "Run 'vsepr gas3 help' for usage.\n";
    return 1;
}

} // namespace gas3
} // namespace vsepr
