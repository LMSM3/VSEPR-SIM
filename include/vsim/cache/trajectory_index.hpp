#pragma once
// WO-72V — hash-indexed trajectory summary index
// Maps trajectory file paths to a lightweight summary record so the CLI
// can answer "what ran?" queries without re-parsing full .dynx/.x bundles.
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cstdint>
#include <optional>

namespace vsim::cache {

struct TrajectorySummary {
	std::string  path;           // canonical file path
	std::string  formula;        // system formula at frame 0
	std::size_t  n_atoms   = 0;
	std::size_t  n_frames  = 0;
	double       dt_fs     = 0.0; // timestep in femtoseconds
	double       total_ps  = 0.0; // total simulation time (ps)
	double       T_mean_K  = 0.0; // mean temperature
	double       E_mean_eV = 0.0; // mean total energy
	uint64_t     file_hash = 0;   // xxHash64 of first 4 KB (fast fingerprint)
	std::string  run_date;        // ISO-8601 date string
};

class TrajectoryIndex {
public:
	// Register a summary (overwrites on same path).
	void register_run(const TrajectorySummary& s);

	std::optional<TrajectorySummary> find(const std::string& path) const;
	const std::vector<TrajectorySummary>& all() const { return entries_; }

	// Persist index to / load from a simple JSON sidecar.
	bool save(const std::filesystem::path& index_path) const;
	bool load(const std::filesystem::path& index_path);

	std::size_t size() const { return entries_.size(); }

private:
	std::vector<TrajectorySummary> entries_;
	std::unordered_map<std::string, std::size_t> path_idx_; // path -> entries_ index
};

} // namespace vsim::cache
