#include "atomistic/golden_run/gr1_condensation.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
	std::filesystem::path input = "examples/golden_runs/GR-1-gas-to-liquid.gr1";
	std::filesystem::path output = "out/GR-1-gas-to-liquid";
	for (int i = 1; i < argc; ++i) {
		const std::string argument = argv[i];
		if (argument == "--input" && i + 1 < argc) input = argv[++i];
		else if (argument == "--output" && i + 1 < argc) output = argv[++i];
		else if (argument == "--help") {
			std::cout << "Usage: gr1-condensation [--input FILE] [--output DIRECTORY]\n";
			return 0;
		} else {
			std::cerr << "Unknown argument: " << argument << '\n';
			return 2;
		}
	}

	try {
		const auto config = atomistic::golden_run::load_gr1_config(input);
		const auto result = atomistic::golden_run::run_gr1(config, true, output);
		std::cout << "scenario           GR-1 Gas-to-Liquid Relaxation\n"
				  << "transition         gas -> liquid\n"
				  << "phase              " << (result.golden_run_passed ? "liquid" : "unconfirmed") << '\n'
				  << "restart replay     " << (result.restart_matched ? "MATCH" : "MISMATCH") << '\n'
				  << "render cadence     " << config.render_fps << " fps\n"
				  << "trajectory hash    " << result.trajectory_hash << '\n'
				  << "run manifest       " << (output / "manifest.json").string() << '\n'
				  << (result.golden_run_passed ? "GOLDEN RUN PASSED\n" : "GOLDEN RUN FAILED\n");
		return result.golden_run_passed ? 0 : 1;
	} catch (const std::exception& error) {
		std::cerr << "GR-1 failed: " << error.what() << '\n';
		return 2;
	}
}
