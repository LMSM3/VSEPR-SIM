/**
 * test_vsim_output_filter.cpp  —  WO-73E / WO-74B  —  Group 79
 * ==============================================================
 * Tests for VsimOutputFilter: alias resolution, batch normalisation, CSV output.
 * WO-74B additions: IOutputFormatHandler registry, JSON/XYZ/xyzFull output,
 * scale-layer passthrough.
 */

#include "multiscale/vsim_output_filter.hpp"
#include "vsim/export/i_output_format_handler.hpp"
#include "vsim/vsim_document.hpp"
#include "vsim/module_registry.hpp"
#include <cstdio>
#include <cmath>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace vsepr::multiscale;

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond)                                                         \
	do {                                                                    \
		++tests_run;                                                        \
		if (cond) { ++tests_passed; }                                       \
		else { std::printf("FAIL  line %d: %s\n", __LINE__, #cond); }      \
	} while(0)

#define CHECK_NEAR(a, b, tol)                                               \
	do {                                                                    \
		++tests_run;                                                        \
		if (std::abs((double)(a) - (double)(b)) <= (tol)) { ++tests_passed; }\
		else { std::printf("FAIL  line %d: %.6f != %.6f\n",                \
			__LINE__, (double)(a), (double)(b)); }                          \
	} while(0)

// ============================================================================
// [1] Canonical field names pass through unchanged
// ============================================================================

static void test_canonical_fields()
{
	VsimRawRecord raw;
	raw.run_id    = "SIM_RUN_0001";
	raw.object_id = "pi_7";
	raw.fields["Lambda_nm"] = 1.18;
	raw.fields["E_value"]   = -12.4;
	raw.fields["UFF"]       = -10.1;
	raw.fields["CFF"]       = 0.71;
	raw.tags["family"]        = "pi_N";
	raw.tags["state"]         = "biological_pocket";
	raw.tags["casting"]       = "field_field";
	raw.tags["primary_scale"] = "S3";

	VsimOutputFilter f;
	auto rec = f.normalise(raw);

	CHECK(rec.valid);
	CHECK(rec.source_type == "simulation");
	CHECK(rec.gap_label   == "pending");
	CHECK(rec.object_id   == "pi_7");
	CHECK_NEAR(rec.Lambda_nm, 1.18, 1e-6);
	CHECK_NEAR(rec.E_value, -12.4, 1e-6);
	CHECK_NEAR(rec.UFF, -10.1, 1e-6);
	CHECK_NEAR(rec.CFF, 0.71,  1e-6);
	CHECK(rec.family        == "pi_N");
	CHECK(rec.state         == "biological_pocket");
	CHECK(rec.casting       == "field_field");
	CHECK(rec.primary_scale == "S3");
	CHECK(rec.run_id        == "SIM_RUN_0001");
}

// ============================================================================
// [2] Banned alias "scaleLength" is resolved to Lambda_nm
// ============================================================================

static void test_alias_scaleLength()
{
	VsimRawRecord raw;
	raw.run_id    = "SIM_ALIAS_001";
	raw.object_id = "HB_N";
	raw.fields["scaleLength"] = 0.28;
	raw.fields["E_value"]     = -25.0;

	VsimOutputFilter f;
	auto rec = f.normalise(raw);
	CHECK(rec.valid);
	CHECK_NEAR(rec.Lambda_nm, 0.28, 1e-6);
}

// ============================================================================
// [3] Banned alias "lscale"
// ============================================================================

static void test_alias_lscale()
{
	VsimRawRecord raw;
	raw.run_id    = "SIM_ALIAS_002";
	raw.object_id = "vdW_N";
	raw.fields["lscale"]  = 0.40;
	raw.fields["E_value"] = -4.5;

	VsimOutputFilter f;
	auto rec = f.normalise(raw);
	CHECK(rec.valid);
	CHECK_NEAR(rec.Lambda_nm, 0.40, 1e-6);
}

// ============================================================================
// [4] Banned alias "ideal_len"
// ============================================================================

static void test_alias_ideal_len()
{
	VsimRawRecord raw;
	raw.run_id    = "SIM_ALIAS_003";
	raw.object_id = "metal_ligand";
	raw.fields["ideal_len"] = 0.22;
	raw.fields["E_value"]   = -95.0;

	VsimOutputFilter f;
	auto rec = f.normalise(raw);
	CHECK(rec.valid);
	CHECK_NEAR(rec.Lambda_nm, 0.22, 1e-6);
}

// ============================================================================
// [5] Missing run_id -> invalid
// ============================================================================

static void test_missing_run_id()
{
	VsimRawRecord raw;
	raw.run_id    = "";
	raw.object_id = "pi_7";
	raw.fields["Lambda_nm"] = 1.18;

	VsimOutputFilter f;
	auto rec = f.normalise(raw);
	CHECK(!rec.valid);
}

// ============================================================================
// [6] Zero Lambda -> invalid
// ============================================================================

static void test_zero_lambda()
{
	VsimRawRecord raw;
	raw.run_id    = "SIM_001";
	raw.object_id = "pi_7";
	raw.fields["Lambda_nm"] = 0.0;

	VsimOutputFilter f;
	auto rec = f.normalise(raw);
	CHECK(!rec.valid);
}

// ============================================================================
// [7] Batch normalise: skips invalid, passes valid
// ============================================================================

static void test_batch_normalise()
{
	std::vector<VsimRawRecord> recs(3);
	// Valid
	recs[0].run_id = "R1"; recs[0].object_id = "pi_7";
	recs[0].fields["Lambda_nm"] = 1.18;
	// Invalid (no run_id)
	recs[1].run_id = ""; recs[1].object_id = "HB_N";
	recs[1].fields["Lambda_nm"] = 0.28;
	// Valid
	recs[2].run_id = "R2"; recs[2].object_id = "vdW_N";
	recs[2].fields["Lambda_nm"] = 0.40;

	VsimOutputFilter f;
	auto out = f.normalise_batch(recs);
	CHECK(out.size() == 2);
}

// ============================================================================
// [8] CSV output contains header and correct rows
// ============================================================================

static void test_csv_output()
{
	NormalisedRecord r;
	r.source_type   = "simulation";
	r.object_id     = "pi_7";
	r.family        = "pi_N";
	r.state         = "biological_pocket";
	r.casting       = "field_field";
	r.Lambda_nm     = 1.18;
	r.E_value       = -12.4;
	r.UFF           = -10.1;
	r.CFF           = 0.71;
	r.primary_scale = "S3";
	r.gap_label     = "pending";
	r.run_id        = "SIM_001";
	r.valid         = true;

	auto csv = VsimOutputFilter::to_csv({ r });
	// Must contain header keyword and data row keyword
	CHECK(csv.find("Lambda_nm") != std::string::npos);
	CHECK(csv.find("pi_7")      != std::string::npos);
	CHECK(csv.find("pending")   != std::string::npos);
}

// ============================================================================
// [9] WO-74B — IOutputFormatHandler registry: csv, json, xyz, xyzFull present
// ============================================================================

static void test_handler_registry()
{
	auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();
	CHECK(reg.has("csv"));
	CHECK(reg.has("json"));
	CHECK(reg.has("xyz"));
	CHECK(reg.has("xyzFull"));
}

// ============================================================================
// [10] WO-74B — JSON handler produces correct field tokens
// ============================================================================

static void test_json_handler_output()
{
	auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();
	auto h = reg.create("json");
	CHECK(h != nullptr);
	if (!h) return;

	NormalisedRecord r;
	r.source_type   = "simulation";
	r.object_id     = "pi_7";
	r.family        = "pi_N";
	r.state         = "biological_pocket";
	r.casting       = "field_field";
	r.Lambda_nm     = 1.18;
	r.E_value       = -12.4;
	r.UFF           = -10.1;
	r.CFF           = 0.71;
	r.primary_scale = "S3";
	r.gap_label     = "pending";
	r.run_id        = "SIM_001";
	r.valid         = true;

	// Write to a temp directory and read back
	const fs::path tmp = fs::temp_directory_path() / "vsim_test_json";
	fs::remove_all(tmp);

	vsim::ExportSection exp;
	exp.write_analysis_json = true;
	h->write(r, exp, tmp);

	const fs::path out = tmp / "vsim_output.json";
	CHECK(fs::exists(out));

	// Read content and verify key tokens
	{
		std::ifstream f(out);
		std::string content((std::istreambuf_iterator<char>(f)),
							 std::istreambuf_iterator<char>());
		CHECK(content.find("pi_7")      != std::string::npos);
		CHECK(content.find("pi_N")      != std::string::npos);
		CHECK(content.find("Lambda_nm") != std::string::npos);
		CHECK(content.find("1.1800")    != std::string::npos);
	}

	fs::remove_all(tmp);
}

// ============================================================================
// [11] WO-74B — XYZ handler produces atom-count header and object_id column
// ============================================================================

static void test_xyz_handler_output()
{
	auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();
	auto h = reg.create("xyz");
	CHECK(h != nullptr);
	if (!h) return;

	NormalisedRecord r;
	r.object_id     = "HB_N";
	r.Lambda_nm     = 0.28;
	r.E_value       = -25.0;
	r.UFF           = -20.0;
	r.run_id        = "SIM_002";
	r.gap_label     = "good_match";
	r.valid         = true;

	const fs::path tmp = fs::temp_directory_path() / "vsim_test_xyz";
	fs::remove_all(tmp);

	vsim::ExportSection exp;
	exp.write_xyz = true;
	h->write(r, exp, tmp);

	const fs::path out = tmp / "vsim_output.xyz";
	CHECK(fs::exists(out));

	{
		std::ifstream f(out);
		std::string content((std::istreambuf_iterator<char>(f)),
							 std::istreambuf_iterator<char>());
		CHECK(content.find("1")     != std::string::npos);  // atom count
		CHECK(content.find("HB_N") != std::string::npos);
	}

	fs::remove_all(tmp);
}

// ============================================================================
// [12] WO-74B — xyzFull handler includes extended fields in output
// ============================================================================

static void test_xyzfull_handler_output()
{
	auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();
	auto h = reg.create("xyzFull");
	CHECK(h != nullptr);
	if (!h) return;

	NormalisedRecord r;
	r.object_id     = "vdW_N";
	r.Lambda_nm     = 0.40;
	r.E_value       = -4.5;
	r.UFF           = -3.8;
	r.CFF           = 0.55;
	r.primary_scale = "S2";
	r.gap_label     = "lambda_high";
	r.family        = "vdW";
	r.state         = "gas";
	r.casting       = "field_field";
	r.run_id        = "SIM_003";
	r.source_type   = "simulation";
	r.valid         = true;

	const fs::path tmp = fs::temp_directory_path() / "vsim_test_xyzfull";
	fs::remove_all(tmp);

	vsim::ExportSection exp;
	exp.write_xyzfull = true;
	h->write(r, exp, tmp);

	const fs::path out = tmp / "vsim_output.xyzFull";
	CHECK(fs::exists(out));

	{
		std::ifstream f(out);
		std::string content((std::istreambuf_iterator<char>(f)),
							 std::istreambuf_iterator<char>());
		CHECK(content.find("properties")  != std::string::npos);
		CHECK(content.find("vdW_N")       != std::string::npos);
		CHECK(content.find("lambda_high") != std::string::npos);
	}

	fs::remove_all(tmp);
}

// ============================================================================
// [13] WO-74B — scale_layer() returns "" for all four handlers (all-layer pass)
// ============================================================================

static void test_handler_scale_layer_passthrough()
{
	auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();
	for (std::string_view key : { "csv", "json", "xyz", "xyzFull" }) {
		auto h = reg.create(key);
		CHECK(h != nullptr);
		if (!h) continue;
		CHECK(h->scale_layer() == "");
	}
}

// ============================================================================
// [14] WO-74B — handles() respects ExportSection flags
// ============================================================================

static void test_handler_flag_gating()
{
	auto& reg = vsim::ModuleRegistry<IOutputFormatHandler>::get();

	// All handlers off — use real ExportSection defaults
	vsim::ExportSection all_off;
	all_off.write_summary_csv   = false;
	all_off.write_analysis_json = false;
	all_off.write_xyz           = false;
	all_off.write_xyzfull       = false;

	vsim::ExportSection csv_on;  csv_on.write_summary_csv   = true;
	vsim::ExportSection json_on; json_on.write_analysis_json = true;
	vsim::ExportSection xyz_on;  xyz_on.write_xyz            = true;
	vsim::ExportSection xyzf_on; xyzf_on.write_xyzfull       = true;

	{
		auto h = reg.create("csv");
		CHECK(h && !h->handles(all_off));
		CHECK(h &&  h->handles(csv_on));
	}
	{
		auto h = reg.create("json");
		CHECK(h && !h->handles(all_off));
		CHECK(h &&  h->handles(json_on));
	}
	{
		auto h = reg.create("xyz");
		CHECK(h && !h->handles(all_off));
		CHECK(h &&  h->handles(xyz_on));
	}
	{
		auto h = reg.create("xyzFull");
		CHECK(h && !h->handles(all_off));
		CHECK(h &&  h->handles(xyzf_on));
	}
}

// ============================================================================
// main
// ============================================================================

int main()
{
	std::printf("=== test_vsim_output_filter (WO-73E / WO-74B Group 79) ===\n");

	test_canonical_fields();
	test_alias_scaleLength();
	test_alias_lscale();
	test_alias_ideal_len();
	test_missing_run_id();
	test_zero_lambda();
	test_batch_normalise();
	test_csv_output();
	test_handler_registry();
	test_json_handler_output();
	test_xyz_handler_output();
	test_xyzfull_handler_output();
	test_handler_scale_layer_passthrough();
	test_handler_flag_gating();

	std::printf("\n%d / %d tests passed\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
