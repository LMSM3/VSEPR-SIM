// ============================================================================
// console_render.cpp  --  shared [console] narration renderer  (WO-84 Preflight)
// ============================================================================
// Implementation of the single shared console-render helper pair declared in
// include/vsim/console_render.hpp.  Keeps the print_console rendering + scan
// logic in one place so classify / validate / run (and future commands) do not
// each carry their own copy.
// ============================================================================

#include "vsim/console_render.hpp"
#include "vsim/vsim_document.hpp"

#include <cctype>
#include <fstream>
#include <ostream>

namespace vsim {

void render_console_block(const std::vector<std::string>& messages,
						  std::ostream& os,
						  bool color) {
	if (messages.empty()) return;

	const char* CYAN  = color ? "\033[0;36m" : "";
	const char* GREEN = color ? "\033[0;32m" : "";
	const char* RESET = color ? "\033[0m"    : "";

	os << CYAN << "  [console]" << RESET << "\n";
	for (const auto& m : messages) {
		os << GREEN << "  > " << RESET << m << "\n";
	}
	os << "\n";
}

void render_console_block(const std::vector<ConsolePrint>& prints,
						  std::ostream& os,
						  bool color) {
	std::vector<std::string> messages;
	messages.reserve(prints.size());
	for (const auto& cp : prints) {
		messages.push_back(cp.message);
	}
	render_console_block(messages, os, color);
}

std::vector<std::string>
collect_console_prints_lightweight(const std::filesystem::path& path) {
	std::vector<std::string> out;
	std::ifstream f(path);
	if (!f) return out;

	std::string line;
	while (std::getline(f, line)) {
		// Quote-aware comment strip.
		bool in_str = false;
		for (std::size_t i = 0; i < line.size(); ++i) {
			if (line[i] == '"') in_str = !in_str;
			if (!in_str && line[i] == '#') { line.resize(i); break; }
		}
		// Left-trim.
		std::size_t s = 0;
		while (s < line.size() && std::isspace((unsigned char)line[s])) ++s;
		std::string t = line.substr(s);
		// Right-trim.
		while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();

		if (t.compare(0, 13, "print_console") != 0) continue;
		char after = (t.size() > 13) ? t[13] : '\0';
		if (!(after == '\0' || after == ' ' || after == '\t' || after == '"')) continue;

		// Everything after the keyword is the message.
		std::string msg = (t.size() > 13) ? t.substr(13) : "";
		while (!msg.empty() && std::isspace((unsigned char)msg.front())) msg.erase(msg.begin());
		while (!msg.empty() && std::isspace((unsigned char)msg.back())) msg.pop_back();
		if (msg.size() >= 2 && msg.front() == '"' && msg.back() == '"') {
			msg = msg.substr(1, msg.size() - 2);
		}
		out.push_back(std::move(msg));
	}
	return out;
}

} // namespace vsim
