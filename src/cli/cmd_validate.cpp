/**
 * cmd_validate.cpp — vsper validate subcommand implementation
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include "cli/cmd_validate.hpp"
#include "vsim/vsim_parser.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace vsepr::cli {

// ANSI color helpers (degrade gracefully on non-TTY)
namespace {
	const char* GREEN  = "\033[0;32m";
	const char* YELLOW = "\033[1;33m";
	const char* RED    = "\033[0;31m";
	const char* BOLD   = "\033[1m";
	const char* RESET  = "\033[0m";
}

int cmd_validate(const std::vector<std::string>& args) {
	if (args.empty()) {
		std::cerr << RED << "error:" << RESET
				  << " vsper validate requires a .vsim file path\n"
				  << "  usage: vsper validate <path/to/script.vsim>\n";
		return 2;
	}

	const std::string& path = args[0];

	// -----------------------------------------------------------------------
	// Parse
	// -----------------------------------------------------------------------
	vsim::VsimDocument doc;
	try {
		doc = vsim::VsimParser::parse_file(path);
	} catch (const vsim::ParseError& e) {
		std::cerr << RED << "parse error:" << RESET << " " << e.what() << "\n";
		return 2;
	} catch (const std::exception& e) {
		std::cerr << RED << "error:" << RESET << " " << e.what() << "\n";
		return 2;
	}

	// -----------------------------------------------------------------------
	// Validate
	// -----------------------------------------------------------------------
	auto result = doc.validate();

	// -----------------------------------------------------------------------
	// Report header
	// -----------------------------------------------------------------------
	std::cout << "\n"
			  << BOLD << "vsper validate" << RESET << "  " << path << "\n"
			  << std::string(60, '-') << "\n\n";

	// Document summary
	std::cout << doc.summary() << "\n";
	std::cout << std::string(60, '-') << "\n";

	// -----------------------------------------------------------------------
	// Warnings
	// -----------------------------------------------------------------------
	if (!result.warnings.empty()) {
		for (const auto& w : result.warnings) {
			std::cout << YELLOW << "  warning:" << RESET << " " << w << "\n";
		}
		std::cout << "\n";
	}

	// -----------------------------------------------------------------------
	// Errors
	// -----------------------------------------------------------------------
	if (!result.errors.empty()) {
		for (const auto& e : result.errors) {
			std::cout << RED << "  error:" << RESET << " " << e << "\n";
		}
		std::cout << "\n"
				  << RED << "INVALID" << RESET
				  << " — " << result.errors.size() << " error(s), "
				  << result.warnings.size() << " warning(s)\n\n";
		return 1;
	}

	// -----------------------------------------------------------------------
	// Success
	// -----------------------------------------------------------------------
	std::cout << GREEN << "OK" << RESET
			  << " — document is valid";
	if (!result.warnings.empty())
		std::cout << " (" << result.warnings.size() << " warning(s))";
	std::cout << "\n\n";

	return 0;
}

} // namespace vsepr::cli
