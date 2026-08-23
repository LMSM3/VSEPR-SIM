/**
 * property_train.cpp  -  Property-Based Training Entry Point
 *
 * Runs the continual VSEPR-SIM formation engine with property-based invariant
 * checking and a live ASCII visual dashboard.
 *
 * Usage:
 *   property_train [options]
 *
 * Options:
 *   --formations N     Maximum number of formations (default: 200, 0 = infinite)
 *   --seeds N          Seeds per formula (default: 3)
 *   --steps N          FIRE steps per formation (default: 1000)
 *   --atoms-min N      Minimum atoms per formula (default: 2)
 *   --atoms-max N      Maximum atoms per formula (default: 12)
 *   --out DIR          Output directory (default: property_train_out)
 *   --dashboard N      Refresh dashboard every N formations (default: 10)
 *   --no-color         Disable ANSI color output
 *   --no-csv           Skip CSV export
 *   --no-json          Skip JSON export
 *   --help             Print this help
 *
 * Outputs (in --out DIR):
 *   training_ledger.csv    Per-formation row ledger (appended, survives restart)
 *   training_results.csv   Full DataTable export
 *   training_results.json  Full DataTable export (JSON)
 *   session_summary.csv    PropertyCard: aggregate session stats + invariant rates
 *   session_summary.json   Same as JSON
 *   results/               Per-formation JSON (optional, if --verbose)
 *
 * Live dashboard columns:
 *   - Formation count / rate
 *   - Convergence % bar
 *   - Stability % bar
 *   - Per-invariant pass rate + worst margin
 *   - Energy/atom distribution (min/mean/max + ASCII number line)
 *   - Last result detail + any invariant failures
 *
 * To stop a running session gracefully:
 *   touch property_train_out/STOP
 *   (or Ctrl+C)
 *
 * VSEPR-SIM v5.0.0-main | WO-VSIM-74-PROP-TRAIN
 */

#include "training/property_trainer.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

static void print_help(const char* prog) {
	std::cout
		<< "Usage: " << prog << " [options]\n\n"
		<< "Property-Based Training over VSEPR-SIM Formation Engine\n\n"
		<< "Options:\n"
		<< "  --formations N     Maximum formations (default 200, 0=infinite)\n"
		<< "  --seeds N          Seeds per formula (default 3)\n"
		<< "  --steps N          FIRE steps per formation (default 1000)\n"
		<< "  --atoms-min N      Min atoms per formula (default 2)\n"
		<< "  --atoms-max N      Max atoms per formula (default 12)\n"
		<< "  --out DIR          Output directory (default property_train_out)\n"
		<< "  --dashboard N      Dashboard refresh interval (default 10)\n"
		<< "  --no-color         Disable ANSI color\n"
		<< "  --no-csv           Skip CSV export\n"
		<< "  --no-json          Skip JSON export\n"
		<< "  --help             This message\n\n"
		<< "Stop a running session: touch <out>/STOP\n";
}

int main(int argc, char* argv[]) {
	using namespace vsepr::training;

	TrainerConfig cfg;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		auto next = [&]() -> std::string {
			if (i + 1 >= argc)
				throw std::runtime_error("Missing value for " + arg);
			return argv[++i];
		};
		auto next_int = [&]() { return std::stoi(next()); };

		if      (arg == "--help")        { print_help(argv[0]); return 0; }
		else if (arg == "--formations")  cfg.max_formations    = next_int();
		else if (arg == "--seeds")       cfg.seeds_per_formula = next_int();
		else if (arg == "--steps")       cfg.fire_steps        = next_int();
		else if (arg == "--atoms-min")   cfg.min_atoms         = next_int();
		else if (arg == "--atoms-max")   cfg.max_atoms         = next_int();
		else if (arg == "--out")         cfg.export_dir        = next();
		else if (arg == "--dashboard")   cfg.dashboard_every   = next_int();
		else if (arg == "--no-color")    cfg.color_ansi        = false;
		else if (arg == "--no-csv")      cfg.export_csv        = false;
		else if (arg == "--no-json")     cfg.export_json       = false;
		else {
			std::cerr << "Unknown option: " << arg << "\n";
			print_help(argv[0]);
			return 1;
		}
	}

	// Validate
	if (cfg.min_atoms < 1) cfg.min_atoms = 1;
	if (cfg.max_atoms < cfg.min_atoms) cfg.max_atoms = cfg.min_atoms;
	if (cfg.dashboard_every < 1) cfg.dashboard_every = 1;
	if (cfg.fire_steps < 1)      cfg.fire_steps = 100;

	std::cout << "VSEPR-SIM  |  Property-Based Training\n"
			  << "  Formations  : " << (cfg.max_formations == 0 ? "infinite" : std::to_string(cfg.max_formations)) << "\n"
			  << "  Seeds/form  : " << cfg.seeds_per_formula << "\n"
			  << "  FIRE steps  : " << cfg.fire_steps << "\n"
			  << "  Atom range  : " << cfg.min_atoms << " - " << cfg.max_atoms << "\n"
			  << "  Output      : " << cfg.export_dir << "\n"
			  << "  Invariants  : " << InvariantRegistry::default_registry().size() << "\n"
			  << "\n  Starting in 1s...\n\n";

	// Brief pause so the user sees the preamble before dashboard takes over
	std::this_thread::sleep_for(std::chrono::seconds(1));

	PropertyTrainer trainer(cfg);
	trainer.run();

	return 0;
}
