// include/ufx_auto2/auto2_randomfill.hpp
// UFX_AUTO_2 Phase 2 -- Random Fill Options and Result
// VSEPR-SIM v5 beta8 -> beta9

#pragma once

#include <string>
#include <cstdint>

namespace vsepr::ufx {

struct Auto2RandomFillOptions {
	std::string db_path  = "ufx_auto2.sqlite";
	int         count    = 100;
	uint64_t    seed     = 0;
	bool        verbose  = false;
};

struct RandomFillResult {
	std::string db_path;
	std::string run_id;
	int         requested     = 0;
	int         generated     = 0;
	int         rejected      = 0;
	int         skipped       = 0;   // duplicate material_key collisions
	bool        success       = false;
	std::string error_message;
};

// Main generation loop. Returns a populated RandomFillResult.
RandomFillResult run_auto2_randomfill(const Auto2RandomFillOptions& opt);

// Print a RandomFillResult to stdout in UFX house style.
void print_randomfill_result(const RandomFillResult& result);

} // namespace vsepr::ufx
