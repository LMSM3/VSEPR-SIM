#pragma once
/**
 * include/batch/batch_checkpoint.hpp
 * =====================================
 * WO-VSIM-62C — Checkpoint System
 *
 * Checkpoint file: <study.name>/batch_checkpoint.json
 *
 *   {
 *     "study_name":      "nacl_verification_batch",
 *     "total_runs":      27,
 *     "completed_runs":  14,
 *     "completed_run_ids": ["run_0001", ...],
 *     "failed_run_ids":  ["run_0007"]
 *   }
 *
 * On resume the expander regenerates the full run list; the runner
 * skips any run_id already in completed_run_ids.
 * Failed runs are re-attempted unless abort_on_fail = true.
 *
 * WO-VSIM-62C | beta-12
 */

#include <string>
#include <vector>

namespace vsim {
namespace batch {

struct BatchCheckpoint {
	std::string study_name;
	int         total_runs     = 0;
	int         completed_runs = 0;
	std::vector<std::string> completed_run_ids;
	std::vector<std::string> failed_run_ids;
};

class CheckpointManager {
public:
	static void           save(const BatchCheckpoint& cp, const std::string& dir);
	static BatchCheckpoint load(const std::string& dir);
	static bool           exists(const std::string& dir);

	static void mark_complete(const std::string& dir, const std::string& run_id);
	static void mark_failed  (const std::string& dir, const std::string& run_id);

	static constexpr const char* kFilename = "batch_checkpoint.json";
};

} // namespace batch
} // namespace vsim
