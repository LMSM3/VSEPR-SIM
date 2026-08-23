#pragma once
/**
 * include/vsim/analysis/ikk_end_tag.hpp
 * =======================================
 * WO-75A  |  IKK Report End-Tag Enrichment
 *
 * Provides two things:
 *
 *   1. IKKEndTag
 *        Plain data aggregate produced by build_ikk_end_tag().
 *        Holds the five headline IKK metrics derived from an
 *        IdentitySidecarSeries and the formatted badge/regime strings.
 *        Immutable once built — no public setters.
 *
 *   2. IkkEndTagModule  (implements IAnalysisModule)
 *        Self-registering analysis pass that builds an IKKEndTag from
 *        whichever sidecar series the runtime supplies and writes the
 *        formatted output to AnalysisRecord::ikk_end_tag_md / _tex.
 *
 * DOCTRINE (mirrors identity_sidecar.hpp):
 *   - All metrics are DERIVED from IdentitySidecarSeries only.
 *   - IKKEndTag values must NEVER be written back into truth-state
 *     (.xyz / .xyzFull) files.
 *   - The second-law check is advisory; it never aborts execution.
 *
 * IKK Notation Registry v1.0 field names used here:
 *   D_rec       – distinguishability (identity preserved fraction)   [0,1]
 *   eta_ab      – identity coupling coefficient                       [0,1]
 *   Psi_hid     – hidden residual norm  |Psi^hid|                  >= 0
 *   Delta_S     – entropy proxy change  (Delta-S = S_after - S_before)
 *   Delta_D     – distinguishability change per frame (dD/dt proxy)
 *
 * Scale-regime identifiers (IKK notation):
 *   "S0"    = existence       "S2"  = colour
 *   "S3"    = EM/spatial      "S4"  = weak
 *   "S_mat" = material
 *
 * Second-law check:
 *   badge = PASS  when D_rec >= d_pass_threshold
 *   badge = WARN  when d_warn_threshold <= D_rec < d_pass_threshold
 *   badge = FAIL  when D_rec < d_warn_threshold
 *
 * Registry key for self-registration: "ikk_end_tag"
 * Matching [analysis.ikk_end_tag] section in .vsim scripts.
 *
 * Added: WO-75A (Day 75)
 */

#include <string>
#include <string_view>

// Forward declarations — avoid pulling full headers into every translation unit.
namespace vsim          { struct VsimDocument; }
namespace vsim::analysis{ struct IdentitySidecarSeries; }
struct SimState;

// i_analysis_module.hpp defines both IAnalysisModule and AnalysisRecord.
#include "vsim/analysis/i_analysis_module.hpp"

namespace vsim {
namespace analysis {

// ============================================================================
// IKKEndTag  —  immutable aggregate of one run's IKK headline metrics
// ============================================================================

struct IKKEndTag {
	// ---- IKK v1.0 headline metrics ----------------------------------------
	double D_rec     { 0.0 };  // distinguishability        [0, 1]
	double eta_ab    { 0.0 };  // identity coupling         [0, 1]
	double Psi_hid   { 0.0 };  // hidden residual norm     >= 0
	double Delta_S   { 0.0 };  // entropy change proxy (nats)
	double Delta_D   { 0.0 };  // distinguishability drift per frame

	// ---- provenance / context ---------------------------------------------
	std::string scale_regime      { "S3" };  // active rung of scale ladder
	std::string section_reference { "" };    // e.g. "IV §3" for report anchor
	std::string run_id            { "" };    // forwarded from sidecar

	// ---- second-law badge (PASS / WARN / FAIL) ----------------------------
	std::string badge { "PASS" };            // set by build_ikk_end_tag()

