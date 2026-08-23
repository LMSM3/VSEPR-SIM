/**
 * src/vsim/io/dynx_writer.cpp
 * ==============================
 * WO-VSIM-DYNX-V1-A / WO-VSIM-DYNX-V1-B  |  Phase 9  |  v5.1.x
 *
 * Implements DynxWriter, dynx_inspect(), and dynx_validate().
 */

#include "vsim/io/dynx_writer.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>

namespace vsim {
namespace io {

// ============================================================================
// Internal helpers
// ============================================================================

static std::string utc_iso8601() {
	auto now = std::chrono::system_clock::now();
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
	char buf[32] = {};
	std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&tt));
	return buf;
}

// ============================================================================
// DynxWriter::open
// ============================================================================

bool DynxWriter::open(const std::string& path, DynxHeader hdr) {
	if (is_open_) close();
	path_  = path;
	error_.clear();

	FILE* f = std::fopen(path.c_str(), "w+b");
	if (!f) {
		error_ = "cannot open '" + path + "' for writing";
		return false;
	}
	fp_            = f;
	is_open_       = true;
	frames_written_ = 0;
	hdr_           = hdr;
	if (hdr_.timestamp.empty())
		hdr_.timestamp = utc_iso8601();

	// Write header comment block  ----------------------------------------
	//   frame_count is unknown yet; write a placeholder that close() patches.
	std::fprintf(f, "#dynx v1\n");
	std::fprintf(f, "#source %s\n",           hdr_.source_path.empty()
												? "(unknown)" : hdr_.source_path.c_str());
	std::fprintf(f, "#source_hash %s\n",      hdr_.source_hash.c_str());
	std::fprintf(f, "#kernel_version %s\n",   hdr_.kernel_version.c_str());
	// Patch target: exactly 20 chars for the int (space-padded)
	std::fprintf(f, "#frame_count          %20d\n", 0);
	std::fprintf(f, "#frame_interval %.6f\n", hdr_.frame_interval_fs);
	std::fprintf(f, "#particle_count %d\n",   hdr_.particle_count);
	std::fprintf(f, "#timestamp %s\n",        hdr_.timestamp.c_str());

	return true;
}

// ============================================================================
// DynxWriter::write_frame
// ============================================================================

bool DynxWriter::write_frame(const DynxFrame& frame) {
	if (!is_open_) {
		error_ = "writer is not open";
		return false;
	}
	FILE* f = static_cast<FILE*>(fp_);

	std::fprintf(f, "FRAME %d %.6f\n", frame.index, frame.time_fs);
	for (const auto& p : frame.particles) {
		if (p.has_vel && p.has_energy) {
			std::fprintf(f, "%s %.6f %.6f %.6f  %.6f %.6f %.6f  %.6f\n",
				p.symbol.c_str(),
				p.pos[0], p.pos[1], p.pos[2],
				p.vel[0], p.vel[1], p.vel[2],
				p.energy);
		} else if (p.has_vel) {
			std::fprintf(f, "%s %.6f %.6f %.6f  %.6f %.6f %.6f\n",
				p.symbol.c_str(),
				p.pos[0], p.pos[1], p.pos[2],
				p.vel[0], p.vel[1], p.vel[2]);
		} else {
			std::fprintf(f, "%s %.6f %.6f %.6f\n",
				p.symbol.c_str(),
				p.pos[0], p.pos[1], p.pos[2]);
		}
	}
	std::fprintf(f, "END_FRAME\n");
	++frames_written_;
	return true;
}

// ============================================================================
// DynxWriter::write_rich_frame  (WO-72B)
// ============================================================================

