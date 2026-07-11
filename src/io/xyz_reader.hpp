#pragma once
/**
 * xyz_reader.hpp -- Spec-compliant unified XYZ family reader
 * ===========================================================
 * VSEPR-SIM  |  WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * Implements the reader API described in docs/xyz_file_formats.tex §9:
 *
 *   XYZFrame  read_xyz  (path)   -- static geometry
 *   XYZFrame  read_xyza (path)   -- + charge/velocity/force/energy columns
 *   XYZData   read_xyzc (path)   -- + checkpoint header (restartable)
 *   XYZData   read_xyzf (path)   -- multi-frame trajectory
 *
 * Critical design rules (from spec §4.3):
 *   1. Each frame in .xyzf is independently parsed.
 *   2. Column layout is re-read from the comment line of EVERY frame.
 *      Caching the first frame's column layout globally is explicitly a bug.
 *   3. Property columns: charge(1), velocity(3), force(3), energy(1).
 *      Column order is fixed; omission from the right is allowed.
 *      Insufficient columns → zero-fill + XYZParseWarning::COLUMN_ZERO_FILL.
 */

#include "xyz_unified.hpp"
#include <istream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>

namespace vsepr {
namespace io {

// ============================================================================
// XYZReadError — hard parse failure
// ============================================================================

struct XYZReadError {
	enum Code {
		INCOMPLETE_FRAME,
		UNKNOWN_ELEMENT,
		BAD_COORDINATE,
		COLUMN_COUNT_MISMATCH_HARD,   // missing coordinate columns
		NO_CHECKPOINT_HEADER,
		IO_ERROR,
	} code;
	std::string message;
	int frame_index = -1;
	int line_number = -1;
};

// ============================================================================
// ParseContext — shared parse state for one reader session
// ============================================================================

struct ParseContext {
	std::vector<XYZParseWarning> warnings;
	std::vector<XYZReadError>    errors;
	int current_frame = 0;

