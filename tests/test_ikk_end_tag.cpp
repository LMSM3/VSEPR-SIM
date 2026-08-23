// =============================================================================
// tests/test_ikk_end_tag.cpp   WO-75A  (Group 87)
// =============================================================================
// IKK1   Default state: section disabled, no output requested
// IKK2   enabled = true parsed and stored correctly
// IKK3   emit_markdown / emit_latex flag round-trip
// IKK4   d_pass_threshold / d_warn_threshold round-trip
// IKK5   Threshold guard: warn clamped below pass when set equal
// IKK6   scale_regime string round-trip
// IKK7   section_reference string round-trip
// IKK8   badge() helper: PASS / WARN / FAIL boundary values
// IKK9   wants_output() helper
// IKK10  build_ikk_end_tag() invalid when section disabled
// IKK11  build_ikk_end_tag() invalid when sidecar empty
// IKK12  build_ikk_end_tag() D_rec = 1 - dataloss
// IKK13  build_ikk_end_tag() eta_ab = 1 - projection_loss
// IKK14  build_ikk_end_tag() Delta_D = 0 for single-frame sidecar
// IKK15  build_ikk_end_tag() Delta_D computed for multi-frame sidecar
// IKK16  render_ikk_end_tag_md() empty for invalid tag
// IKK17  render_ikk_end_tag_md() contains badge and key fields
// IKK18  render_ikk_end_tag_tex() empty for invalid tag
// IKK19  render_ikk_end_tag_tex() contains \ikkendsection macro
// IKK20  Full parser round-trip: complete [analysis.ikk_end_tag] block
// =============================================================================

#include <cassert>
#include <cstdio>
#include <string>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"
#include "include/vsim/analysis/ikk_end_tag.hpp"
#include "include/vsim/analysis/identity_sidecar.hpp"

using namespace vsim;
using namespace vsim::analysis;

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

static VsimDocument parse(const char* script) {
	return VsimParser::parse_string(script, "<test>");
}

// Build a minimal single-frame sidecar for metric derivation tests.
static IdentitySidecarSeries make_sidecar(double dataloss,
										  double proj_loss,
										  double hidden,
										  double entropy) {
	IdentitySidecarSeries s;
	s.run_id = "test_run";
	IdentitySidecarRecord r;
	r.frame_index        = 0;
	r.dataloss           = dataloss;
	r.projection_loss    = proj_loss;
	r.hidden_channel     = hidden;
	r.entropy_loss_proxy = entropy;
	s.push_frame(r);
	s.build_summary();
	return s;
}

// ---------------------------------------------------------------------------
// IKK1  Default state
// ---------------------------------------------------------------------------
static void IKK1() {
	VsimIkkEndTagSection sec;
	assert(sec.enabled            == false);
	assert(sec.emit_markdown      == true);
	assert(sec.emit_latex         == false);
	assert(sec.d_pass_threshold   == 0.60);
	assert(sec.d_warn_threshold   == 0.35);
	assert(sec.scale_regime       == "S3");
	assert(sec.section_reference  == "");
	std::puts("  IKK1  PASS  default state: enabled=false, correct defaults");
}

// ---------------------------------------------------------------------------
// IKK2  enabled = true parsed
// ---------------------------------------------------------------------------
static void IKK2() {
	auto doc = parse(
		"[analysis.ikk_end_tag]\n"
		"enabled = true\n"
	);
	assert(doc.pipeline_ikk_end_tag.enabled == true);
	std::puts("  IKK2  PASS  enabled=true parsed");
}

// ---------------------------------------------------------------------------
// IKK3  emit_markdown / emit_latex round-trip
// ---------------------------------------------------------------------------
static void IKK3() {
	auto doc = parse(
		"[analysis.ikk_end_tag]\n"
		"enabled       = true\n"
		"emit_markdown = false\n"
		"emit_latex    = true\n"
	);
	assert(doc.pipeline_ikk_end_tag.emit_markdown == false);
	assert(doc.pipeline_ikk_end_tag.emit_latex    == true);
	std::puts("  IKK3  PASS  emit_markdown=false emit_latex=true round-trip");
}

