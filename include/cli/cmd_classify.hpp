#pragma once
/**
 * cmd_classify.hpp  --  vsepr classify <input.vsim>
 *
 * WO-83D/I/Q: Rich VSEPR + OrganicCandidate summary for a .vsim script's
 *             stated formula/molecule, printed with ANSI color to the terminal.
 * WO-84E: Optional JSON-lite + Markdown report export.
 */

#include <filesystem>
#include <string>

namespace vsepr {
namespace cli {

// Run the classify preview for the given .vsim path.
// Optional report_md_path and report_json_path trigger WO-84E export.
// Returns 0 on success, 1 on error.
int run_classify_preview(const std::filesystem::path& vsim_path,
                         const std::filesystem::path& report_md_path   = {},
                         const std::filesystem::path& report_json_path  = {});

} // namespace cli
} // namespace vsepr
