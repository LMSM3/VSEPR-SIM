// =============================================================================
// tests/test_ikk_identity_vector.cpp   WO-75B Phase 1  (Group 88)
// =============================================================================
// IV1   Default IKKIdentityVector: all components zero, mag = 0
// IV2   Named accessors x/y/z/t/w return correct components
// IV3   Axis order is non-permutable: index 0=x,1=y,2=z,3=t,4=w
// IV4   mag() = 1.0 when all components = 1.0
// IV5   mag() = 0.0 when all components = 0.0
// IV6   dist() == 0 for identical vectors
// IV7   dist() correct for known pair
// IV8   operator+= accumulates correctly
// IV9   operator-= subtracts correctly
// IV10  operator*= scales all components
// IV11  from_sidecar_record(): zero sidecar -> x=1,y=1,z=0,t=1,w=1
// IV12  from_sidecar_record(): full-loss sidecar -> all components near 0
// IV13  from_sidecar_record(): clamped -- no component < 0 or > 1
// IV14  build_ivec_series(): empty sidecar returns empty series
// IV15  build_ivec_series(): frame_count matches sidecar frame count
// IV16  build_ivec_series(): frame 0 delta_valid = false
// IV17  build_ivec_series(): frame 1+ delta_valid = true (Phase 1 N_f=1)
// IV18  build_ivec_series(): run_mean is per-component average of frame means
// IV19  build_ivec_series(): drift_valid = false for single-frame series
// IV20  build_ivec_series(): drift_valid = true for two-frame series
// IV21  write_identity_json(): creates a non-empty file
// IV22  write_identity_json(): JSON contains run_id field
// IV23  write_identity_json(): JSON contains correct frame_count
// IV24  Parser round-trip: [analysis.ivec] enabled = true
// IV25  Parser round-trip: [analysis.ivec] write_json = false
// IV26  Parser round-trip: [analysis.ivec] include_delta = false
// IV27  Parser round-trip: [analysis.ivec] include_var = true
// IV28  Parser round-trip: [analysis.ivec] output_dir set
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"
#include "include/vsim/analysis/ikk_identity_vector.hpp"
#include "include/vsim/analysis/identity_sidecar.hpp"

using vsim::VsimDocument;
using vsim::VsimParser;
using vsim::analysis::IKKIdentityVector;
using vsim::analysis::IKKIdentityFrameRecord;
using vsim::analysis::IKKIdentitySeries;
using vsim::analysis::IdentitySidecarRecord;
using vsim::analysis::IdentitySidecarSeries;
using vsim::analysis::from_sidecar_record;
using vsim::analysis::build_ivec_series;
using vsim::analysis::write_identity_json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static VsimDocument parse(const char* script) {
	return VsimParser::parse_string(script, "<test>");
}

static IdentitySidecarSeries make_sidecar_n(int n,
											 double dataloss,
											 double proj_loss,
											 double hidden,
											 double recov,
											 double id_res) {
	IdentitySidecarSeries s;
	s.run_id = "test_run";
	for (int i = 0; i < n; ++i) {
		IdentitySidecarRecord r;
		r.frame_index      = i;
		r.time_fs          = static_cast<double>(i) * 1.0;
		r.dataloss         = dataloss;
		r.projection_loss  = proj_loss;
		r.hidden_channel   = hidden;
		r.recoverable_info = recov;
		r.identity_residual= id_res;
		s.push_frame(r);
	}
	s.build_summary();
	return s;
}

static bool near(float a, float b, float tol = 1e-5f) {
	return std::fabs(a - b) < tol;
}

// ---------------------------------------------------------------------------
// IV1   Default IKKIdentityVector: all components zero, mag = 0
// ---------------------------------------------------------------------------
static void IV1() {
	IKKIdentityVector v{};
	for (int i = 0; i < 5; ++i) assert(v[i] == 0.f);
	assert(v.mag() == 0.f);
	printf("IV1  PASS\n");
}

// ---------------------------------------------------------------------------
// IV2   Named accessors return correct components
// ---------------------------------------------------------------------------
static void IV2() {
	IKKIdentityVector v;
	v.x() = 0.1f; v.y() = 0.2f; v.z() = 0.3f; v.t() = 0.4f; v.w() = 0.5f;
	assert(v[0] == 0.1f);
	assert(v[1] == 0.2f);
	assert(v[2] == 0.3f);
	assert(v[3] == 0.4f);
	assert(v[4] == 0.5f);
	printf("IV2  PASS\n");
}

