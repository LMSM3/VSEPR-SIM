/**
 * src/batch/batch_checkpoint.cpp
 * ================================
 * WO-VSIM-62C — Checkpoint System Implementation
 *
 * Uses a hand-rolled minimal JSON writer/reader to avoid adding
 * a third-party JSON dependency.  The schema is simple enough
 * that a custom parser is safe and maintainable.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_checkpoint.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace batch {

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Write JSON array of strings.
static void write_str_array(std::ostream& out,
							 const std::string& indent,
							 const std::vector<std::string>& vec) {
	out << "[\n";
	for (size_t i = 0; i < vec.size(); ++i) {
		out << indent << "    \"" << vec[i] << "\"";
		if (i + 1 < vec.size()) out << ",";
		out << "\n";
	}
	out << indent << "  ]";
}

// ── save ──────────────────────────────────────────────────────────────────────

void CheckpointManager::save(const BatchCheckpoint& cp, const std::string& dir) {
	std::string path = dir + "/" + kFilename;
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("CheckpointManager: cannot write " + path);

	f << "{\n";
	f << "  \"study_name\": \"" << cp.study_name << "\",\n";
	f << "  \"total_runs\": "   << cp.total_runs  << ",\n";
	f << "  \"completed_runs\": " << cp.completed_runs << ",\n";
	f << "  \"completed_run_ids\": "; write_str_array(f, "  ", cp.completed_run_ids); f << ",\n";
	f << "  \"failed_run_ids\": ";   write_str_array(f, "  ", cp.failed_run_ids);   f << "\n";
	f << "}\n";
}

// ── load ──────────────────────────────────────────────────────────────────────

static std::vector<std::string> parse_json_str_array(const std::string& src,
													   size_t& pos) {
	// Expects cursor at or before '['; advances past ']'
	size_t open = src.find('[', pos);
	if (open == std::string::npos) return {};
	size_t close = src.find(']', open);
	if (close == std::string::npos) return {};

	std::string inner = src.substr(open + 1, close - open - 1);
	std::vector<std::string> result;
	size_t p = 0;
	while (p < inner.size()) {
		size_t q1 = inner.find('"', p);
		if (q1 == std::string::npos) break;
		size_t q2 = inner.find('"', q1 + 1);
		if (q2 == std::string::npos) break;
		result.push_back(inner.substr(q1 + 1, q2 - q1 - 1));
		p = q2 + 1;
	}
	pos = close + 1;
	return result;
}

BatchCheckpoint CheckpointManager::load(const std::string& dir) {
	std::string path = dir + "/" + kFilename;
	std::ifstream f(path);
	if (!f.is_open())
		throw std::runtime_error("CheckpointManager: cannot read " + path);

	std::string src((std::istreambuf_iterator<char>(f)),
					 std::istreambuf_iterator<char>());
	BatchCheckpoint cp;

	// Extract quoted string values by key
	auto extract_str = [&](const std::string& key) -> std::string {
		size_t k = src.find("\"" + key + "\"");
		if (k == std::string::npos) return {};
		size_t colon = src.find(':', k);
		size_t q1    = src.find('"', colon + 1);
		size_t q2    = src.find('"', q1 + 1);
		if (q1 == std::string::npos || q2 == std::string::npos) return {};
		return src.substr(q1 + 1, q2 - q1 - 1);
	};

	auto extract_int = [&](const std::string& key) -> int {
		size_t k = src.find("\"" + key + "\"");
		if (k == std::string::npos) return 0;
		size_t colon = src.find(':', k);
		size_t p = src.find_first_not_of(" \t\n\r", colon + 1);
		if (p == std::string::npos) return 0;
		try { return std::stoi(src.substr(p)); } catch (...) { return 0; }
	};

	cp.study_name     = extract_str("study_name");
	cp.total_runs     = extract_int("total_runs");
	cp.completed_runs = extract_int("completed_runs");

	{
		size_t p = 0; size_t k = src.find("\"completed_run_ids\"");
		if (k != std::string::npos) { p = k; cp.completed_run_ids = parse_json_str_array(src, p); }
	}
	{
		size_t p = 0; size_t k = src.find("\"failed_run_ids\"");
		if (k != std::string::npos) { p = k; cp.failed_run_ids = parse_json_str_array(src, p); }
	}

	return cp;
}

// ── exists ────────────────────────────────────────────────────────────────────

bool CheckpointManager::exists(const std::string& dir) {
	std::ifstream f(dir + "/" + kFilename);
	return f.good();
}

// ── mark_complete / mark_failed ───────────────────────────────────────────────

void CheckpointManager::mark_complete(const std::string& dir,
									   const std::string& run_id) {
	BatchCheckpoint cp = exists(dir) ? load(dir) : BatchCheckpoint{};
	// Remove from failed if it was there (shouldn't happen, but be safe)
	cp.failed_run_ids.erase(
		std::remove(cp.failed_run_ids.begin(), cp.failed_run_ids.end(), run_id),
		cp.failed_run_ids.end());
	if (std::find(cp.completed_run_ids.begin(), cp.completed_run_ids.end(), run_id)
		== cp.completed_run_ids.end()) {
		cp.completed_run_ids.push_back(run_id);
		cp.completed_runs = static_cast<int>(cp.completed_run_ids.size());
	}
	save(cp, dir);
}

void CheckpointManager::mark_failed(const std::string& dir,
									 const std::string& run_id) {
	BatchCheckpoint cp = exists(dir) ? load(dir) : BatchCheckpoint{};
	if (std::find(cp.failed_run_ids.begin(), cp.failed_run_ids.end(), run_id)
		== cp.failed_run_ids.end()) {
		cp.failed_run_ids.push_back(run_id);
	}
	save(cp, dir);
}

} // namespace batch
} // namespace vsim