bool DynxWriter::write_rich_frame(const DynxRichFrame& frame) {
	if (!is_open_) {
		error_ = "writer is not open";
		return false;
	}
	FILE* f = static_cast<FILE*>(fp_);

	std::fprintf(f, "FRAME %d %.6f\n", frame.index, frame.time_fs);

	// Particle lines (same as write_frame)
	for (const auto& p : frame.particles) {
		if (p.has_vel && p.has_energy) {
			std::fprintf(f, "%s %.6f %.6f %.6f  %.6f %.6f %.6f  %.6f\n",
				p.symbol.c_str(),
				p.pos[0], p.pos[1], p.pos[2],
				p.vel[0], p.vel[1], p.vel[2],
				p.energy);
		} else if (p.has_vel) {
			std::fprintf(f, "%s %.6f %.6f %.6f  %.6f %.6f %.6f\n",
				p.symbol.c_str(),
				p.pos[0], p.pos[1], p.pos[2],
				p.vel[0], p.vel[1], p.vel[2]);
		} else {
			std::fprintf(f, "%s %.6f %.6f %.6f\n",
				p.symbol.c_str(),
				p.pos[0], p.pos[1], p.pos[2]);
		}
	}

	// FORCE lines
	for (const auto& fs : frame.forces) {
		std::fprintf(f, "FORCE %d %.6f %.6f %.6f\n",
			fs.particle_idx,
			fs.force[0], fs.force[1], fs.force[2]);
	}

	// BOND_FORCE lines
	for (const auto& bf : frame.bond_forces) {
		std::fprintf(f, "BOND_FORCE %d %d %.6f %.6f %.6f\n",
			bf.i, bf.j,
			bf.force[0], bf.force[1], bf.force[2]);
	}

	// FIELD lines
	for (const auto& fv : frame.field_vectors) {
		std::fprintf(f, "FIELD %s %.6f %.6f %.6f\n",
			fv.label.c_str(),
			fv.vec[0], fv.vec[1], fv.vec[2]);
	}

	// EVENT lines
	for (const auto& ev : frame.events) {
		std::fprintf(f, "EVENT %s %llu %s %.6f\n",
			ev.kind.c_str(),
			static_cast<unsigned long long>(ev.event_id),
			ev.source.c_str(),
			ev.value);
	}

	// RENDER lines
	for (const auto& rm : frame.render) {
		std::fprintf(f, "RENDER %d %u %u %u %s %d\n",
			rm.particle_idx,
			static_cast<unsigned>(rm.r),
			static_cast<unsigned>(rm.g),
			static_cast<unsigned>(rm.b),
			rm.tag.c_str(),
			rm.visible ? 1 : 0);
	}

	// CAMERA lines
	for (const auto& cam : frame.cameras) {
		std::fprintf(f, "CAMERA %s %.6f %.6f %.6f %.6f %.6f %.6f\n",
			cam.label.c_str(),
			cam.x, cam.y, cam.z,
			cam.pitch, cam.yaw, cam.zoom);
	}

	std::fprintf(f, "END_FRAME\n");
	++frames_written_;
	return true;
}

// ============================================================================
// DynxWriter — streaming API (WO-72D)
// ============================================================================

bool DynxWriter::begin_frame(std::size_t frame_id, double t, double dt) {
	if (!is_open_) { error_ = "writer is not open"; return false; }
	// Auto-close any frame that wasn't explicitly ended
	if (frame_open_) end_frame();
	FILE* f = static_cast<FILE*>(fp_);
	if (dt != 0.0)
		std::fprintf(f, "# dt=%.6f\n", dt);
	std::fprintf(f, "FRAME %zu %.6f\n", frame_id, t);
	frame_open_ = true;
	return true;
}

bool DynxWriter::write_particle(std::size_t idx, double x, double y, double z) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	FILE* f = static_cast<FILE*>(fp_);
	// idx is written as the integer index; symbol placeholder "_"
	// Callers that have a symbol should use write_rich_frame / write_frame instead.
	// Here we emit a positional record indexed by idx.
	std::fprintf(f, "%zu %.6f %.6f %.6f\n", idx, x, y, z);
	return true;
}

bool DynxWriter::write_force(std::size_t idx, double fx, double fy, double fz) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	std::fprintf(static_cast<FILE*>(fp_),
		"FORCE %zu %.6f %.6f %.6f\n", idx, fx, fy, fz);
	return true;
}

bool DynxWriter::write_bond_force(std::size_t i, std::size_t j,
								  double bfx, double bfy, double bfz) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	std::fprintf(static_cast<FILE*>(fp_),
		"BOND_FORCE %zu %zu %.6f %.6f %.6f\n", i, j, bfx, bfy, bfz);
	return true;
}