// ---------------------------------------------------------------------------
// IV3   Axis order is non-permutable: index 0=x,1=y,2=z,3=t,4=w
// ---------------------------------------------------------------------------
static void IV3() {
	IKKIdentityVector v;
	v.v[0] = 10.f; v.v[1] = 20.f; v.v[2] = 30.f; v.v[3] = 40.f; v.v[4] = 50.f;
	assert(v.x() == 10.f && v.y() == 20.f && v.z() == 30.f
		   && v.t() == 40.f && v.w() == 50.f);
	printf("IV3  PASS\n");
}

// ---------------------------------------------------------------------------
// IV4   mag() = 1.0 when all components = 1.0
// ---------------------------------------------------------------------------
static void IV4() {
	IKKIdentityVector v;
	for (int i = 0; i < 5; ++i) v.v[i] = 1.f;
	assert(near(v.mag(), 1.f));
	printf("IV4  PASS\n");
}

// ---------------------------------------------------------------------------
// IV5   mag() = 0.0 when all components = 0.0
// ---------------------------------------------------------------------------
static void IV5() {
	IKKIdentityVector v{};
	assert(v.mag() == 0.f);
	printf("IV5  PASS\n");
}

// ---------------------------------------------------------------------------
// IV6   dist() == 0 for identical vectors
// ---------------------------------------------------------------------------
static void IV6() {
	IKKIdentityVector v;
	v.x() = 0.5f; v.y() = 0.3f; v.z() = 0.8f; v.t() = 0.1f; v.w() = 0.9f;
	assert(v.dist(v) == 0.f);
	printf("IV6  PASS\n");
}

// ---------------------------------------------------------------------------
// IV7   dist() correct for known pair
// ---------------------------------------------------------------------------
static void IV7() {
	IKKIdentityVector a{}, b{};
	a.x() = 1.f;           // difference in x only = 1
	// ||a-b||_2 = 1
	assert(near(a.dist(b), 1.f));
	printf("IV7  PASS\n");
}

// ---------------------------------------------------------------------------
// IV8   operator+= accumulates correctly
// ---------------------------------------------------------------------------
static void IV8() {
	IKKIdentityVector a, b;
	a.x() = 0.2f; b.x() = 0.3f;
	a += b;
	assert(near(a.x(), 0.5f));
	printf("IV8  PASS\n");
}

// ---------------------------------------------------------------------------
// IV9   operator-= subtracts correctly
// ---------------------------------------------------------------------------
static void IV9() {
	IKKIdentityVector a, b;
	a.x() = 0.8f; b.x() = 0.3f;
	a -= b;
	assert(near(a.x(), 0.5f));
	printf("IV9  PASS\n");
}

// ---------------------------------------------------------------------------
// IV10  operator*= scales all components
// ---------------------------------------------------------------------------
static void IV10() {
	IKKIdentityVector v;
	for (int i = 0; i < 5; ++i) v.v[i] = 1.f;
	v *= 0.5f;
	for (int i = 0; i < 5; ++i) assert(near(v.v[i], 0.5f));
	printf("IV10 PASS\n");
}

// ---------------------------------------------------------------------------
// IV11  from_sidecar_record(): zero-loss sidecar -> x=1,y=1,z=0,t=1,w=1
// ---------------------------------------------------------------------------
static void IV11() {
	IdentitySidecarRecord r;
	r.dataloss          = 0.0;
	r.hidden_channel    = 0.0;
	r.recoverable_info  = 0.0;
	r.projection_loss   = 0.0;
	r.identity_residual = 0.0;
	IKKIdentityVector iv = from_sidecar_record(r);
	assert(near(iv.x(), 1.f));   // 1 - dataloss = 1
	assert(near(iv.y(), 1.f));   // 1 - hidden = 1
	assert(near(iv.z(), 0.f));   // recoverable = 0
	assert(near(iv.t(), 1.f));   // 1 - proj_loss = 1
	assert(near(iv.w(), 1.f));   // 1 - id_res = 1
	printf("IV11 PASS\n");
}

// ---------------------------------------------------------------------------
// IV12  from_sidecar_record(): full-loss sidecar -> all near 0
// ---------------------------------------------------------------------------
static void IV12() {
	IdentitySidecarRecord r;
	r.dataloss          = 1.0;
	r.hidden_channel    = 1.0;
	r.recoverable_info  = 0.0;
	r.projection_loss   = 1.0;
	r.identity_residual = 1.0;
	IKKIdentityVector iv = from_sidecar_record(r);
	for (int a = 0; a < 5; ++a) assert(near(iv.v[a], 0.f));
	printf("IV12 PASS\n");
}

