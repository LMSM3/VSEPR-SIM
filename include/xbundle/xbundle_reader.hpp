#pragma once
/**
 * include/xbundle/xbundle_reader.hpp
 * =====================================
 * WO-72A  -  .X Bundle Format  —  Reader / Parser
 *
 * Reads a .X bundle file (or string) into an XBundle struct.
 *
 * Usage:
 *   auto bundle = XBundleReader::read_file("suite.X");
 *   auto bundle = XBundleReader::read_string(src, "suite.X");
 *
 * WO-72A | V5.1.4
 */

#include "include/xbundle/xbundle_document.hpp"
#include <string>

namespace vsim {
namespace xbundle {

class XBundleReader {
public:
	static XBundle read_file  (const std::string& path);
	static XBundle read_string(const std::string& src,
							   const std::string& source_path = "");

private:
	XBundle bundle_;

	// Parse states
	enum class State { top, manifest, member_header, member_body };
	State       state_          = State::top;
	bool        in_body_        = false;
	XBundleEntry* current_entry_ = nullptr;

	void parse(const std::string& src);
	void handle_line(const std::string& line);
	void handle_line_internal(const std::string& raw, bool& magic_seen);

	void begin_manifest();
	void begin_member();
	void flush_entry();

	static std::string trim(const std::string& s);
	static bool parse_kv(const std::string& line,
						 std::string& key, std::string& value);
};

} // namespace xbundle
} // namespace vsim
