#pragma once
/**
 * i_analysis_module.hpp  —  WO-74: IAnalysisModule interface
 * ===========================================================
 * Abstract base for all pluggable analysis passes.
 *
 * Self-register each concrete module at file scope in its .cpp:
 *
 *   #include "vsim/module_registry.hpp"
 *   static vsim::AutoRegister<MyModule, IAnalysisModule> s_reg("my_module");
 *
 * VsimRuntime dispatch (replaces if/switch chain):
 *
 *   for (auto& name : doc.enabled_analysis_modules()) {
 *       auto mod = vsim::ModuleRegistry<IAnalysisModule>::get().create(name);
 *       if (mod) { mod->configure(doc); mod->run(state, record); }
 *   }
 *
 * The VSIM section name is the registry key:
 *   [analysis.scale_sampling] -> "scale_sampling"
 *   [analysis.structure]      -> "structure"
 */

#include <string>
#include <string_view>

// Forward declarations — avoid pulling the full headers into every module.
namespace vsim { struct VsimDocument; }
struct SimState;

// Pull in IdentitySidecarSeries for AnalysisRecord::ikk_sidecar.
#include "vsim/analysis/identity_sidecar.hpp"

// ============================================================================
// AnalysisRecord  —  accumulator passed through the analysis module chain
// ============================================================================
/**
 * AnalysisRecord is a plain aggregate that every IAnalysisModule writes into.
 * Modules append their output here; nothing is written back into particle state
 * or truth-state files.
 *
 * When adding a new module:
 *   1. Add a result field for your module (e.g. ikk_end_tag_md below).
 *   2. Populate it inside your module's run() override.
 *   3. Document the field here.
 */
struct AnalysisRecord {
    // WO-75A  —  IKK end-tag formatted output
    // Populated by IkkEndTagModule::run() when [analysis.ikk_end_tag] enabled = true.
    // ikk_end_tag_md  : Markdown fenced block for appending to report sections.
    // ikk_end_tag_tex : LaTeX \ikkendsection{}{} macro call (empty when emit_latex=false).
    std::string ikk_end_tag_md;
    std::string ikk_end_tag_tex;

    // WO-75A  —  IKK sidecar attachment point
    // The runtime should populate this before calling IkkEndTagModule::run().
    // When empty (no frames), IkkEndTagModule::run() emits a PASS stub
    // (identity fully preserved with no loss data recorded).
    // DOCTRINE: read-only within all IAnalysisModule::run() overrides;
    // never written back to truth-state (.xyz / .xyzFull) files.
    vsim::analysis::IdentitySidecarSeries ikk_sidecar;
};

// ============================================================================
// IAnalysisModule
// ============================================================================

class IAnalysisModule {
public:
	virtual ~IAnalysisModule() = default;

	// Canonical registry key — must match the [analysis.<key>] section name.
	[[nodiscard]] virtual std::string_view name() const = 0;

	// Called once before run() with the parsed document.
	// Implementations should read their [analysis.<name>] section here.
	virtual void configure(const vsim::VsimDocument& doc) = 0;

	// Execute the analysis pass against the current simulation state.
	// Results are written into record.
	virtual void run(SimState& state, AnalysisRecord& record) = 0;

	// Optional: return true if this module requires periodic boundary conditions.
	[[nodiscard]] virtual bool requires_pbc() const { return false; }

	// Optional: human-readable description for vsepr doctor output.
	[[nodiscard]] virtual std::string_view description() const { return name(); }
};
