/**
 * src/batch/manifest_loader.cpp
 * ==============================
 * WO-B9-001 — Batch Manifest Loader
 *
 * Minimal JSON parser for batch_manifest.json.
 * Handles: string, integer, boolean, array-of-string, array-of-object.
 * No third-party dependency.
 */

#include "src/batch/manifest_loader.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

// ─────────────────────────────────────────────────────────────────────────────
// Tokeniser helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Skip whitespace; returns index past it.
static size_t skip_ws(const std::string& s, size_t i) {
	while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
	return i;
}

// Expect character at position i (skip whitespace first).
// Returns next index.  Throws on mismatch.
static size_t expect(const std::string& s, size_t i, char c) {
	i = skip_ws(s, i);
	if (i >= s.size() || s[i] != c) {
		std::string msg = "expected '";
		msg += c;
		msg += "' at pos ";
		msg += std::to_string(i);
		if (i < s.size()) { msg += " (got '"; msg += s[i]; msg += "')"; }
		throw std::runtime_error(msg);
	}
	return i + 1;
}

// Parse a JSON string literal starting at pos (after the opening quote).
// Returns the unescaped value and sets pos to just after the closing quote.
static std::string parse_string(const std::string& s, size_t& pos) {
	pos = skip_ws(s, pos);
	if (pos >= s.size() || s[pos] != '"')
		throw std::runtime_error("expected '\"' at pos " + std::to_string(pos));
	++pos; // consume opening quote
	std::string result;
	while (pos < s.size() && s[pos] != '"') {
		if (s[pos] == '\\') {
			++pos;
			if (pos >= s.size()) throw std::runtime_error("unexpected end in string escape");
			switch (s[pos]) {
				case '"':  result += '"';  break;
				case '\\': result += '\\'; break;
				case '/':  result += '/';  break;
				case 'n':  result += '\n'; break;
				case 't':  result += '\t'; break;
				case 'r':  result += '\r'; break;
				default:   result += s[pos]; break;
			}
		} else {
			result += s[pos];
		}
		++pos;
	}
	if (pos >= s.size())
		throw std::runtime_error("unterminated string literal");
	++pos; // consume closing quote
	return result;
}

// Parse a JSON integer at pos.
static int parse_int(const std::string& s, size_t& pos) {
	pos = skip_ws(s, pos);
	bool neg = false;
	if (pos < s.size() && s[pos] == '-') { neg = true; ++pos; }
	if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
		throw std::runtime_error("expected integer at pos " + std::to_string(pos));
	int v = 0;
	while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
		v = v * 10 + (s[pos] - '0');
		++pos;
	}
	return neg ? -v : v;
}

// Parse a JSON boolean (true/false) at pos.
static bool parse_bool(const std::string& s, size_t& pos) {
	pos = skip_ws(s, pos);
	if (s.compare(pos, 4, "true") == 0)  { pos += 4; return true;  }
	if (s.compare(pos, 5, "false") == 0) { pos += 5; return false; }
	throw std::runtime_error("expected boolean at pos " + std::to_string(pos));
}

// Parse an array of strings: ["a", "b", ...]
static std::vector<std::string> parse_string_array(const std::string& s, size_t& pos) {
	pos = expect(s, pos, '[');
	std::vector<std::string> result;
	pos = skip_ws(s, pos);
	while (pos < s.size() && s[pos] != ']') {
		result.push_back(parse_string(s, pos));
		pos = skip_ws(s, pos);
		if (pos < s.size() && s[pos] == ',') ++pos;
		pos = skip_ws(s, pos);
	}
	pos = expect(s, pos, ']');
	return result;
}