	// ---- validity flag ----------------------------------------------------
	// false when the sidecar series was empty or the section was disabled.
	bool valid { false };
};

// ============================================================================
// Free functions  —  build and render IKKEndTag
// ============================================================================

/**
 * build_ikk_end_tag()
 *
 * Derives an IKKEndTag from the aggregate values in a completed
 * IdentitySidecarSeries.  Uses doc.pipeline_ikk_end_tag for thresholds
 * and context strings.
 *
 * Returns a tag with valid=false when the section is disabled or the
 * sidecar series is empty.
 *
 * Mapping from IdentitySidecarRecord fields:
 *   D_rec   = 1.0 - run_summary.dataloss
 *   eta_ab  = 1.0 - run_summary.projection_loss
 *   Psi_hid = run_summary.hidden_channel
 *   Delta_S = run_summary.entropy_loss_proxy
 *   Delta_D = (D_last - D_first) across all frames; 0 if < 2 frames
 */
IKKEndTag build_ikk_end_tag(const IdentitySidecarSeries& sidecar,
							 const VsimDocument&           doc);

/**
 * render_ikk_end_tag_md()
 *
 * Renders the IKKEndTag as a Markdown fenced block suitable for
 * appending to any report section.  Returns an empty string when
 * tag.valid is false.
 *
 * Output format:
 *
 *   ---
 *   **IKK End-Tag** | run: <run_id> | regime: <scale_regime>
 *   | Metric | Value |
 *   |---|---|
 *   | D_rec (distinguishability) | 0.83 |
 *   | eta_ab (identity coupling) | 0.91 |
 *   | |Psi^hid| (hidden residual) | 0.13 |
 *   | Delta-S (entropy proxy) | +0.021 |
 *   | Delta-D (drift per frame) | -0.003 |
 *
 *   **Second-law check:** PASS   IKK IV §<section_reference>
 */
std::string render_ikk_end_tag_md(const IKKEndTag& tag);

/**
 * render_ikk_end_tag_tex()
 *
 * Renders the IKKEndTag as a LaTeX \ikkendsection{}{} macro call.
 * Returns an empty string when tag.valid is false.
 *
 * Output format:
 *   \ikkendsection{<badge>}{D=0.83,etaab=0.91,Psihid=0.13,DS=0.021,DD=-0.003}
 */
std::string render_ikk_end_tag_tex(const IKKEndTag& tag);

/**
 * write_ikk_end_tag()
 *
 * Appends the rendered IKK end-tag block to a report file on disk.
 *
 * When emit_md is true, the Markdown block is appended to <path>.md
 * (creating the file if necessary).  When emit_tex is true, the LaTeX
 * macro call is appended to <path>.tex.
 *
 * Returns false on file-write failure or when tag.valid is false.
 * The sidecar-only doctrine applies: this function must never be called
 * with derived values as force inputs or truth-state mutations.
 *
 * @param tag       IKKEndTag to render (must have valid = true).
 * @param path_stem Path without extension; e.g. "out/my_run/report"
 *                  produces "out/my_run/report.md" and/or ".tex".
 * @param emit_md   Append Markdown block to <path_stem>.md.
 * @param emit_tex  Append LaTeX macro call to <path_stem>.tex.
 */
bool write_ikk_end_tag(const IKKEndTag& tag,
                       const std::string& path_stem,
                       bool emit_md  = true,
                       bool emit_tex = false);

// ============================================================================
// IkkEndTagModule  —  self-registering IAnalysisModule
// ============================================================================
/**
 * IkkEndTagModule
 *
 * Pluggable analysis module for the IKK end-tag pass.
 *
 * Lifecycle (IAnalysisModule contract):
 *   1. configure(doc)   — copies VsimIkkEndTagSection settings into the module.
 *   2. run(state, rec)  — builds the IKKEndTag from rec.ikk_sidecar_series
 *                         and stores render output in rec.ikk_end_tag_md and
 *                         rec.ikk_end_tag_tex.  Does nothing if disabled.
 *
 * Self-registration (static-init time):
 *   static AutoRegister<IkkEndTagModule, IAnalysisModule> s_reg("ikk_end_tag");
 *
 * Registry key: "ikk_end_tag"   (matches [analysis.ikk_end_tag] section)
 */
class IkkEndTagModule : public IAnalysisModule {
public:
	IkkEndTagModule() = default;
	~IkkEndTagModule() override = default;

	// IAnalysisModule interface -----------------------------------------------

	[[nodiscard]] std::string_view name()        const override { return "ikk_end_tag"; }
	[[nodiscard]] std::string_view description() const override {
		return "WO-75A: IKK report end-tag enrichment "
			   "(D, eta_ab, |Psi^hid|, Delta-S, badge)";
	}

	// configure() — called once before run(), stores section settings.
	void configure(const vsim::VsimDocument& doc) override;

	// run() — builds end-tag from rec.ikk_sidecar_series and writes
	//         formatted output into rec.ikk_end_tag_md / rec.ikk_end_tag_tex.
	void run(SimState& state, AnalysisRecord& rec) override;

	// Does not require PBC.
	[[nodiscard]] bool requires_pbc() const override { return false; }

private:
	bool        enabled_          { false };
	bool        emit_markdown_    { true  };
	bool        emit_latex_       { false };
	double      d_pass_threshold_ { 0.60  };
	double      d_warn_threshold_ { 0.35  };
	std::string scale_regime_     { "S3"  };
	std::string section_reference_{};
};

} // namespace analysis
} // namespace vsim
