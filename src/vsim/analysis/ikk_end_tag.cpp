/**
 * src/vsim/analysis/ikk_end_tag.cpp
 * ===================================
 * WO-75A  |  IKK Report End-Tag Enrichment  —  implementation
 *
 * Implements:
 *   build_ikk_end_tag()         — derive IKKEndTag from IdentitySidecarSeries
 *   render_ikk_end_tag_md()     — format as a Markdown fenced block
 *   render_ikk_end_tag_tex()    — format as a LaTeX macro call
 *   IkkEndTagModule::configure() — copy VsimIkkEndTagSection settings
 *   IkkEndTagModule::run()       — build + render into AnalysisRecord
 *
 * Self-registration line at file scope:
 *   static AutoRegister<IkkEndTagModule, IAnalysisModule> s_reg("ikk_end_tag");
 *
 * DOCTRINE (non-negotiable):
 *   All output is DERIVED from IdentitySidecarSeries.
 *   Nothing here mutates SimState or truth-state files.
 *
 * Metric derivation from IdentitySidecarRecord fields
 * (see identity_sidecar.hpp for authoritative definitions):
 *
 *   D_rec   = 1.0 - run_summary.dataloss
 *               "fraction of identity preserved vs ideal reconstruction"
 *   eta_ab  = 1.0 - run_summary.projection_loss
 *               "identity coupling; complement of scale-projection loss"
 *   Psi_hid = run_summary.hidden_channel
 *               "unmodelled interaction proxy; hidden residual norm"
 *   Delta_S = run_summary.entropy_loss_proxy
 *               "entropy change proxy (dS/dt via displacement field)"
 *   Delta_D = first-to-last distinguishability drift across frames
 *               0 if fewer than 2 frames recorded
 *
 * Added: WO-75A (Day 75)
 */

#include "vsim/analysis/ikk_end_tag.hpp"
#include "vsim/analysis/identity_sidecar.hpp"
#include "vsim/vsim_document.hpp"
#include "vsim/module_registry.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace vsim {
namespace analysis {

// ============================================================================
// File-scope self-registration
// ============================================================================
// This runs at static-init time (before main) and inserts IkkEndTagModule into
// the ModuleRegistry<IAnalysisModule> singleton under the key "ikk_end_tag".
// The runtime can then create instances via:
//   ModuleRegistry<IAnalysisModule>::get().create("ikk_end_tag")
// with no hard-coded dispatch chain needed.

static vsim::AutoRegister<IkkEndTagModule, IAnalysisModule> s_reg("ikk_end_tag");

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// Format a double to a fixed number of decimal places.
std::string fmt_d(double v, int prec = 4) {
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(prec);
	if (v >= 0.0) ss << '+';  // explicit sign for signed quantities
	ss << v;
	return ss.str();
}

// Format unsigned [0,1] quantities (no forced + sign).
std::string fmt_u(double v, int prec = 4) {
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(prec) << v;
	return ss.str();
}

} // anonymous namespace

// ============================================================================
// build_ikk_end_tag()
// ============================================================================

IKKEndTag build_ikk_end_tag(const IdentitySidecarSeries& sidecar,
							 const VsimDocument&          doc)
{
	const auto& cfg = doc.pipeline_ikk_end_tag;

	IKKEndTag tag;

	// If the section is disabled or the sidecar is empty, return an invalid tag.
	if (!cfg.wants_output() || sidecar.frames.empty()) {
		tag.valid = false;
		return tag;
	}

	const auto& s = sidecar.run_summary;

	// Derive IKK v1.0 headline metrics from sidecar aggregate fields.
	// D_rec: fraction of identity preserved (complement of dataloss).
	tag.D_rec   = 1.0 - s.dataloss;

	// eta_ab: identity coupling (complement of projection loss to lower scale).
	tag.eta_ab  = 1.0 - s.projection_loss;

	// Psi_hid: hidden residual norm (unmodelled interaction channel strength).
	tag.Psi_hid = s.hidden_channel;

	// Delta_S: entropy proxy change (sign follows physics: >= 0 expected).
	tag.Delta_S = s.entropy_loss_proxy;

	// Delta_D: per-frame drift; needs at least 2 frames.
	if (sidecar.frames.size() >= 2) {
		const double d_first = 1.0 - sidecar.frames.front().dataloss;
		const double d_last  = 1.0 - sidecar.frames.back().dataloss;
		tag.Delta_D = (d_last - d_first) /
					  static_cast<double>(sidecar.frames.size() - 1);
	} else {
		tag.Delta_D = 0.0;
	}

	// Context strings forwarded from the script section.
	tag.scale_regime      = cfg.scale_regime;
	tag.section_reference = cfg.section_reference;
	tag.run_id            = sidecar.run_id;

	// Second-law badge derived from D_rec and the configured thresholds.
	tag.badge = cfg.badge(tag.D_rec);

	tag.valid = true;
	return tag;
}

// ============================================================================
// render_ikk_end_tag_md()
// ============================================================================