// Parse one sweep-axis object: { "param": "...", "values": [...] }
static BatchManifestSweepAxis parse_sweep_axis(const std::string& s, size_t& pos) {
	BatchManifestSweepAxis ax;
	pos = expect(s, pos, '{');
	pos = skip_ws(s, pos);
	while (pos < s.size() && s[pos] != '}') {
		std::string key = parse_string(s, pos);
		pos = expect(s, pos, ':');
		if (key == "param") {
			ax.param = parse_string(s, pos);
		} else if (key == "values") {
			ax.values = parse_string_array(s, pos);
		} else {
			// skip unknown value — scan past it
			pos = skip_ws(s, pos);
			if (pos < s.size() && s[pos] == '"') parse_string(s, pos);
			else if (pos < s.size() && (s[pos] == 't' || s[pos] == 'f')) parse_bool(s, pos);
			else if (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '-')) parse_int(s, pos);
		}
		pos = skip_ws(s, pos);
		if (pos < s.size() && s[pos] == ',') ++pos;
		pos = skip_ws(s, pos);
	}
	pos = expect(s, pos, '}');
	return ax;
}

// Parse the sweep array: [ {...}, {...}, ... ]
static std::vector<BatchManifestSweepAxis> parse_sweep_array(const std::string& s, size_t& pos) {
	pos = expect(s, pos, '[');
	std::vector<BatchManifestSweepAxis> result;
	pos = skip_ws(s, pos);
	while (pos < s.size() && s[pos] != ']') {
		result.push_back(parse_sweep_axis(s, pos));
		pos = skip_ws(s, pos);
		if (pos < s.size() && s[pos] == ',') ++pos;
		pos = skip_ws(s, pos);
	}
	pos = expect(s, pos, ']');
	return result;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool load_manifest(const std::string& path,
				   BatchManifestSection& out,
				   std::string& error_msg) {
	// Read file
	std::ifstream f(path);
	if (!f.is_open()) {
		error_msg = "cannot open manifest: " + path;
		return false;
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	const std::string src = ss.str();

	// Parse top-level object
	try {
		size_t pos = 0;
		pos = expect(src, pos, '{');
		pos = skip_ws(src, pos);

		while (pos < src.size() && src[pos] != '}') {
			std::string key = parse_string(src, pos);
			pos = expect(src, pos, ':');

			if (key == "batch_id")     out.batch_id    = parse_string(src, pos);
			else if (key == "description") out.description = parse_string(src, pos);
			else if (key == "base_vsim")   out.base_vsim   = parse_string(src, pos);
			else if (key == "score_by")    out.score_by    = parse_string(src, pos);
			else if (key == "output_root") out.output_root = parse_string(src, pos);
			else if (key == "seeds")       out.seeds       = parse_int(src, pos);
			else if (key == "abort_on_fail")           out.abort_on_fail           = parse_bool(src, pos);
			else if (key == "write_per_run_meta")      out.write_per_run_meta      = parse_bool(src, pos);
			else if (key == "write_per_run_metrics")   out.write_per_run_metrics   = parse_bool(src, pos);
			else if (key == "sweep")       out.sweep = parse_sweep_array(src, pos);
			else {
				// Skip unknown value
				pos = skip_ws(src, pos);
				if (pos < src.size() && src[pos] == '"') parse_string(src, pos);
				else if (pos < src.size() && (src[pos]=='t'||src[pos]=='f')) parse_bool(src, pos);
				else if (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos]))||src[pos]=='-')) parse_int(src, pos);
			}

			pos = skip_ws(src, pos);
			if (pos < src.size() && src[pos] == ',') ++pos;
			pos = skip_ws(src, pos);
		}
	} catch (const std::exception& ex) {
		error_msg = std::string("manifest parse error: ") + ex.what();
		return false;
	}

	error_msg.clear();
	return true;
}

ManifestValidation validate_manifest(const BatchManifestSection& m) {
	ManifestValidation v;

	if (m.batch_id.empty())
		v.errors.push_back("batch_id is required");

	if (m.seeds < 1)
		v.errors.push_back("seeds must be >= 1");

	if (m.score_by != "energy" && m.score_by != "convergence" && m.score_by != "composite")
		v.warnings.push_back("score_by '" + m.score_by + "' unrecognised; defaulting to 'composite'");

	for (const auto& ax : m.sweep) {
		if (ax.param.empty())
			v.errors.push_back("sweep axis has empty param name");
		if (ax.values.empty())
			v.warnings.push_back("sweep axis '" + ax.param + "' has no values — axis will be skipped");
	}

	if (!v.errors.empty()) v.ok = false;
	return v;
}

} // namespace batch
} // namespace vsim