	bool has_errors()   const { return !errors.empty(); }
	bool has_warnings() const { return !warnings.empty(); }
};

// ============================================================================
// Internal helpers
// ============================================================================

namespace detail {

// Trim leading/trailing whitespace
inline std::string trim(const std::string& s) {
	auto a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	auto b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Parse "properties=..." from comment line → vector of lowercase key names
// Spec: colon-separated list of keys
// Examples: properties="charge:velocity"
//           properties="charge:velocity:force:energy"
inline std::vector<std::string> parse_properties_declaration(const std::string& comment) {
	std::vector<std::string> props;
	auto pos = comment.find("properties=");
	if (pos == std::string::npos) return props;
	pos += 11;  // skip "properties="
	// Skip optional opening quote
	if (pos < comment.size() && comment[pos] == '"') ++pos;
	auto end = comment.find('"', pos);
	if (end == std::string::npos) end = comment.find(' ', pos);
	if (end == std::string::npos) end = comment.size();
	std::string list = comment.substr(pos, end - pos);
	// Split on ':'
	std::istringstream ss(list);
	std::string token;
	while (std::getline(ss, token, ':')) {
		for (auto& c : token) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (!token.empty()) props.push_back(token);
	}
	return props;
}

// Parse bounding-box / PBC from comment line (spec §5)
// Lattice="ax 0 0  0 ay 0  0 0 az" pbc="T T T"
inline std::optional<XYZBox> parse_box_from_comment(const std::string& comment) {
	auto lp = comment.find("Lattice=");
	if (lp == std::string::npos) return std::nullopt;
	lp += 8;
	if (lp < comment.size() && comment[lp] == '"') ++lp;
	auto le = comment.find('"', lp);
	if (le == std::string::npos) return std::nullopt;
	std::string lstr = comment.substr(lp, le - lp);
	std::istringstream ls(lstr);
	XYZBox box;
	double v[9] = {};
	for (int i = 0; i < 9 && (ls >> v[i]); ++i) {}
	box.lattice = {v[0],v[1],v[2], v[3],v[4],v[5], v[6],v[7],v[8]};
	box.ax = v[0]; box.ay = v[4]; box.az = v[8];

	auto pp = comment.find("pbc=");
	if (pp != std::string::npos) {
		pp += 4;
		if (pp < comment.size() && comment[pp] == '"') ++pp;
		for (int i = 0; i < 3 && pp < comment.size(); ++i) {
			while (pp < comment.size() && comment[pp] == ' ') ++pp;
			box.pbc[i] = (pp < comment.size() && (comment[pp] == 'T' || comment[pp] == 't'));
			++pp;
		}
	}
	return box;
}

// Parse one atom line for .xyz: symbol x y z
// Returns false on hard error
inline bool parse_atom_xyz(const std::string& line, AtomRecord& out,
							 XYZReadError& err, int fi, int li)
{
	std::istringstream ss(line);
	std::string sym;
	double x, y, z;
	if (!(ss >> sym >> x >> y >> z)) {
		err = {XYZReadError::BAD_COORDINATE,
			   "Cannot parse coordinates on line " + std::to_string(li), fi, li};
		return false;
	}
	int Z = symbol_to_Z(sym);
	if (Z == 0) {
		err = {XYZReadError::UNKNOWN_ELEMENT,
			   "Unknown element symbol '" + sym + "'", fi, li};
		return false;
	}
	out.Z = Z; out.symbol = sym;
	out.x = x; out.y = y; out.z = z;
	return true;
}

// Parse one atom line for .xyza, given the property column list
// Column order (fixed, spec §3): x y z [q] [vx vy vz] [fx fy fz] [e]
inline bool parse_atom_xyza(const std::string& line,
							  const std::vector<std::string>& props,
							  AtomRecord& out,
							  ParseContext& ctx, int fi, int li)
{
	std::istringstream ss(line);
	std::string sym;
	double x, y, z;
	if (!(ss >> sym >> x >> y >> z)) {
		XYZReadError err{XYZReadError::BAD_COORDINATE,
						 "Cannot parse coordinates on line " + std::to_string(li), fi, li};
		ctx.errors.push_back(err);
		return false;
	}
	int Z = symbol_to_Z(sym);
	if (Z == 0) {
		ctx.errors.push_back({XYZReadError::UNKNOWN_ELEMENT,
							   "Unknown element '" + sym + "'", fi, li});
		return false;
	}
	out.Z = Z; out.symbol = sym;
	out.x = x; out.y = y; out.z = z;

	// Read optional columns in declaration order
	auto read1 = [&](double& v) { return static_cast<bool>(ss >> v); };
	auto warn_zero = [&](const std::string& prop) {
		ctx.warnings.push_back({XYZWarnCode::COLUMN_ZERO_FILL, fi, li,
								 "zero-filled '" + prop + "' on line " + std::to_string(li)});
	};

	for (const auto& prop : props) {
		if (prop == "charge") {
			double q = 0.0;
			if (!read1(q)) warn_zero(prop);
			out.q = q;
		} else if (prop == "velocity") {
			XYZVec3 vel;
			if (!read1(vel.x) || !read1(vel.y) || !read1(vel.z)) {
				warn_zero(prop);
				vel = {};
			}
			out.v = vel;
		} else if (prop == "force") {
			XYZVec3 force;
			if (!read1(force.x) || !read1(force.y) || !read1(force.z)) {
				warn_zero(prop);
				force = {};
			}
			out.f = force;
		} else if (prop == "energy") {
			double e = 0.0;
			if (!read1(e)) warn_zero(prop);
			out.e = e;
		} else {
			// Unknown property — consume no columns, emit warning
			ctx.warnings.push_back({XYZWarnCode::UNKNOWN_PROPERTY, fi, li,
									 "Unknown property key '" + prop + "'"});
		}
	}
	return true;
}

// Parse one complete frame from stream (base: .xyz)
// On entry the stream must be positioned at the atom-count line.
inline bool parse_frame_base(std::istream& in, XYZFrame& frame,
							   ParseContext& ctx, int fi, int& lineno)
{
	std::string line;
	// Line 1: atom count
	if (!std::getline(in, line)) return false;
	line = trim(line);
	if (line.empty()) {
		// Skip blank lines between frames (graceful)
		while (std::getline(in, line)) { ++lineno; line = trim(line); if (!line.empty()) break; }
	}
	try { frame.N = std::stoi(line); }
	catch (...) { return false; }
	++lineno;

	// Line 2: comment
	if (!std::getline(in, frame.comment)) {
		ctx.errors.push_back({XYZReadError::INCOMPLETE_FRAME,
							   "Missing comment line in frame " + std::to_string(fi), fi, lineno});
		return false;
	}
	++lineno;

	// Parse comment-line metadata
	frame.energy      = parse_comment_energy(frame.comment);
	frame.temperature = parse_comment_temperature(frame.comment);
	frame.box         = parse_box_from_comment(frame.comment);
	frame.frame_index = fi;

	// Lines 3..N+2: atom records (.xyz columns only)
	frame.atoms.reserve(frame.N);
	for (int i = 0; i < frame.N; ++i) {
		if (!std::getline(in, line)) {
			ctx.errors.push_back({XYZReadError::INCOMPLETE_FRAME,
								   "Frame " + std::to_string(fi) + " has fewer than N=" +
								   std::to_string(frame.N) + " atom lines", fi, lineno});
			return false;
		}
		++lineno;
		line = trim(line);
		AtomRecord a;
		XYZReadError err;
		if (!parse_atom_xyz(line, a, err, fi, lineno)) {
			ctx.errors.push_back(err);
			return false;
		}
		frame.atoms.push_back(std::move(a));
	}
	return true;
}

// Parse one complete .xyza frame (per-frame independent property parsing)
inline bool parse_frame_xyza(std::istream& in, XYZFrame& frame,
							   ParseContext& ctx, int fi, int& lineno)
{
	std::string line;
	if (!std::getline(in, line)) return false;
	line = trim(line);
	if (line.empty()) {
		while (std::getline(in, line)) { ++lineno; line = trim(line); if (!line.empty()) break; }
	}
	try { frame.N = std::stoi(line); }
	catch (...) { return false; }
	++lineno;

	if (!std::getline(in, frame.comment)) {
		ctx.errors.push_back({XYZReadError::INCOMPLETE_FRAME,
							   "Missing comment line in frame " + std::to_string(fi), fi, lineno});
		return false;
	}
	++lineno;

	// Per-frame independent property parsing (spec §4.3 rule 2)
	auto props = parse_properties_declaration(frame.comment);
	frame.has_charge     = std::find(props.begin(), props.end(), "charge")   != props.end();
	frame.has_velocity   = std::find(props.begin(), props.end(), "velocity") != props.end();
	frame.has_force      = std::find(props.begin(), props.end(), "force")    != props.end();
	frame.has_energy_col = std::find(props.begin(), props.end(), "energy")   != props.end();

	frame.energy      = parse_comment_energy(frame.comment);
	frame.temperature = parse_comment_temperature(frame.comment);
	frame.box         = parse_box_from_comment(frame.comment);
	frame.frame_index = fi;

	frame.atoms.reserve(frame.N);
	for (int i = 0; i < frame.N; ++i) {
		if (!std::getline(in, line)) {
			ctx.errors.push_back({XYZReadError::INCOMPLETE_FRAME,
								   "Incomplete atom block in frame " + std::to_string(fi),
								   fi, lineno});
			return false;
		}
		++lineno;
		line = trim(line);
		AtomRecord a;
		if (!parse_atom_xyza(line, props, a, ctx, fi, lineno)) return false;
		frame.atoms.push_back(std::move(a));
	}
	return true;
}

// Parse CHECKPOINT block (spec §4)
inline bool parse_checkpoint_header(std::istream& in, CheckpointState& ck,
									 ParseContext& ctx, int& lineno)
{
	std::string line;
	while (std::getline(in, line)) {
		++lineno;
		line = detail::trim(line);
		if (line == "END_CHECKPOINT") return true;
		std::istringstream ss(line);
		std::string key;
		if (!(ss >> key)) continue;
		if (key == "step")     { ss >> ck.step; }
		else if (key == "time")     { ss >> ck.time; }
		else if (key == "dt")       { ss >> ck.dt; }
		else if (key == "T_target") { ss >> ck.T_target; }
		else if (key == "seed")     { ss >> ck.seed; }
		else if (key == "box") {
			XYZBox b;
			ss >> b.ax >> b.ay >> b.az;
			ck.box = b;
		}
	}
	ctx.errors.push_back({XYZReadError::NO_CHECKPOINT_HEADER,
						   "Missing END_CHECKPOINT marker", -1, lineno});
	return false;
}

} // namespace detail

// ============================================================================
// Free-function readers (spec §3 API)
// ============================================================================

/**
 * Read a .xyz file (static geometry).
 * Returns the first frame; ignores additional frames if present.
 */
inline XYZFrame read_xyz(const std::string& path, ParseContext* ctx_out = nullptr) {
	ParseContext ctx;
	std::ifstream in(path);
	if (!in) {
		ctx.errors.push_back({XYZReadError::IO_ERROR, "Cannot open: " + path});
		if (ctx_out) *ctx_out = std::move(ctx);
		return {};
	}
	XYZFrame frame;
	int lineno = 0;
	detail::parse_frame_base(in, frame, ctx, 0, lineno);
	if (ctx_out) *ctx_out = std::move(ctx);
	return frame;
}

/**
 * Read a .xyza file (extended: charge, velocity, force, energy columns).
 * Per-frame property parsing; no cached column layout.
 */
inline XYZFrame read_xyza(const std::string& path, ParseContext* ctx_out = nullptr) {
	ParseContext ctx;
	std::ifstream in(path);
	if (!in) {
		ctx.errors.push_back({XYZReadError::IO_ERROR, "Cannot open: " + path});
		if (ctx_out) *ctx_out = std::move(ctx);
		return {};
	}
	XYZFrame frame;
	int lineno = 0;
	detail::parse_frame_xyza(in, frame, ctx, 0, lineno);
	if (ctx_out) *ctx_out = std::move(ctx);
	return frame;
}

/**
 * Read a .xyzc file (checkpoint + single .xyza frame).
 * Returns XYZData with checkpoint field populated.
 */
inline XYZData read_xyzc(const std::string& path, ParseContext* ctx_out = nullptr) {
	ParseContext ctx;
	XYZData data;
	std::ifstream in(path);
	if (!in) {
		ctx.errors.push_back({XYZReadError::IO_ERROR, "Cannot open: " + path});
		if (ctx_out) *ctx_out = std::move(ctx);
		return data;
	}

	// Peek at first line to detect CHECKPOINT block
	std::string first;
	int lineno = 0;
	if (!std::getline(in, first)) {
		if (ctx_out) *ctx_out = std::move(ctx);
		return data;
	}
	++lineno;
	first = detail::trim(first);

	CheckpointState ck;
	if (first == "CHECKPOINT") {
		if (!detail::parse_checkpoint_header(in, ck, ctx, lineno)) {
			if (ctx_out) *ctx_out = std::move(ctx);
			return data;
		}
		data.checkpoint = ck;
	} else {
		// No CHECKPOINT block — treat as plain .xyza, rewind via unget trick
		// We push the consumed line back by prepending to stream via sstream
		ctx.errors.push_back({XYZReadError::NO_CHECKPOINT_HEADER,
							   "Expected CHECKPOINT on line 1 of .xyzc file"});
		if (ctx_out) *ctx_out = std::move(ctx);
		return data;
	}

	// Atom block is .xyza
	XYZFrame frame;
	detail::parse_frame_xyza(in, frame, ctx, 0, lineno);
	data.frames.push_back(std::move(frame));

	if (ctx_out) *ctx_out = std::move(ctx);
	return data;
}

/**
 * Read a .xyzf file (multi-frame trajectory).
 * Each frame is independently parsed per spec §4.3.
 */
inline XYZData read_xyzf(const std::string& path, ParseContext* ctx_out = nullptr) {
	ParseContext ctx;
	XYZData data;
	std::ifstream in(path);
	if (!in) {
		ctx.errors.push_back({XYZReadError::IO_ERROR, "Cannot open: " + path});
		if (ctx_out) *ctx_out = std::move(ctx);
		return data;
	}

	int lineno = 0;
	int fi = 0;
	while (in.peek() != EOF) {
		// Skip blank lines between frames
		std::string peek_line;
		auto save_pos = in.tellg();
		if (!std::getline(in, peek_line)) break;
		++lineno;
		peek_line = detail::trim(peek_line);
		if (peek_line.empty()) continue;

		// Try to parse as atom count (start of new frame)
		int n_test = 0;
		try { n_test = std::stoi(peek_line); }
		catch (...) { break; }

		// Re-inject by seeking back and re-reading inside parse_frame_xyza
		// We do this by constructing a temporary stream from the remainder
		// Strategy: seek back to save_pos, then use parse_frame_xyza on the
		// main stream — it will re-read the atom count line.
		in.seekg(save_pos);
		--lineno;  // undo the getline we peeked with

		XYZFrame frame;
		// Per-frame independent parse: read_xyza if any properties= present,
		// else read_xyz columns. We always call xyza parser which falls back
		// gracefully to xyz-only when props is empty.
		if (!detail::parse_frame_xyza(in, frame, ctx, fi, lineno)) {
			// Soft: log error and continue to next frame
			if (ctx.has_errors()) break;
		}
		data.frames.push_back(std::move(frame));
		++fi;
		(void)n_test;
	}

	if (ctx_out) *ctx_out = std::move(ctx);
	return data;
}

// ============================================================================
// XYZFReader — streaming iterator for .xyzf (spec §9 reader API)
// ============================================================================

class XYZFReader {
public:
	explicit XYZFReader(const std::string& path)
		: in_(path), lineno_(0), fi_(0)
	{
		if (!in_) ctx_.errors.push_back({XYZReadError::IO_ERROR, "Cannot open: " + path});
	}

	// Returns true and fills frame if a frame is available; false at EOF or error
	bool next(XYZFrame& frame) {
		if (!in_ || in_.peek() == EOF) return false;

		// Skip blank lines
		std::string line;
		while (in_.peek() != EOF) {
			auto pos = in_.tellg();
			if (!std::getline(in_, line)) return false;
			++lineno_;
			if (!detail::trim(line).empty()) {
				in_.seekg(pos);
				--lineno_;
				break;
			}
		}
		if (in_.peek() == EOF) return false;

		if (!detail::parse_frame_xyza(in_, frame, ctx_, fi_, lineno_)) return false;
		++fi_;
		return true;
	}

	bool has_errors()   const { return ctx_.has_errors(); }
	const ParseContext& context() const { return ctx_; }

private:
	std::ifstream in_;
	ParseContext  ctx_;
	int lineno_, fi_;
};

} // namespace io
} // namespace vsepr
