/**
 * src/vsim/io/dynx_reader.cpp
 * =============================
 * WO-72D  |  Phase 9  |  v5.1.x
 *
 * Implements DynxReader.
 */

#include "vsim/io/dynx_reader.hpp"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace vsim {
namespace io {

// ============================================================================
// Internal helpers
// ============================================================================

static void strip_newline(std::string& s) {
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
		s.pop_back();
}

// Parse the header comment block from the current file position.
// Stops when the first non-comment, non-blank line is encountered.
// That line is NOT consumed; caller must re-read it.
static bool parse_header(FILE* f, DynxHeader& hdr, std::string& error) {
	char buf[1024];
	bool saw_dynx = false;
	long body_start = 0;

	while (true) {
		body_start = std::ftell(f);
		if (!std::fgets(buf, sizeof(buf), f)) break;
		std::string s = buf;
		strip_newline(s);

		if (s.rfind("#dynx", 0) == 0)          { saw_dynx = true; continue; }
		if (s.rfind("#source_hash ", 0) == 0)   { hdr.source_hash    = s.substr(13); continue; }
		if (s.rfind("#source ", 0) == 0)        { hdr.source_path    = s.substr(8);  continue; }
		if (s.rfind("#kernel_version ", 0) == 0){ hdr.kernel_version = s.substr(16); continue; }
		if (s.rfind("#frame_count", 0) == 0) {
			try { hdr.frame_count = std::stoi(s.substr(12)); } catch (...) {}
			continue;
		}
		if (s.rfind("#frame_interval ", 0) == 0) {
			try { hdr.frame_interval_fs = std::stod(s.substr(16)); } catch (...) {}
			continue;
		}
		if (s.rfind("#particle_count ", 0) == 0) {
			try { hdr.particle_count = std::stoi(s.substr(16)); } catch (...) {}
			continue;
		}
		if (s.rfind("#timestamp ", 0) == 0) { hdr.timestamp = s.substr(11); continue; }
		if (!s.empty() && s[0] == '#')      { continue; }  // other comment
		if (s.empty())                       { continue; }  // blank line

		// First non-comment, non-blank line — rewind to it
		std::fseek(f, body_start, SEEK_SET);
		break;
	}
	if (!saw_dynx) {
		error = "not a dynx file (missing '#dynx v1' header)";
		return false;
	}
	return true;
}

// ============================================================================
// DynxReader::open
// ============================================================================

bool DynxReader::open(const std::string& path) {
	close();
	path_  = path;
	error_.clear();
	frames_read_ = 0;

	FILE* f = std::fopen(path.c_str(), "r");
	if (!f) {
		error_ = "cannot open '" + path + "'";
		return false;
	}
	hdr_ = DynxHeader{};
	if (!parse_header(f, hdr_, error_)) {
		std::fclose(f);
		return false;
	}
	fp_ = f;
	return true;
}

// ============================================================================
// DynxReader::close
// ============================================================================

void DynxReader::close() {
	if (fp_) {
		std::fclose(static_cast<FILE*>(fp_));
		fp_ = nullptr;
	}
}

// ============================================================================
// DynxReader::parse_in_frame_line
// ============================================================================

bool DynxReader::parse_in_frame_line(const std::string& s, DynxRichFrame& frame) {
	// FORCE <idx> <fx> <fy> <fz>
	if (s.rfind("FORCE ", 0) == 0) {
		DynxForceState fs;
		if (std::sscanf(s.c_str(), "FORCE %d %lf %lf %lf",
						&fs.particle_idx,
						&fs.force[0], &fs.force[1], &fs.force[2]) == 4)
			frame.forces.push_back(fs);
		return true;
	}
	// BOND_FORCE <i> <j> <bfx> <bfy> <bfz>
	if (s.rfind("BOND_FORCE ", 0) == 0) {
		DynxBondForce bf;
		if (std::sscanf(s.c_str(), "BOND_FORCE %d %d %lf %lf %lf",
						&bf.i, &bf.j,
						&bf.force[0], &bf.force[1], &bf.force[2]) == 5)
			frame.bond_forces.push_back(bf);
		return true;
	}
	// FIELD <label> <fx> <fy> <fz>
	if (s.rfind("FIELD ", 0) == 0) {
		char label[256] = {};
		DynxFieldVector fv;
		if (std::sscanf(s.c_str(), "FIELD %255s %lf %lf %lf",
						label, &fv.vec[0], &fv.vec[1], &fv.vec[2]) == 4) {
			fv.label = label;
			frame.field_vectors.push_back(fv);
		}
		return true;
	}
	// EVENT <kind> <event_id> <source> <value>
	if (s.rfind("EVENT ", 0) == 0) {
		char kind[128] = {}, src[256] = {};
		DynxEventPacket ev;
		unsigned long long eid = 0;
		if (std::sscanf(s.c_str(), "EVENT %127s %llu %255s %lf",
						kind, &eid, src, &ev.value) == 4) {
			ev.kind     = kind;
			ev.event_id = static_cast<uint64_t>(eid);
			ev.source   = src;
			frame.events.push_back(ev);
		}
		return true;
	}
	// RENDER <idx> <r> <g> <b> <tag> <visible>
	if (s.rfind("RENDER ", 0) == 0) {
		DynxRenderMeta rm;
		char tag[256] = {};
		unsigned ur = 255, ug = 255, ub = 255;
		int vis = 1;
		if (std::sscanf(s.c_str(), "RENDER %d %u %u %u %255s %d",
						&rm.particle_idx, &ur, &ug, &ub, tag, &vis) == 6) {
			rm.r       = static_cast<uint8_t>(ur);
			rm.g       = static_cast<uint8_t>(ug);
			rm.b       = static_cast<uint8_t>(ub);
			rm.tag     = tag;
			rm.visible = (vis != 0);
			frame.render.push_back(rm);
		}
		return true;
	}
	// CAMERA <label> <x> <y> <z> <pitch> <yaw> <zoom>
	if (s.rfind("CAMERA ", 0) == 0) {
		char label[256] = {};
		DynxCameraState cam;
		if (std::sscanf(s.c_str(), "CAMERA %255s %lf %lf %lf %lf %lf %lf",
						label,
						&cam.x, &cam.y, &cam.z,
						&cam.pitch, &cam.yaw, &cam.zoom) == 7) {
			cam.label = label;
			frame.cameras.push_back(cam);
		}
		return true;
	}

	// Particle record — leading token is either a symbol (alpha) or an index (digit).
	// Format (symbol path):  <sym> <x> <y> <z> [<vx> <vy> <vz>] [<energy>]
	// Format (index path):   <idx> <x> <y> <z>
	DynxParticleState ps;
	if (!s.empty() && (std::isalpha(static_cast<unsigned char>(s[0])) ||
					   std::isdigit(static_cast<unsigned char>(s[0])))) {
		char tok[64] = {};
		double a, b, c, d = 0, e = 0, g = 0, h = 0;
		int n = std::sscanf(s.c_str(), "%63s %lf %lf %lf %lf %lf %lf %lf",
							tok, &a, &b, &c, &d, &e, &g, &h);
		if (n >= 4) {
			// If tok is purely numeric treat as positional index with no symbol
			bool is_idx = true;
			for (int i = 0; tok[i]; ++i)
				if (!std::isdigit(static_cast<unsigned char>(tok[i]))) { is_idx = false; break; }
			ps.symbol    = is_idx ? "_" : tok;
			ps.pos[0]    = a; ps.pos[1] = b; ps.pos[2] = c;
			if (n >= 7) { ps.vel[0] = d; ps.vel[1] = e; ps.vel[2] = g; ps.has_vel = true; }
			if (n == 8) { ps.energy = h; ps.has_energy = true; }
			frame.particles.push_back(ps);
			return true;
		}
	}
	return false;  // unrecognised / comment — silently skip
}

// ============================================================================
// DynxReader::read_next_frame
// ============================================================================

bool DynxReader::read_next_frame(DynxRichFrame& out) {
	if (!fp_) { error_ = "reader is not open"; return false; }
	FILE* f = static_cast<FILE*>(fp_);

	out = DynxRichFrame{};
	char buf[1024];
	bool in_frame = false;

	while (std::fgets(buf, sizeof(buf), f)) {
		std::string s = buf;
		strip_newline(s);

		if (s.empty() || s[0] == '#') continue;

		if (s.rfind("FRAME ", 0) == 0) {
			int    idx = 0;
			double t   = 0.0;
			std::sscanf(s.c_str(), "FRAME %d %lf", &idx, &t);
			out.index   = idx;
			out.time_fs = t;
			in_frame = true;
			continue;
		}
		if (s == "END_FRAME") {
			if (in_frame) { ++frames_read_; return true; }
			continue;
		}
		if (s == "#END_DYNX") break;
		if (in_frame) parse_in_frame_line(s, out);
	}
	// EOF without END_FRAME — partial frame or clean end
	if (in_frame && !out.particles.empty()) {
		++frames_read_;
		return true;
	}
	return false;
}

// ============================================================================
// DynxReader::read_all_frames
// ============================================================================

bool DynxReader::read_all_frames(std::vector<DynxRichFrame>& out) {
	out.clear();
	DynxRichFrame frame;
	while (read_next_frame(frame))
		out.push_back(std::move(frame));
	return !out.empty();
}

} // namespace io
} // namespace vsim
