/**
 * vsepr-batch  -  standalone batch inlet executable
 * ==================================================
 * Thin wrapper over vsepr::cli::cmd_batch().
 *
 * Usage (identical to `vsepr batch`):
 *   vsepr-batch [options] [script.vsim ...]
 *   vsepr-batch --manifest ci/nightly.txt --jobs 4 --report out/batch.json
 *   vsepr-batch --dir scripts/ --stop-on-fail
 *   vsepr-batch --help
 *
 * WO-BATCH-INLET  |  v5.0.0
 */
#include "cli/cmd_batch.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
	// Enable ANSI virtual terminal sequences on Windows 10+
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	if (GetConsoleMode(hOut, &dwMode))
		SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

	// argv[0] is the binary name; pass everything after it to cmd_batch
	std::vector<std::string> args;
	args.reserve(static_cast<size_t>(argc - 1));
	for (int i = 1; i < argc; ++i)
		args.emplace_back(argv[i]);

	return vsepr::cli::cmd_batch(args);
}