bool DynxWriter::write_field(const std::string& label,
							 double fx, double fy, double fz) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	std::fprintf(static_cast<FILE*>(fp_),
		"FIELD %s %.6f %.6f %.6f\n", label.c_str(), fx, fy, fz);
	return true;
}

bool DynxWriter::write_event(const std::string& kind, int event_id,
							 const std::string& source, double value) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	std::fprintf(static_cast<FILE*>(fp_),
		"EVENT %s %d %s %.6f\n",
		kind.c_str(), event_id, source.c_str(), value);
	return true;
}

bool DynxWriter::write_render(std::size_t idx, int r, int g, int b,
							  const std::string& tag, bool visible) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	std::fprintf(static_cast<FILE*>(fp_),
		"RENDER %zu %d %d %d %s %d\n",
		idx, r, g, b, tag.c_str(), visible ? 1 : 0);
	return true;
}

bool DynxWriter::write_camera(const std::string& label,
							  double x, double y, double z,
							  double pitch, double yaw, double zoom) {
	if (!is_open_ || !frame_open_) { error_ = "no open frame"; return false; }
	std::fprintf(static_cast<FILE*>(fp_),
		"CAMERA %s %.6f %.6f %.6f %.6f %.6f %.6f\n",
		label.c_str(), x, y, z, pitch, yaw, zoom);
	return true;
}

bool DynxWriter::end_frame() {
	if (!is_open_ || !frame_open_) return true;  // no-op
	std::fprintf(static_cast<FILE*>(fp_), "END_FRAME\n");
	++frames_written_;
	frame_open_ = false;
	return true;
}

// ============================================================================
// DynxWriter::close
// ============================================================================

bool DynxWriter::close() {
	if (!is_open_) return true;

	if (frame_open_) end_frame();  // flush any in-progress streaming frame

	FILE* f = static_cast<FILE*>(fp_);

	std::fprintf(f, "#END_DYNX\n");

	// Patch frame_count in the header.
	// Read entire file content, patch the placeholder, close, then rewrite.
	std::fflush(f);
	std::rewind(f);

	std::string content;
	char ch;
	while ((ch = static_cast<char>(std::fgetc(f))) != EOF)
		content += ch;

	std::fclose(f);
	fp_      = nullptr;
	is_open_ = false;
	hdr_.frame_count = frames_written_;

	// Replace the placeholder
	const std::string marker = "#frame_count          ";
	auto pos = content.find(marker);
	if (pos != std::string::npos) {
		char newval[21] = {};
		std::snprintf(newval, sizeof(newval), "%20d", frames_written_);
		content.replace(pos + marker.size(), 20, newval, 20);
	}

	// Rewrite file with patched content
	FILE* fw = std::fopen(path_.c_str(), "w");
	if (fw) {
		std::fwrite(content.data(), 1, content.size(), fw);
		std::fclose(fw);
	}

	return true;
}

// ============================================================================
// dynx_inspect
// ============================================================================

DynxInspectResult dynx_inspect(const std::string& path) {
	DynxInspectResult r;
	FILE* f = std::fopen(path.c_str(), "r");
	if (!f) {
		r.error = "cannot open '" + path + "'";
		return r;
	}

	char line[512];
	bool saw_dynx = false;
	while (std::fgets(line, sizeof(line), f)) {
		std::string s = line;
		// Strip trailing newline
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();

		if (s.rfind("#dynx", 0) == 0) { saw_dynx = true; continue; }
		if (s.rfind("#source_hash ", 0) == 0) {
			r.source_hash = s.substr(13);
			continue;
		}
		if (s.rfind("#source ", 0) == 0) {
			r.source_path = s.substr(8);
			continue;
		}
		if (s.rfind("#kernel_version ", 0) == 0) {
			r.kernel_version = s.substr(16);
			continue;
		}
		if (s.rfind("#frame_count", 0) == 0) {
			try { r.frame_count = std::stoi(s.substr(12)); } catch (...) {}
			continue;
		}
		if (s.rfind("#frame_interval ", 0) == 0) {
			try { r.frame_interval_fs = std::stod(s.substr(16)); } catch (...) {}
			continue;
		}
		if (s.rfind("#particle_count ", 0) == 0) {
			try { r.particle_count = std::stoi(s.substr(16)); } catch (...) {}
			continue;
		}
		if (s.rfind("#timestamp ", 0) == 0) {
			r.timestamp = s.substr(11);
			continue;
		}
		// Stop at first non-comment line
		if (!s.empty() && s[0] != '#') break;
	}
	std::fclose(f);

	if (!saw_dynx) {
		r.error = "not a dynx file (missing '#dynx v1' header)";
		return r;
	}
	r.ok = true;
	return r;
}