// ---------------------------------------------------------------------------
// IV13  from_sidecar_record(): clamped -- no component < 0 or > 1
// ---------------------------------------------------------------------------
static void IV13() {
	IdentitySidecarRecord r;
	r.dataloss          = -0.5;   // would give x > 1 without clamp
	r.hidden_channel    =  2.0;   // would give y < 0 without clamp
	r.recoverable_info  = -1.0;   // would give z < 0
	r.projection_loss   =  3.0;   // would give t < 0
	r.identity_residual = -0.2;   // would give w > 1
	IKKIdentityVector iv = from_sidecar_record(r);
	for (int a = 0; a < 5; ++a) {
		assert(iv.v[a] >= 0.f && iv.v[a] <= 1.f);
	}
	printf("IV13 PASS\n");
}

// ---------------------------------------------------------------------------
// IV14  build_ivec_series(): empty sidecar returns empty series
// ---------------------------------------------------------------------------
static void IV14() {
	IdentitySidecarSeries empty;
	IKKIdentitySeries out = build_ivec_series(empty);
	assert(out.frame_count() == 0);
	assert(!out.drift_valid);
	printf("IV14 PASS\n");
}

// ---------------------------------------------------------------------------
// IV15  build_ivec_series(): frame_count matches sidecar frame count
// ---------------------------------------------------------------------------
static void IV15() {
	auto sc = make_sidecar_n(5, 0.1, 0.1, 0.1, 0.8, 0.05);
	auto out = build_ivec_series(sc);
	assert(out.frame_count() == 5);
	printf("IV15 PASS\n");
}

// ---------------------------------------------------------------------------
// IV16  build_ivec_series(): frame 0 delta_valid = false
// ---------------------------------------------------------------------------
static void IV16() {
	auto sc = make_sidecar_n(3, 0.1, 0.1, 0.1, 0.8, 0.05);
	auto out = build_ivec_series(sc);
	assert(!out.frames[0].delta_valid);
	printf("IV16 PASS\n");
}

// ---------------------------------------------------------------------------
// IV17  build_ivec_series(): frame 1+ delta_valid = true (Phase 1, N_f=1)
// ---------------------------------------------------------------------------
static void IV17() {
	auto sc = make_sidecar_n(3, 0.1, 0.1, 0.1, 0.8, 0.05);
	auto out = build_ivec_series(sc);
	assert(out.frames[1].delta_valid);
	assert(out.frames[2].delta_valid);
	printf("IV17 PASS\n");
}

// ---------------------------------------------------------------------------
// IV18  build_ivec_series(): run_mean is per-component average of frame means
// ---------------------------------------------------------------------------
static void IV18() {
	// Two frames with different dataloss -> different I^x
	IdentitySidecarSeries sc;
	sc.run_id = "avg_test";
	{
		IdentitySidecarRecord r;
		r.frame_index = 0; r.dataloss = 0.0; r.recoverable_info = 0.4;
		r.hidden_channel = 0.0; r.projection_loss = 0.0; r.identity_residual = 0.0;
		sc.push_frame(r);
	}
	{
		IdentitySidecarRecord r;
		r.frame_index = 1; r.dataloss = 0.4; r.recoverable_info = 0.4;
		r.hidden_channel = 0.0; r.projection_loss = 0.0; r.identity_residual = 0.0;
		sc.push_frame(r);
	}
	sc.build_summary();
	auto out = build_ivec_series(sc);
	// frame 0: I^x = 1.0; frame 1: I^x = 0.6  => mean = 0.8
	assert(near(out.run_mean.x(), 0.8f, 1e-4f));
	printf("IV18 PASS\n");
}

// ---------------------------------------------------------------------------
// IV19  build_ivec_series(): drift_valid = false for single-frame series
// ---------------------------------------------------------------------------
static void IV19() {
	auto sc = make_sidecar_n(1, 0.1, 0.1, 0.1, 0.8, 0.05);
	auto out = build_ivec_series(sc);
	assert(!out.drift_valid);
	printf("IV19 PASS\n");
}

// ---------------------------------------------------------------------------
// IV20  build_ivec_series(): drift_valid = true for two-frame series
// ---------------------------------------------------------------------------
static void IV20() {
	auto sc = make_sidecar_n(2, 0.1, 0.1, 0.1, 0.8, 0.05);
	auto out = build_ivec_series(sc);
	assert(out.drift_valid);
	printf("IV20 PASS\n");
}

