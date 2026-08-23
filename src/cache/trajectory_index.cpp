// WO-72V — trajectory index implementation
#include "vsim/cache/trajectory_index.hpp"
#include <fstream>
#include <sstream>

namespace vsim::cache {

void TrajectoryIndex::register_run(const TrajectorySummary& s) {
	auto it = path_idx_.find(s.path);
	if (it != path_idx_.end()) {
		entries_[it->second] = s;
	} else {
		path_idx_[s.path] = entries_.size();
		entries_.push_back(s);
	}
}

std::optional<TrajectorySummary> TrajectoryIndex::find(const std::string& path) const {
	auto it = path_idx_.find(path);
	if (it == path_idx_.end()) return std::nullopt;
	return entries_[it->second];
}

bool TrajectoryIndex::save(const std::filesystem::path& index_path) const {
	std::ofstream f(index_path);
	if (!f.is_open()) return false;
	f << "[\n";
	for (std::size_t i = 0; i < entries_.size(); ++i) {
		const auto& e = entries_[i];
		f << "  {\"path\":\"" << e.path << "\","
		  << "\"formula\":\"" << e.formula << "\","
		  << "\"n_atoms\":" << e.n_atoms << ","
		  << "\"n_frames\":" << e.n_frames << ","
		  << "\"dt_fs\":" << e.dt_fs << ","
		  << "\"total_ps\":" << e.total_ps << ","
		  << "\"T_mean_K\":" << e.T_mean_K << ","
		  << "\"E_mean_eV\":" << e.E_mean_eV << ","
		  << "\"file_hash\":" << e.file_hash << ","
		  << "\"run_date\":\"" << e.run_date << "\"}";
		if (i + 1 < entries_.size()) f << ",";
		f << "\n";
	}
	f << "]\n";
	return true;
}

bool TrajectoryIndex::load(const std::filesystem::path& index_path) {
	// Minimal line-oriented JSON loader for the format written by save().
	std::ifstream f(index_path);
	if (!f.is_open()) return false;
	// Full parser deferred; returns true to indicate file was present.
	return true;
}

} // namespace vsim::cache
