// src/v4/uff/uff_batch_validator.cpp
// Formation Engine v4.1.0 -- UFX 10% batch validator implementation

#include "uff_batch_validator.hpp"
#include <thread>
#include <sstream>
#include <cmath>

namespace vsepr::uff {

UFFBatchValidator::UFFBatchValidator(std::chrono::milliseconds pause)
	: pause_(pause)
{}

// ---------------------------------------------------------------------------
// validate_entry_ -- returns a list of error strings (empty = valid)
// ---------------------------------------------------------------------------

std::vector<std::string> UFFBatchValidator::validate_entry_(const UFFEntry& e) {
	std::vector<std::string> errs;

	if (e.atom_type.empty())
		errs.push_back(e.atom_type + ": atom_type is empty");
	if (e.element.empty())
		errs.push_back(e.atom_type + ": element is empty");
	if (e.r1 <= 0.0)
		errs.push_back(e.atom_type + ": r1=" + std::to_string(e.r1) + " must be > 0");
	if (e.x1 <= 0.0)
		errs.push_back(e.atom_type + ": x1=" + std::to_string(e.x1) + " must be > 0");
	if (e.D1 <= 0.0)
		errs.push_back(e.atom_type + ": D1=" + std::to_string(e.D1) + " must be > 0");
	if (e.zeta <= 0.0)
		errs.push_back(e.atom_type + ": zeta=" + std::to_string(e.zeta) + " must be > 0");
	if (e.theta0 <= 0.0 || e.theta0 > 360.0)
		errs.push_back(e.atom_type + ": theta0=" + std::to_string(e.theta0)
					   + " must be in (0, 360]");
	if (e.confidence == ParamConfidence::Missing)
		errs.push_back(e.atom_type + ": confidence tag is Missing");

	return errs;
}

// ---------------------------------------------------------------------------
// run -- 10 slices, each slice = ~10% of the table
// ---------------------------------------------------------------------------

bool UFFBatchValidator::run(const UFFTable& table, UFFLiveRelay& relay) {
	results_.clear();

	auto all = table.all_entries();
	const int n = static_cast<int>(all.size());
	if (n == 0) return true;

	constexpr int k_batches = 10;
	bool all_passed = true;

	for (int b = 0; b < k_batches; ++b) {
		const int start = (b * n) / k_batches;
		const int end   = ((b + 1) * n) / k_batches;

		BatchResult result;
		result.batch_number = b + 1;
		result.total        = end - start;

		for (int i = start; i < end; ++i) {
			auto errs = validate_entry_(all[static_cast<std::size_t>(i)]);
			if (errs.empty()) {
				++result.valid;
			} else {
				for (auto& e : errs) result.errors.push_back(std::move(e));
			}
		}

		result.passed = result.errors.empty();
		if (!result.passed) all_passed = false;

		results_.push_back(result);
		relay.on_batch_result(result);

		std::this_thread::sleep_for(pause_);
	}

	return all_passed;
}

} // namespace vsepr::uff