// ---------------------------------------------------------------------------
// IKK4  d_pass / d_warn threshold round-trip
// ---------------------------------------------------------------------------
static void IKK4() {
	auto doc = parse(
		"[analysis.ikk_end_tag]\n"
		"enabled          = true\n"
		"d_pass_threshold = 0.75\n"
		"d_warn_threshold = 0.40\n"
	);
	assert(doc.pipeline_ikk_end_tag.d_pass_threshold > 0.74);
	assert(doc.pipeline_ikk_end_tag.d_warn_threshold > 0.39);
	std::puts("  IKK4  PASS  d_pass=0.75 d_warn=0.40 round-trip");
}

// ---------------------------------------------------------------------------
// IKK5  Threshold guard: pass > warn enforced
// ---------------------------------------------------------------------------
static void IKK5() {
	// Setting warn == pass should cause parser to adjust one of them.
	auto doc = parse(
		"[analysis.ikk_end_tag]\n"
		"enabled          = true\n"
		"d_warn_threshold = 0.60\n"
		"d_pass_threshold = 0.60\n"
	);
	assert(doc.pipeline_ikk_end_tag.d_pass_threshold >
		   doc.pipeline_ikk_end_tag.d_warn_threshold);
	std::puts("  IKK5  PASS  threshold guard: pass > warn enforced");
}

// ---------------------------------------------------------------------------
// IKK6  scale_regime string round-trip
// ---------------------------------------------------------------------------
static void IKK6() {
	auto doc = parse(
		"[analysis.ikk_end_tag]\n"
		"enabled      = true\n"
		"scale_regime = \"S4\"\n"
	);
	assert(doc.pipeline_ikk_end_tag.scale_regime == "S4");
	std::puts("  IKK6  PASS  scale_regime=S4 round-trip");
}

// ---------------------------------------------------------------------------
// IKK7  section_reference string round-trip
// ---------------------------------------------------------------------------
static void IKK7() {
	auto doc = parse(
		"[analysis.ikk_end_tag]\n"
		"enabled           = true\n"
		"section_reference = \"IV.3\"\n"
	);
	assert(doc.pipeline_ikk_end_tag.section_reference == "IV.3");
	std::puts("  IKK7  PASS  section_reference=IV.3 round-trip");
}

// ---------------------------------------------------------------------------
// IKK8  badge() helper: PASS / WARN / FAIL boundary values
// ---------------------------------------------------------------------------
static void IKK8() {
	VsimIkkEndTagSection sec;
	sec.d_pass_threshold = 0.60;
	sec.d_warn_threshold = 0.35;

	assert(std::string(sec.badge(0.60)) == "PASS"); // exact pass boundary
	assert(std::string(sec.badge(0.80)) == "PASS"); // above pass
	assert(std::string(sec.badge(0.59)) == "WARN"); // just below pass
	assert(std::string(sec.badge(0.35)) == "WARN"); // exact warn boundary
	assert(std::string(sec.badge(0.34)) == "FAIL"); // below warn
	assert(std::string(sec.badge(0.00)) == "FAIL"); // zero
	std::puts("  IKK8  PASS  badge() PASS/WARN/FAIL boundaries");
}

// ---------------------------------------------------------------------------
// IKK9  wants_output() helper
// ---------------------------------------------------------------------------
static void IKK9() {
	VsimIkkEndTagSection sec;

	sec.enabled       = false;
	sec.emit_markdown = true;
	assert(sec.wants_output() == false); // disabled -> always false

	sec.enabled       = true;
	sec.emit_markdown = true;
	sec.emit_latex    = false;
	assert(sec.wants_output() == true);  // enabled + md on

	sec.emit_markdown = false;
	sec.emit_latex    = false;
	assert(sec.wants_output() == false); // enabled but both off

	sec.emit_latex = true;
	assert(sec.wants_output() == true);  // enabled + tex only
	std::puts("  IKK9  PASS  wants_output() logic");
}

// ---------------------------------------------------------------------------
// IKK10  build_ikk_end_tag() invalid when section disabled
// ---------------------------------------------------------------------------
static void IKK10() {
	auto doc     = parse("[analysis.ikk_end_tag]\nenabled = false\n");
	auto sidecar = make_sidecar(0.1, 0.05, 0.02, 0.01);
	IKKEndTag tag = build_ikk_end_tag(sidecar, doc);
	assert(tag.valid == false);
	std::puts("  IKK10 PASS  build returns invalid tag when disabled");
}

