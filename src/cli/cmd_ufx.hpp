// src/cli/cmd_ufx.hpp
// UFX AUTO2 CLI Command -- Phases 2-10
// Handles all ufx auto2 subcommands

#pragma once

#include "commands.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vsepr::cli {

// ============================================================================
// Block name constants — used in SQL queries; no raw strings at call sites.
// ============================================================================

namespace ufx_blocks {
	inline constexpr std::string_view molecular  = "molecular";
	inline constexpr std::string_view thermo     = "thermo";
	inline constexpr std::string_view crystal    = "crystal";
	inline constexpr std::string_view transport  = "transport";
	inline constexpr std::string_view mechanical = "mechanical";
	inline constexpr std::string_view meta       = "meta";
}

// ============================================================================
// Auto2Options — parsed once, typed.  Passed to every handler.
// ============================================================================

struct Auto2Options {
	std::filesystem::path db_path;
	int         batch        = 500;
	int         validate_batch = 100;
	int         web_batch    = 25;
	double      min_score    = 0.92;
	bool        verbose      = true;
	bool        recompute    = false;
	bool        web_validate = false;
	std::string cache_dir    = "web_cache";
	std::string run_dir;
};

// ============================================================================
// UfxCommand
// ============================================================================

class UfxCommand : public Command {
public:
	int         Execute(const std::vector<std::string>& args) override;
	std::string Name()        const override { return "ufx"; }
	std::string Description() const override { return "UFX AUTO2 materials database commands"; }
	std::string Help()        const override;

private:
	// ── Legacy handlers (Phase 2-5) — unchanged signatures ─────────────────
	int run_init_            (const std::vector<std::string>& args) const;
	int run_audit_           (const std::vector<std::string>& args) const;
	int run_randomfill_      (const std::vector<std::string>& args) const;
	int run_validate_        (const std::vector<std::string>& args) const;
	int run_promote_         (const std::vector<std::string>& args) const;
	int run_validate_web_    (const std::vector<std::string>& args) const;
	int run_backfill_provenance_(const std::vector<std::string>& args) const;
	int run_clear_block_     (const std::vector<std::string>& args) const;

	// ── Phase 6-10 handlers — take typed options ────────────────────────────
	int run_fill_molecular_  (const Auto2Options& opt) const;
	int run_fill_thermo_     (const Auto2Options& opt) const;
	int run_fill_crystal_    (const Auto2Options& opt) const;
	int run_fill_transport_  (const Auto2Options& opt) const;
	int run_fill_mechanical_ (const Auto2Options& opt) const;
	int run_fill_macro_      (const Auto2Options& opt) const;
	int run_score_           (const Auto2Options& opt) const;

	// ── Helpers ─────────────────────────────────────────────────────────────
	static Auto2Options parse_auto2_options_(const std::vector<std::string>& args);
	static std::string  resolve_db_path_    (const std::vector<std::string>& args,
											 const std::string& default_path);
	static void         print_phase_banner_ (std::string_view phase_label,
											 std::string_view block_name,
											 std::string_view db_path,
											 int batch);
};

} // namespace vsepr::cli
