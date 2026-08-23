#pragma once
/**
 * include/vsim/io/dynx_inspector.hpp
 * ====================================
 * 72-floating-6  |  V5.1.4
 *
 * Lightweight .dynx archive inspector.
 *
 * Reads only the header metadata and performs a single linear scan to count
 * per-frame record types, without storing any particle data in memory.
 * Useful before full animation playback is available, and as a pre-flight
 * validation step for viewer tools.
 *
 * Usage:
 *   DynxInspectResult r = dynx_inspect("output/run.dynx");
 *   if (!r.valid) { ... }
 *   std::cout << r.summary();
 *
 * The inspect function is intentionally header-only: no link dependency.
 *
 * 72-floating-6  |  V5.1.4  |  V5.0.0-main
 */

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace vsim {
namespace io {

// ============================================================================
// DynxFrameStats  —  per-frame record counts
// ============================================================================

struct DynxFrameStats {
	int64_t  frame_index     {-1};
	double   time_fs         {0.0};
	uint32_t particle_lines  {0};
	uint32_t force_lines     {0};
	uint32_t bond_force_lines{0};
	uint32_t field_lines     {0};
	uint32_t event_lines     {0};
	uint32_t render_lines    {0};
	uint32_t camera_lines    {0};
};

// ============================================================================
// DynxInspectResult  —  full inspection output
// ============================================================================

struct DynxInspectResult {
	// ---- header metadata ---------------------------------------------------
	std::string path;
	std::string dynx_version;
	std::string source_path;
	std::string source_hash;
	std::string kernel_version;
	std::string timestamp;
	int64_t     header_frame_count   {0};   // declared in header
	int64_t     header_particle_count{0};   // declared in header
	double      header_frame_interval{0.0}; // dt_fs declared in header

	// ---- scan results ------------------------------------------------------
	int64_t     actual_frame_count   {0};   // frames found by scan
	uint64_t    total_force_records  {0};
	uint64_t    total_bond_force_records{0};
	uint64_t    total_field_records  {0};
	uint64_t    total_event_records  {0};
	uint64_t    total_render_records {0};
	uint64_t    total_camera_records {0};

	std::vector<DynxFrameStats> frames;     // one entry per frame

	// ---- validation --------------------------------------------------------
	bool        valid               {false};
	std::string error_msg;                  // non-empty on failure

	// ---- helpers -----------------------------------------------------------
	bool header_matches_scan() const noexcept {
		return header_frame_count == actual_frame_count;
	}

	std::string summary() const {
		if (!valid) return "[DYNX INSPECT] ERROR: " + error_msg + "\n";
		char buf[512];
		std::snprintf(buf, sizeof(buf),
			"[DYNX INSPECT] %s\n"
			"  dynx version    : %s\n"
			"  source          : %s\n"
			"  kernel          : %s\n"
			"  timestamp       : %s\n"
			"  frames (hdr/act): %lld / %lld  %s\n"
			"  particles       : %lld\n"
			"  FORCE records   : %llu\n"
			"  BOND_FORCE      : %llu\n"
			"  FIELD records   : %llu\n"
			"  EVENT records   : %llu\n"
			"  RENDER records  : %llu\n"
			"  CAMERA records  : %llu\n",
			path.c_str(),
			dynx_version.c_str(),
			source_path.c_str(),
			kernel_version.c_str(),
			timestamp.c_str(),
			(long long)header_frame_count,
			(long long)actual_frame_count,
			header_matches_scan() ? "(OK)" : "(MISMATCH)",
			(long long)header_particle_count,
			(unsigned long long)total_force_records,
			(unsigned long long)total_bond_force_records,
			(unsigned long long)total_field_records,
			(unsigned long long)total_event_records,
			(unsigned long long)total_render_records,
			(unsigned long long)total_camera_records);
		return std::string(buf);
	}
};

// ============================================================================
// dynx_inspect()  —  inspect a .dynx file from a path
// ============================================================================

inline DynxInspectResult dynx_inspect(const std::string& path) {
	DynxInspectResult r;
	r.path = path;

	std::ifstream f(path);
	if (!f.is_open()) {
		r.error_msg = "cannot open: " + path;
		return r;
	}

	std::string line;
	DynxFrameStats* cur = nullptr;

	while (std::getline(f, line)) {
		// strip CR
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (line.empty()) continue;

		// ---- header metadata lines ----------------------------------------
		if (line.rfind("#dynx ", 0) == 0) {
			r.dynx_version = line.substr(6);
		} else if (line.rfind("#source_hash ", 0) == 0) {
			r.source_hash = line.substr(13);
		} else if (line.rfind("#source ", 0) == 0) {
			r.source_path = line.substr(8);
		} else if (line.rfind("#kernel_version ", 0) == 0) {
			r.kernel_version = line.substr(16);
		} else if (line.rfind("#frame_count ", 0) == 0) {
			r.header_frame_count = std::stoll(line.substr(13));
		} else if (line.rfind("#frame_interval ", 0) == 0) {
			r.header_frame_interval = std::stod(line.substr(16));
		} else if (line.rfind("#particle_count ", 0) == 0) {
			r.header_particle_count = std::stoll(line.substr(16));
		} else if (line.rfind("#timestamp ", 0) == 0) {
			r.timestamp = line.substr(11);
		} else if (line == "#END_DYNX") {
			break;
		} else if (line.rfind('#', 0) == 0) {
			// other comment — skip
		}
		// ---- frame markers -------------------------------------------------
		else if (line.rfind("FRAME ", 0) == 0) {
			r.frames.emplace_back();
			cur = &r.frames.back();
			++r.actual_frame_count;
			std::istringstream ss(line.substr(6));
			ss >> cur->frame_index >> cur->time_fs;
		} else if (line == "END_FRAME") {
			cur = nullptr;
		}
		// ---- optional record lines -----------------------------------------
		else if (cur) {
			if (line.rfind("FORCE ", 0) == 0) {
				++cur->force_lines;
				++r.total_force_records;
			} else if (line.rfind("BOND_FORCE ", 0) == 0) {
				++cur->bond_force_lines;
				++r.total_bond_force_records;
			} else if (line.rfind("FIELD ", 0) == 0) {
				++cur->field_lines;
				++r.total_field_records;
			} else if (line.rfind("EVENT ", 0) == 0) {
				++cur->event_lines;
				++r.total_event_records;
			} else if (line.rfind("RENDER ", 0) == 0) {
				++cur->render_lines;
				++r.total_render_records;
			} else if (line.rfind("CAMERA ", 0) == 0) {
				++cur->camera_lines;
				++r.total_camera_records;
			} else {
				// particle line: starts with a digit or element symbol
				++cur->particle_lines;
			}
		}
	}

	if (r.dynx_version.empty()) {
		r.error_msg = "missing #dynx header — not a valid .dynx file";
		return r;
	}

	r.valid = true;
	return r;
}

// ============================================================================
// dynx_inspect_string()  —  inspect from an in-memory string (for tests)
// ============================================================================

inline DynxInspectResult dynx_inspect_string(const std::string& content,
											  const std::string& label = "<string>") {
	DynxInspectResult r;
	r.path = label;

	std::istringstream src(content);
	std::string line;
	DynxFrameStats* cur = nullptr;

	while (std::getline(src, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;

		if (line.rfind("#dynx ", 0) == 0) {
			r.dynx_version = line.substr(6);
		} else if (line.rfind("#source_hash ", 0) == 0) {
			r.source_hash = line.substr(13);
		} else if (line.rfind("#source ", 0) == 0) {
			r.source_path = line.substr(8);
		} else if (line.rfind("#kernel_version ", 0) == 0) {
			r.kernel_version = line.substr(16);
		} else if (line.rfind("#frame_count ", 0) == 0) {
			r.header_frame_count = std::stoll(line.substr(13));
		} else if (line.rfind("#frame_interval ", 0) == 0) {
			r.header_frame_interval = std::stod(line.substr(16));
		} else if (line.rfind("#particle_count ", 0) == 0) {
			r.header_particle_count = std::stoll(line.substr(16));
		} else if (line.rfind("#timestamp ", 0) == 0) {
			r.timestamp = line.substr(11);
		} else if (line == "#END_DYNX") {
			break;
		} else if (line.rfind('#', 0) == 0) {
			// skip
		} else if (line.rfind("FRAME ", 0) == 0) {
			r.frames.emplace_back();
			cur = &r.frames.back();
			++r.actual_frame_count;
			std::istringstream ss(line.substr(6));
			ss >> cur->frame_index >> cur->time_fs;
		} else if (line == "END_FRAME") {
			cur = nullptr;
		} else if (cur) {
			if      (line.rfind("FORCE ",      0) == 0) { ++cur->force_lines;      ++r.total_force_records; }
			else if (line.rfind("BOND_FORCE ", 0) == 0) { ++cur->bond_force_lines; ++r.total_bond_force_records; }
			else if (line.rfind("FIELD ",      0) == 0) { ++cur->field_lines;      ++r.total_field_records; }
			else if (line.rfind("EVENT ",      0) == 0) { ++cur->event_lines;      ++r.total_event_records; }
			else if (line.rfind("RENDER ",     0) == 0) { ++cur->render_lines;     ++r.total_render_records; }
			else if (line.rfind("CAMERA ",     0) == 0) { ++cur->camera_lines;     ++r.total_camera_records; }
			else                                         { ++cur->particle_lines; }
		}
	}

	if (r.dynx_version.empty()) {
		r.error_msg = "missing #dynx header";
		return r;
	}

	r.valid = true;
	return r;
}

} // namespace io
} // namespace vsim
