/**
 * src/xbundle/xbundle_reader.cpp
 * ================================
 * WO-72A  -  .X Bundle Format  —  Reader / Parser implementation
 *
 * .X file grammar (text-based, line-oriented):
 *
 *   XBUNDLE <version>
 *   # comment lines
 *   [manifest]
 *     key = value
 *   [[member]]
 *     key = value
 *     >>>
 *     <verbatim content lines>
 *     <<<
 *
 * WO-72A | V5.1.4
 */

#include "include/xbundle/xbundle_reader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace xbundle {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

XBundle XBundleReader::read_file(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open())
		throw std::runtime_error("XBundleReader: cannot open file: " + path);
	std::ostringstream ss;
	ss << f.rdbuf();
	XBundle b = read_string(ss.str(), path);
	return b;
}

XBundle XBundleReader::read_string(const std::string& src,
									const std::string& source_path) {
	XBundleReader r;
	r.parse(src);
	r.bundle_.source_path = source_path;
	return std::move(r.bundle_);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

std::string XBundleReader::trim(const std::string& s) {
	const auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return {};
	const auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

bool XBundleReader::parse_kv(const std::string& line,
							  std::string& key, std::string& value) {
	const auto eq = line.find('=');
	if (eq == std::string::npos) return false;
	key   = trim(line.substr(0, eq));
	value = trim(line.substr(eq + 1));
	// Strip surrounding quotes from value if present
	if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
		value = value.substr(1, value.size() - 2);
	return !key.empty();
}

// ---------------------------------------------------------------------------
// Main parse loop
// ---------------------------------------------------------------------------

void XBundleReader::parse(const std::string& src) {
	std::istringstream stream(src);
	std::string raw_line;
	bool magic_seen = false;

	while (std::getline(stream, raw_line)) {
		// Strip CRLF
		if (!raw_line.empty() && raw_line.back() == '\r')
			raw_line.pop_back();

		handle_line_internal(raw_line, magic_seen);
	}

	// Finalise any open member
	flush_entry();
}

void XBundleReader::handle_line_internal(const std::string& raw,
										  bool& magic_seen) {
	// While inside a body block, accumulate verbatim (check for end marker)
	if (in_body_) {
		if (trim(raw) == "<<<") {
			in_body_ = false;
		} else {
			if (current_entry_)
				current_entry_->content += raw + "\n";
		}
		return;
	}

	const std::string line = trim(raw);

	// Skip blank lines and comments at any level
	if (line.empty() || line[0] == '#') return;

	// Magic header (first non-blank non-comment line)
	if (!magic_seen) {
		// Expect: XBUNDLE <version>
		if (line.rfind(XBUNDLE_MAGIC, 0) == 0) {
			magic_seen = true;
			const auto sp = line.find(' ');
			if (sp != std::string::npos) {
				try {
					bundle_.manifest.format_version =
						std::stoi(line.substr(sp + 1));
				} catch (...) {}
			}
		}
		return;
	}

	// Section markers
	if (line == "[manifest]") {
		flush_entry();
		state_ = State::manifest;
		bundle_.manifest.populated = true;
		return;
	}
	if (line == "[[member]]") {
		flush_entry();
		state_ = State::member_header;
		bundle_.entries.emplace_back();
		current_entry_ = &bundle_.entries.back();
		return;
	}
	// Body begin marker
	if (line == ">>>") {
		in_body_ = true;
		return;
	}

	// Key = value lines
	std::string key, val;
	if (!parse_kv(line, key, val)) return;

	if (state_ == State::manifest) {
		if      (key == "name")         bundle_.manifest.name         = val;
		else if (key == "description")  bundle_.manifest.description  = val;
		else if (key == "author")       bundle_.manifest.author       = val;
		else if (key == "created")      bundle_.manifest.created      = val;
		else if (key == "entry_point")  bundle_.manifest.entry_point  = val;
		// entry_count is informational; we ignore it (we re-count)
	} else if (state_ == State::member_header && current_entry_) {
		if      (key == "name")  current_entry_->name = val;
		else if (key == "kind")  current_entry_->kind = entry_kind_from_string(val);
		else if (key == "path")  current_entry_->path = val;
		else if (key == "size") {
			try { current_entry_->declared_size =
					  static_cast<std::uint64_t>(std::stoull(val)); }
			catch (...) {}
		}
	}
}

void XBundleReader::flush_entry() {
	// Nothing to do — entries are pushed on [[member]] and edited in-place.
	// Trim trailing newline from content for cleanliness.
	if (current_entry_ && !current_entry_->content.empty()) {
		while (!current_entry_->content.empty() &&
			   (current_entry_->content.back() == '\n' ||
				current_entry_->content.back() == '\r'))
			current_entry_->content.pop_back();
	}
	current_entry_ = nullptr;
}

// The public handle_line is the internal driver via parse(); expose via parse
void XBundleReader::handle_line(const std::string& /*line*/) {}

void XBundleReader::begin_manifest()  { state_ = State::manifest; }
void XBundleReader::begin_member()    { state_ = State::member_header; }

} // namespace xbundle
} // namespace vsim