// ---------------------------------------------------------------------------
// IV21  write_identity_json(): creates a non-empty file
// ---------------------------------------------------------------------------
static void IV21() {
	auto sc  = make_sidecar_n(2, 0.05, 0.05, 0.05, 0.9, 0.02);
	auto out = build_ivec_series(sc);
	out.run_id = "iv21_run";
	const std::string tmp = "test_iv21_output.json";
	bool ok = write_identity_json(out, tmp);
	assert(ok);
	std::ifstream f(tmp);
	std::string content((std::istreambuf_iterator<char>(f)),
						 std::istreambuf_iterator<char>());
	assert(!content.empty());
	std::remove(tmp.c_str());
	printf("IV21 PASS\n");
}

// ---------------------------------------------------------------------------
// IV22  write_identity_json(): JSON contains run_id field
// ---------------------------------------------------------------------------
static void IV22() {
	auto sc  = make_sidecar_n(1, 0.0, 0.0, 0.0, 1.0, 0.0);
	auto out = build_ivec_series(sc);
	out.run_id = "run_iv22";
	const std::string tmp = "test_iv22_output.json";
	write_identity_json(out, tmp);
	std::ifstream f(tmp);
	std::string content((std::istreambuf_iterator<char>(f)),
						 std::istreambuf_iterator<char>());
	assert(content.find("run_iv22") != std::string::npos);
	std::remove(tmp.c_str());
	printf("IV22 PASS\n");
}

// ---------------------------------------------------------------------------
// IV23  write_identity_json(): JSON contains correct frame_count
// ---------------------------------------------------------------------------
static void IV23() {
	auto sc  = make_sidecar_n(7, 0.0, 0.0, 0.0, 1.0, 0.0);
	auto out = build_ivec_series(sc);
	const std::string tmp = "test_iv23_output.json";
	write_identity_json(out, tmp);
	std::ifstream f(tmp);
	std::string content((std::istreambuf_iterator<char>(f)),
						 std::istreambuf_iterator<char>());
	assert(content.find("\"frame_count\": 7") != std::string::npos);
	std::remove(tmp.c_str());
	printf("IV23 PASS\n");
}

// ---------------------------------------------------------------------------
// IV24  Parser: [analysis.ivec] enabled = true
// ---------------------------------------------------------------------------
static void IV24() {
	auto doc = parse(
		"[project]\n"
		"name = \"iv24\"\n"
		"[analysis.ivec]\n"
		"enabled = true\n"
	);
	assert(doc.pipeline_ivec.enabled == true);
	printf("IV24 PASS\n");
}

// ---------------------------------------------------------------------------
// IV25  Parser: [analysis.ivec] write_json = false
// ---------------------------------------------------------------------------
static void IV25() {
	auto doc = parse(
		"[project]\n"
		"name = \"iv25\"\n"
		"[analysis.ivec]\n"
		"enabled    = true\n"
		"write_json = false\n"
	);
	assert(!doc.pipeline_ivec.write_json);
	assert(!doc.pipeline_ivec.wants_output());
	printf("IV25 PASS\n");
}

// ---------------------------------------------------------------------------
// IV26  Parser: [analysis.ivec] include_delta = false
// ---------------------------------------------------------------------------
static void IV26() {
	auto doc = parse(
		"[project]\n"
		"name = \"iv26\"\n"
		"[analysis.ivec]\n"
		"enabled       = true\n"
		"include_delta = false\n"
	);
	assert(!doc.pipeline_ivec.include_delta);
	printf("IV26 PASS\n");
}

// ---------------------------------------------------------------------------
// IV27  Parser: [analysis.ivec] include_var = true
// ---------------------------------------------------------------------------
static void IV27() {
	auto doc = parse(
		"[project]\n"
		"name = \"iv27\"\n"
		"[analysis.ivec]\n"
		"enabled     = true\n"
		"include_var = true\n"
	);
	assert(doc.pipeline_ivec.include_var);
	printf("IV27 PASS\n");
}

// ---------------------------------------------------------------------------
// IV28  Parser: [analysis.ivec] output_dir set
// ---------------------------------------------------------------------------
static void IV28() {
	auto doc = parse(
		"[project]\n"
		"name = \"iv28\"\n"
		"[analysis.ivec]\n"
		"enabled    = true\n"
		"output_dir = \"out/identity\"\n"
	);
	assert(doc.pipeline_ivec.output_dir == "out/identity");
	printf("IV28 PASS\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	printf("=== Group 88 : IKK Identity Vector (WO-75B Phase 1) ===\n");
	IV1();  IV2();  IV3();  IV4();  IV5();
	IV6();  IV7();  IV8();  IV9();  IV10();
	IV11(); IV12(); IV13(); IV14(); IV15();
	IV16(); IV17(); IV18(); IV19(); IV20();
	IV21(); IV22(); IV23(); IV24(); IV25();
	IV26(); IV27(); IV28();
	printf("=== All IV tests passed ===\n");
	return 0;
}
