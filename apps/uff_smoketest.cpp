// apps/uff_smoketest.cpp
// Formation Engine v4.1.0 -- UFX population with live shell relay
//
// Populates the full UFF runtime table from the Rappé 1992 baseline,
// streams every entry through the live ANSI dashboard, runs 10% batch
// validation with live error output, then spot-checks OsO4 and CH3HgI.
//
// Usage: uff_smoketest [output_dir]
//   output_dir  -- optional path for CSV/JSONL logs (default: "output")

#include "v4/uff/run_stage.hpp"
#include "v4/uff/uff_table.hpp"
#include "v4/uff/uff_reference_provider.hpp"
#include "v4/uff/uff_autocreate.hpp"
#include "v4/uff/uff_provenance_writer.hpp"
#include "v4/uff/uff_spotcheck.hpp"
#include "v4/uff/uff_live_relay.hpp"
#include "v4/uff/uff_batch_validator.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

int main(int argc, char* argv[]) {
	using namespace vsepr::uff;
	using namespace std::chrono_literals;

	const std::string output_dir = (argc > 1) ? argv[1] : "output";

	// ------------------------------------------------------------------
	// Bootstrap: build the reference provider and count total entries
	// so the live relay can size its progress bar correctly.
	// ------------------------------------------------------------------
	LocalUFFReferenceProvider seeder;
	const auto all_types = seeder.known_types();
	const int  total     = static_cast<int>(all_types.size());

	UFFLiveRelay relay(total);
	relay.start();

	// ------------------------------------------------------------------
	// 1. Seed the runtime table — stream every entry through the relay
	// ------------------------------------------------------------------
	relay.on_stage(RunStage::LoadingUFF);

	UFFTable            table;
	UFFProvenanceWriter prov_writer(output_dir);

	for (const auto& at : all_types) {
		auto entry = seeder.lookup(at);
		if (entry.has_value()) {
			table.insert(*entry);
			prov_writer.write_provenance_record(*entry);
			relay.on_entry(*entry);
		}
		// Small pause so the animation is visible during fast loads.
		std::this_thread::sleep_for(12ms);
	}

	// ------------------------------------------------------------------
	// 2. Auto-creator ready for fallback on any missing types
	// ------------------------------------------------------------------
	relay.on_stage(RunStage::AutoCreating);
	auto ref_provider = make_local_reference_provider();
	UFFAutoCreator auto_creator(*ref_provider);
	std::this_thread::sleep_for(80ms);

	// ------------------------------------------------------------------
	// 3. Flush provenance logs
	// ------------------------------------------------------------------
	relay.on_stage(RunStage::WritingLogs);
	prov_writer.flush();
	std::this_thread::sleep_for(60ms);

	// ------------------------------------------------------------------
	// 4. 10% batch validation — results appear live in the dashboard
	// ------------------------------------------------------------------
	relay.on_stage(RunStage::SpotChecking);

	UFFBatchValidator validator(200ms);
	const bool batches_ok = validator.run(table, relay);

	// ------------------------------------------------------------------
	// 5. Spot-check OsO4 and CH3HgI
	// ------------------------------------------------------------------
	SpotCheckHarness harness(table, auto_creator, prov_writer);
	const auto molecules = std::vector<SpotMolecule>{
		SpotCheckHarness::make_OsO4(),
		SpotCheckHarness::make_CH3HgI(),
	};
	const auto spot_results = harness.run_all(molecules);
	prov_writer.flush();

	// ------------------------------------------------------------------
	// 6. Stop relay, print spot-check summary
	// ------------------------------------------------------------------
	relay.on_stage(batches_ok ? RunStage::Complete : RunStage::Failed);
	relay.stop();

	// Spot-check summary (printed below the frozen dashboard)
	int spot_failures = 0;
	for (const auto& r : spot_results) {
		const char* tag = r.passed ? "\033[92mPASS\033[0m" : "\033[91mWARN\033[0m";
		std::cout << "[UFX] [" << tag << "] "
				  << r.test_id << "  " << r.molecule
				  << "  missing=" << r.missing_count
				  << "  " << r.notes << '\n';
		if (!r.passed) ++spot_failures;
	}

	// Batch validation error summary (if any failures)
	if (!batches_ok) {
		std::cout << "\n\033[91m[UFX] Batch validation errors:\033[0m\n";
		for (const auto& br : validator.results()) {
			if (!br.passed) {
				std::cout << "  Batch #" << br.batch_number << ":\n";
				for (const auto& err : br.errors)
					std::cout << "    \033[91m⚠\033[0m " << err << '\n';
			}
		}
	}

	const int total_failures = spot_failures + (batches_ok ? 0 : 1);
	return total_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