// ---------------------------------------------------------------------------
// IKK11  build_ikk_end_tag() invalid when sidecar empty
// ---------------------------------------------------------------------------
static void IKK11() {
	auto doc = parse("[analysis.ikk_end_tag]\nenabled = true\n");
	IdentitySidecarSeries empty;
	IKKEndTag tag = build_ikk_end_tag(empty, doc);
	assert(tag.valid == false);
	std::puts("  IKK11 PASS  build returns invalid tag for empty sidecar");
}

// ---------------------------------------------------------------------------
// IKK12  D_rec = 1 - dataloss
// ---------------------------------------------------------------------------
static void IKK12() {
	auto doc     = parse("[analysis.ikk_end_tag]\nenabled = true\n");
	auto sidecar = make_sidecar(0.17, 0.0, 0.0, 0.0);
	IKKEndTag tag = build_ikk_end_tag(sidecar, doc);
	assert(tag.valid);
	assert(tag.D_rec > 0.829 && tag.D_rec < 0.831); // 1.0 - 0.17 = 0.83
	std::puts("  IKK12 PASS  D_rec = 1 - dataloss");
}

// ---------------------------------------------------------------------------
// IKK13  eta_ab = 1 - projection_loss
// ---------------------------------------------------------------------------
static void IKK13() {
	auto doc     = parse("[analysis.ikk_end_tag]\nenabled = true\n");
	auto sidecar = make_sidecar(0.0, 0.09, 0.0, 0.0);
	IKKEndTag tag = build_ikk_end_tag(sidecar, doc);
	assert(tag.valid);
	assert(tag.eta_ab > 0.909 && tag.eta_ab < 0.911); // 1.0 - 0.09 = 0.91
	std::puts("  IKK13 PASS  eta_ab = 1 - projection_loss");
}

// ---------------------------------------------------------------------------
// IKK14  Delta_D = 0 for single-frame sidecar
// ---------------------------------------------------------------------------
static void IKK14() {
	auto doc     = parse("[analysis.ikk_end_tag]\nenabled = true\n");
	auto sidecar = make_sidecar(0.2, 0.0, 0.0, 0.0);
	IKKEndTag tag = build_ikk_end_tag(sidecar, doc);
	assert(tag.valid);
	assert(tag.Delta_D == 0.0);
	std::puts("  IKK14 PASS  Delta_D = 0 for single-frame sidecar");
}

// ---------------------------------------------------------------------------
// IKK15  Delta_D computed for multi-frame sidecar
// ---------------------------------------------------------------------------
static void IKK15() {
	auto doc = parse("[analysis.ikk_end_tag]\nenabled = true\n");

	IdentitySidecarSeries s;
	s.run_id = "multi_frame";

	// Frame 0: dataloss 0.30 -> D = 0.70
	IdentitySidecarRecord f0; f0.frame_index = 0; f0.dataloss = 0.30; s.push_frame(f0);
	// Frame 1: dataloss 0.20 -> D = 0.80
	IdentitySidecarRecord f1; f1.frame_index = 1; f1.dataloss = 0.20; s.push_frame(f1);
	// Frame 2: dataloss 0.10 -> D = 0.90
	IdentitySidecarRecord f2; f2.frame_index = 2; f2.dataloss = 0.10; s.push_frame(f2);
	s.build_summary();

	IKKEndTag tag = build_ikk_end_tag(s, doc);
	assert(tag.valid);
	// Delta_D = (0.90 - 0.70) / (3 - 1) = 0.10
	assert(tag.Delta_D > 0.099 && tag.Delta_D < 0.101);
	std::puts("  IKK15 PASS  Delta_D computed from multi-frame sidecar");
}

// ---------------------------------------------------------------------------
// IKK16  render_ikk_end_tag_md() empty for invalid tag
// ---------------------------------------------------------------------------
static void IKK16() {
	IKKEndTag invalid; invalid.valid = false;
	assert(render_ikk_end_tag_md(invalid).empty());
	std::puts("  IKK16 PASS  render_md returns empty for invalid tag");
}

