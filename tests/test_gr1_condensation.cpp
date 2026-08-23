#include "atomistic/golden_run/gr1_condensation.hpp"

#include <filesystem>
#include <iostream>

namespace {

bool has_state(const atomistic::golden_run::GR1Result& result, atomistic::golden_run::CompletionState state) {
	for (const auto completed : result.completed_states) if (completed == state) return true;
	return false;
}

} // namespace

int main() {
	try {
		const auto input = std::filesystem::current_path() / "examples/golden_runs/GR-1-gas-to-liquid.gr1";
		const auto output = std::filesystem::temp_directory_path() / "vsepr_sim" / "gr1-condensation-test";
		std::filesystem::remove_all(output);
		const auto config = atomistic::golden_run::load_gr1_config(input);
		const auto first = atomistic::golden_run::run_gr1(config, true, output);
		const auto second = atomistic::golden_run::run_gr1(config, false);

		const bool deterministic = first.trajectory_hash == second.trajectory_hash && first.final_state_hash == second.final_state_hash;
		const bool all_gates = has_state(first, atomistic::golden_run::CompletionState::NumericallyConverged)
			&& has_state(first, atomistic::golden_run::CompletionState::PhysicallyStable)
			&& has_state(first, atomistic::golden_run::CompletionState::LiquidStateConfirmed)
			&& has_state(first, atomistic::golden_run::CompletionState::HoldPeriodPassed)
			&& has_state(first, atomistic::golden_run::CompletionState::ConservationPassed)
			&& has_state(first, atomistic::golden_run::CompletionState::RestartMatched)
			&& has_state(first, atomistic::golden_run::CompletionState::RenderStreamValid)
			&& has_state(first, atomistic::golden_run::CompletionState::GoldenRunPassed);
		const bool artifacts = std::filesystem::exists(output / "manifest.json")
			&& std::filesystem::exists(output / "trajectory.jsonl")
			&& std::filesystem::exists(output / "render_60fps.jsonl")
			&& std::filesystem::exists(output / "checkpoint.gr1state")
			&& std::filesystem::exists(output / "restart_comparison.json");

		if (!deterministic || !first.golden_run_passed || !first.restart_matched || !all_gates || !artifacts) {
			std::cerr << "GR-1 validation failed: deterministic=" << deterministic
					  << " golden=" << first.golden_run_passed
					  << " restart=" << first.restart_matched
					  << " gates=" << all_gates
					  << " artifacts=" << artifacts << '\n';
			return 1;
		}
		std::cout << "GR-1 passed with trajectory hash " << first.trajectory_hash << '\n';
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "GR-1 validation error: " << error.what() << '\n';
		return 2;
	}
}