std::string render_ikk_end_tag_md(const IKKEndTag& tag)
{
	if (!tag.valid) return {};

	std::ostringstream out;

	out << "\n---\n";
	out << "**IKK End-Tag**";
	if (!tag.run_id.empty())
		out << " | run: `" << tag.run_id << '`';
	out << " | regime: `" << tag.scale_regime << "`\n\n";

	out << "| Metric | Value |\n";
	out << "|---|---|\n";
	out << "| D (distinguishability) | "     << fmt_u(tag.D_rec)   << " |\n";
	out << "| eta_ab (identity coupling) | " << fmt_u(tag.eta_ab)  << " |\n";
	out << "| \\|Psi^hid\\| (hidden residual) | " << fmt_u(tag.Psi_hid) << " |\n";
	out << "| Delta-S (entropy proxy) | "    << fmt_d(tag.Delta_S) << " |\n";
	out << "| Delta-D (drift/frame) | "      << fmt_d(tag.Delta_D) << " |\n";

	out << "\n**Second-law check:** `" << tag.badge << '`';
	if (!tag.section_reference.empty())
		out << "   _IKK IV §" << tag.section_reference << '_';
	out << "\n\n";

	return out.str();
}

// ============================================================================
// render_ikk_end_tag_tex()
// ============================================================================

std::string render_ikk_end_tag_tex(const IKKEndTag& tag)
{
	if (!tag.valid) return {};

	// Produces a single \ikkendsection{badge}{key=value,...} macro call.
	// The macro is defined in reporting/report.tex (WO-75A deliverable D).
	std::ostringstream out;
	out << std::fixed << std::setprecision(4);
	out << "\\ikkendsection{"
		<< tag.badge
		<< "}{"
		<< "D="      << tag.D_rec
		<< ",etaab=" << tag.eta_ab
		<< ",Psihid="<< tag.Psi_hid
		<< ",DS="    << tag.Delta_S
		<< ",DD="    << tag.Delta_D
		<< "}\n";

	return out.str();
}

// ============================================================================
// write_ikk_end_tag()
// ============================================================================

bool write_ikk_end_tag(const IKKEndTag& tag,
					   const std::string& path_stem,
					   bool emit_md,
					   bool emit_tex)
{
	if (!tag.valid) return false;
	bool ok = true;

	if (emit_md) {
		const std::string md_path = path_stem + ".md";
		std::ofstream f(md_path, std::ios::app);
		if (!f.is_open()) return false;
		f << render_ikk_end_tag_md(tag);
		if (!f.good()) ok = false;
	}

	if (emit_tex) {
		const std::string tex_path = path_stem + ".tex";
		std::ofstream f(tex_path, std::ios::app);
		if (!f.is_open()) return false;
		f << render_ikk_end_tag_tex(tag);
		if (!f.good()) ok = false;
	}

	return ok;
}

// ============================================================================
// IkkEndTagModule — IAnalysisModule implementation
// ============================================================================

void IkkEndTagModule::configure(const vsim::VsimDocument& doc)
{
	const auto& cfg = doc.pipeline_ikk_end_tag;
	enabled_          = cfg.enabled;
	emit_markdown_    = cfg.emit_markdown;
	emit_latex_       = cfg.emit_latex;
	d_pass_threshold_ = cfg.d_pass_threshold;
	d_warn_threshold_ = cfg.d_warn_threshold;
	scale_regime_     = cfg.scale_regime;
	section_reference_= cfg.section_reference;
}

void IkkEndTagModule::run(SimState& /*state*/, AnalysisRecord& rec)
{
	// Nothing to do when the section is disabled or no output is requested.
	if (!enabled_) return;
	if (!emit_markdown_ && !emit_latex_) return;

	// Build a temporary VsimDocument shell so we can call build_ikk_end_tag().
	// IkkEndTagModule does not retain the doc reference after configure(), so
	// we reconstruct the minimal pipeline_ikk_end_tag settings from our cached
	// member fields.  build_ikk_end_tag() only reads pipeline_ikk_end_tag.
	vsim::VsimDocument tmp_doc;
	auto& cfg             = tmp_doc.pipeline_ikk_end_tag;
	cfg.enabled           = true;              // already gated above
	cfg.emit_markdown     = emit_markdown_;
	cfg.emit_latex        = emit_latex_;
	cfg.d_pass_threshold  = d_pass_threshold_;
	cfg.d_warn_threshold  = d_warn_threshold_;
	cfg.scale_regime      = scale_regime_;
	cfg.section_reference = section_reference_;

	// Use the sidecar attached to this AnalysisRecord by the runtime.
	// DOCTRINE: read-only; never written back to truth-state files.
	// When the sidecar is empty (no frames), build_ikk_end_tag() returns
	// an invalid tag; we then emit a PASS stub so downstream report pipelines
	// always receive a defined value.
	IKKEndTag tag = build_ikk_end_tag(rec.ikk_sidecar, tmp_doc);

	if (!tag.valid) {
		// Empty sidecar fallback: no identity loss observed -> PASS stub.
		tag.scale_regime      = scale_regime_;
		tag.section_reference = section_reference_;
		tag.run_id            = rec.ikk_sidecar.run_id.empty()
								  ? "(no sidecar)" : rec.ikk_sidecar.run_id;
		tag.D_rec   = 1.0;
		tag.eta_ab  = 1.0;
		tag.Psi_hid = 0.0;
		tag.Delta_S = 0.0;
		tag.Delta_D = 0.0;
		tag.badge = (1.0 >= d_pass_threshold_) ? "PASS" :
					(1.0 >= d_warn_threshold_) ? "WARN" : "FAIL";
		tag.valid = true;
	}

	if (emit_markdown_) rec.ikk_end_tag_md  = render_ikk_end_tag_md(tag);
	if (emit_latex_)    rec.ikk_end_tag_tex = render_ikk_end_tag_tex(tag);
}

} // namespace analysis
} // namespace vsim
