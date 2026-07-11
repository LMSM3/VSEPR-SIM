// src/v4/uff/uff_batch_validator.hpp
// Formation Engine v4.1.0 -- UFX 10% batch field validator
//
// Splits all entries currently in the runtime table into 10 equal slices
// and validates field-level sanity for every entry in each slice.
//
// Validation rules (Rev 1):
//   r1     > 0
//   x1     > 0
//   D1     > 0
//   zeta   > 0
//   theta0 in (0, 360]
//   atom_type non-empty
//   element   non-empty
//   confidence != Missing
//
// Results (including per-entry error strings) are emitted live through
// UFFLiveRelay::on_batch_result() as each batch completes.

#pragma once

#include "uff_table.hpp"
#include "uff_live_relay.hpp"
#include <vector>
#include <string>
#include <chrono>

namespace vsepr::uff {

class UFFBatchValidator {
public:
	// pause_between_batches: how long to sleep after each batch result so
	// the dashboard update is visible to the user.
	explicit UFFBatchValidator(
		std::chrono::milliseconds pause_between_batches =
			std::chrono::milliseconds(200));

	// Run all 10 batch slices over the current table contents.
	// Calls relay.on_batch_result() after each slice.
	// Returns false if any batch failed.
	bool run(const UFFTable& table, UFFLiveRelay& relay);

	// Access results after run() completes.
	const std::vector<BatchResult>& results() const { return results_; }

private:
	std::chrono::milliseconds       pause_;
	std::vector<BatchResult>        results_;

	static std::vector<std::string> validate_entry_(const UFFEntry& e);
};

} // namespace vsepr::uff
