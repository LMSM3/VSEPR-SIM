#pragma once
/**
 * i_output_format_handler.hpp  —  WO-74B: IOutputFormatHandler interface
 * ========================================================================
 * Abstract interface for pluggable export format writers.
 * Each format (CSV, JSON, SVG, XLSX, …) is one .cpp file that self-registers.
 * VsimOutputFilter iterates the registry instead of branching per format.
 *
 * Self-register in each format's .cpp:
 *
 *   #include "vsim/module_registry.hpp"
 *   static vsim::AutoRegister<CsvFormatHandler, IOutputFormatHandler> s_reg("csv");
 *
 * Dispatch in the filter / export layer:
 *
 *   auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();
 *   for (auto& fmt_name : exp.enabled_formats()) {
 *       auto handler = reg.create(fmt_name);
 *       if (handler && handler->handles(exp))
 *           handler->write(run_record, exp, output_dir);
 *   }
 *
 * Scale-layer gating is a property of each handler, not of the filter,
 * so adding a new scale layer or format never requires touching VsimOutputFilter.
 */

#include <filesystem>
#include <string_view>

// Forward declarations
namespace vsepr { namespace multiscale {
	struct NormalisedRecord;
	struct VsimRawRecord;
} }
namespace vsim { struct ExportSection; }
struct RunRecord;

namespace fs = std::filesystem;

// ============================================================================
// IOutputFormatHandler
// ============================================================================

class IOutputFormatHandler {
public:
	virtual ~IOutputFormatHandler() = default;

	// Canonical format key — e.g. "csv", "json", "svg", "xyz", "xlsx".
	// Must match the value used in [export] enabled_formats list.
	[[nodiscard]] virtual std::string_view format_key() const = 0;

	// Return true if this handler should run given the [export] section.
	// Handlers may check export flags (e.g. write_csv, write_json) here.
	[[nodiscard]] virtual bool handles(const vsim::ExportSection& exp) const = 0;

	// Write output records to output_dir in the handler's format.
	// Implementations must be idempotent (safe to call multiple times).
	virtual void write(const vsepr::multiscale::NormalisedRecord& record,
					   const vsim::ExportSection& exp,
					   const fs::path& output_dir) = 0;

	// Batch variant — default delegates to single-record write().
	virtual void write_batch(
		const std::vector<vsepr::multiscale::NormalisedRecord>& records,
		const vsim::ExportSection& exp,
		const fs::path& output_dir)
	{
		for (const auto& r : records) write(r, exp, output_dir);
	}

	// Scale layer this handler targets: "bead", "coarse_bead", "premacro",
	// "macro", or "" (all layers).
	[[nodiscard]] virtual std::string_view scale_layer() const { return ""; }

	// Human-readable description for vsepr doctor output.
	[[nodiscard]] virtual std::string_view description() const { return format_key(); }
};