// ============================================================================
// dynx_validate
// ============================================================================

DynxValidateResult dynx_validate(const std::string& path) {
	DynxValidateResult r;
	FILE* f = std::fopen(path.c_str(), "r");
	if (!f) {
		r.error = "cannot open '" + path + "'";
		return r;
	}

	// First pass: inspect header
	DynxInspectResult hdr = dynx_inspect(path);
	if (!hdr.ok) {
		r.error = hdr.error;
		std::fclose(f);
		return r;
	}
	r.hash_present   = (hdr.source_hash != "none" && !hdr.source_hash.empty());
	r.particle_count = hdr.particle_count;

	if (!r.hash_present)
		r.warnings.push_back("source_hash is 'none' — provenance unverified");

	// Second pass: walk frames
	char line[512];
	int  frame_count   = 0;
	double prev_time   = -1e30;
	bool   mono_ok     = true;
	int    in_frame    = 0;
	int    particles_this_frame = 0;

	// Rich-line prefixes that are NOT particle records.
	// Any in-frame line starting with one of these is skipped for particle counting.
	static const char* const k_rich_prefixes[] = {
		"FORCE ", "BOND_FORCE ", "FIELD ", "EVENT ", "RENDER ", "CAMERA "
	};
	auto is_rich_line = [](const std::string& s) -> bool {
		for (auto* pfx : k_rich_prefixes)
			if (s.rfind(pfx, 0) == 0) return true;
		return false;
	};

	std::rewind(f);
	while (std::fgets(line, sizeof(line), f)) {
		std::string s = line;
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();

		if (s.rfind("FRAME ", 0) == 0) {
			// Parse "FRAME <idx> <time_fs>"
			double t = 0.0;
			int    idx = 0;
			if (std::sscanf(s.c_str(), "FRAME %d %lf", &idx, &t) == 2) {
				if (t <= prev_time && frame_count > 0) mono_ok = false;
				prev_time = t;
				++frame_count;
				in_frame = 1;
				particles_this_frame = 0;
			}
			continue;
		}
		if (s == "END_FRAME") {
			if (hdr.particle_count > 0 && particles_this_frame != hdr.particle_count) {
				r.warnings.push_back(
					"frame " + std::to_string(frame_count) +
					": particle count mismatch (header=" +
					std::to_string(hdr.particle_count) +
					" actual=" + std::to_string(particles_this_frame) + ")");
			}
			in_frame = 0;
			continue;
		}
		if (in_frame && !s.empty() && s[0] != '#' && !is_rich_line(s)) {
			++particles_this_frame;
		}
	}
	std::fclose(f);

	r.frame_count   = frame_count;
	r.monotonic_time = mono_ok;

	if (!mono_ok)
		r.warnings.push_back("frame times are not strictly monotonically increasing");

	if (hdr.frame_count > 0 && frame_count != hdr.frame_count) {
		r.warnings.push_back(
			"header frame_count=" + std::to_string(hdr.frame_count) +
			" but found " + std::to_string(frame_count) + " FRAME blocks");
	}

	if (frame_count == 0) {
		r.error = "no frames found";
		return r;
	}

	r.ok = r.warnings.empty() || r.error.empty();
	if (!r.ok && r.error.empty()) r.ok = true; // warnings don't fail validation
	return r;
}

} // namespace io
} // namespace vsim