// ---------------------------------------------------------------------------
// IKK17  render_ikk_end_tag_md() contains badge and key fields
// ---------------------------------------------------------------------------
static void IKK17() {
	IKKEndTag tag;
	tag.valid             = true;
	tag.D_rec             = 0.83;
	tag.eta_ab            = 0.91;
	tag.Psi_hid           = 0.13;
	tag.Delta_S           = 0.021;
	tag.Delta_D           = -0.003;
	tag.scale_regime      = "S3";
	tag.section_reference = "IV.3";
	tag.badge             = "PASS";
	tag.run_id            = "nacl_test";

	std::string md = render_ikk_end_tag_md(tag);
	assert(!md.empty());
	assert(md.find("IKK End-Tag")  != std::string::npos);
	assert(md.find("PASS")         != std::string::npos);
	assert(md.find("S3")           != std::string::npos);
	assert(md.find("IV.3")         != std::string::npos);
	assert(md.find("nacl_test")    != std::string::npos);
	std::puts("  IKK17 PASS  render_md contains badge + key fields");
}

// ---------------------------------------------------------------------------
// IKK18  render_ikk_end_tag_tex() empty for invalid tag
// ---------------------------------------------------------------------------
static void IKK18() {
	IKKEndTag invalid; invalid.valid = false;
	assert(render_ikk_end_tag_tex(invalid).empty());
	std::puts("  IKK18 PASS  render_tex returns empty for invalid tag");
}

// ---------------------------------------------------------------------------
// IKK19  render_ikk_end_tag_tex() contains \ikkendsection macro
// ---------------------------------------------------------------------------
static void IKK19() {
	IKKEndTag tag;
	tag.valid   = true;
	tag.D_rec   = 0.83;
	tag.eta_ab  = 0.91;
	tag.Psi_hid = 0.13;
	tag.Delta_S = 0.021;
	tag.Delta_D = -0.003;
	tag.badge   = "WARN";

	std::string tex = render_ikk_end_tag_tex(tag);
	assert(!tex.empty());
	assert(tex.find("\\ikkendsection") != std::string::npos);
	assert(tex.find("WARN")            != std::string::npos);
	assert(tex.find("D=")              != std::string::npos);
	assert(tex.find("etaab=")          != std::string::npos);
	std::puts("  IKK19 PASS  render_tex contains \\ikkendsection macro");
}

// ---------------------------------------------------------------------------
// IKK20  Full parser round-trip: complete [analysis.ikk_end_tag] block
// ---------------------------------------------------------------------------
static void IKK20() {
	auto doc = parse(
		"[project]\n"
		"name    = \"nacl_ikk_test\"\n"
		"version = \"v5.0.0\"\n"
		"\n"
		"[analysis.ikk_end_tag]\n"
		"enabled           = true\n"
		"emit_markdown     = true\n"
		"emit_latex        = true\n"
		"d_pass_threshold  = 0.70\n"
		"d_warn_threshold  = 0.40\n"
		"scale_regime      = \"S_mat\"\n"
		"section_reference = \"IV.7\"\n"
	);

	// Note: reference t used directly in all asserts below.
	// -Wunused-variable is a GCC false positive when -O3 inlines field accesses.
	const VsimIkkEndTagSection& t = doc.pipeline_ikk_end_tag;
	assert(t.enabled           == true);
	assert(t.emit_markdown     == true);
	assert(t.emit_latex        == true);
	assert(t.d_pass_threshold   > 0.69);
	assert(t.d_warn_threshold   > 0.39);
	assert(t.scale_regime      == "S_mat");
	assert(t.section_reference == "IV.7");
	assert(t.wants_output()    == true);
	std::puts("  IKK20 PASS  full [analysis.ikk_end_tag] round-trip");
}

// ---------------------------------------------------------------------------

int main() {
	std::puts("=== WO-75A  IKK End-Tag Enrichment  (Group 87) ===");
	IKK1();
	IKK2();
	IKK3();
	IKK4();
	IKK5();
	IKK6();
	IKK7();
	IKK8();
	IKK9();
	IKK10();
	IKK11();
	IKK12();
	IKK13();
	IKK14();
	IKK15();
	IKK16();
	IKK17();
	IKK18();
	IKK19();
	IKK20();
	std::puts("=== ALL PASS ===");
	return 0;
}
