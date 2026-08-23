/**
 * tests/test_xbundle_smoke.cpp  —  Group 70: .X Bundle Format Smoke
 * ===================================================================
 * WO-72A  |  V5.1.4
 *
 * Full pipeline: write → serialise → read → validate
 *
 *   XB-01  write_string produces XBUNDLE magic header
 *   XB-02  write_string contains [manifest] block
 *   XB-03  write_string contains [[member]] block
 *   XB-04  round-trip: read_string recovers manifest.name
 *   XB-05  round-trip: read_string recovers entry name
 *   XB-06  round-trip: read_string recovers entry content
 *   XB-07  validate() passes on a well-formed bundle
 *   XB-08  validate() rejects bundle with no entries (V-03)
 *   XB-09  validate() rejects entry with empty name (V-04)
 *   XB-10  entry_point() returns first vsim entry when none specified
 */

#include "include/xbundle/xbundle_document.hpp"
#include "include/xbundle/xbundle_reader.hpp"
#include "include/xbundle/xbundle_writer.hpp"
#include "include/xbundle/xbundle_validator.hpp"

#include <iostream>
#include <string>

using namespace vsim::xbundle;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static XBundle make_bundle() {
	XBundle b;
	b.manifest.name        = "smoke_suite";
	b.manifest.description = "WO-72A smoke test suite";
	b.manifest.author      = "vsepr-sim";
	b.manifest.populated   = true;

	XBundleEntry e1;
	e1.name    = "run_a";
	e1.kind    = XBundleEntryKind::vsim;
	e1.content = "kernel.channel.reset()\nruntime.run_case(\"a\")\n";

	XBundleEntry e2;
	e2.name    = "run_b";
	e2.kind    = XBundleEntryKind::vsim;
	e2.content = "kernel.channel.reset()\nruntime.run_case(\"b\")\n";

	b.entries.push_back(e1);
	b.entries.push_back(e2);
	return b;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
int main() {
	// --- XB-01: magic header present ---
	{
		const auto text = XBundleWriter::write_string(make_bundle());
		CHECK("XB-01", text.find("XBUNDLE 1") != std::string::npos,
			  "write_string contains XBUNDLE magic");
	}

	// --- XB-02: [manifest] block present ---
	{
		const auto text = XBundleWriter::write_string(make_bundle());
		CHECK("XB-02", text.find("[manifest]") != std::string::npos,
			  "write_string contains [manifest] block");
	}

	// --- XB-03: [[member]] block present ---
	{
		const auto text = XBundleWriter::write_string(make_bundle());
		CHECK("XB-03", text.find("[[member]]") != std::string::npos,
			  "write_string contains [[member]] block");
	}

	// --- XB-04: round-trip manifest.name ---
	{
		const auto text   = XBundleWriter::write_string(make_bundle());
		const auto bundle = XBundleReader::read_string(text, "smoke.X");
		CHECK("XB-04", bundle.manifest.name == "smoke_suite",
			  "round-trip: manifest.name == \"smoke_suite\"");
	}

	// --- XB-05: round-trip entry name ---
	{
		const auto text   = XBundleWriter::write_string(make_bundle());
		const auto bundle = XBundleReader::read_string(text, "smoke.X");
		CHECK("XB-05", !bundle.entries.empty() &&
						bundle.entries[0].name == "run_a",
			  "round-trip: first entry name == \"run_a\"");
	}

	// --- XB-06: round-trip entry content ---
	{
		const auto text   = XBundleWriter::write_string(make_bundle());
		const auto bundle = XBundleReader::read_string(text, "smoke.X");
		bool ok = !bundle.entries.empty() &&
				  bundle.entries[0].content.find("run_case") != std::string::npos;
		CHECK("XB-06", ok,
			  "round-trip: entry content contains \"run_case\"");
	}

	// --- XB-07: validate() passes on well-formed bundle ---
	{
		const auto errs = XBundleValidator::validate(make_bundle());
		CHECK("XB-07", errs.empty(),
			  "validate() returns no errors for well-formed bundle");
	}

	// --- XB-08: validate() rejects bundle with no entries (V-03) ---
	{
		XBundle b;
		b.manifest.name      = "empty_suite";
		b.manifest.populated = true;
		const auto errs = XBundleValidator::validate(b);
		bool has_v03 = false;
		for (const auto& e : errs)
			if (e.find("V-03") != std::string::npos) { has_v03 = true; break; }
		CHECK("XB-08", has_v03,
			  "validate() emits V-03 for bundle with no entries");
	}

	// --- XB-09: validate() rejects entry with empty name (V-04) ---
	{
		XBundle b = make_bundle();
		b.entries[0].name = "";  // corrupt first entry name
		const auto errs = XBundleValidator::validate(b);
		bool has_v04 = false;
		for (const auto& e : errs)
			if (e.find("V-04") != std::string::npos) { has_v04 = true; break; }
		CHECK("XB-09", has_v04,
			  "validate() emits V-04 for entry with empty name");
	}

	// --- XB-10: entry_point() returns first vsim entry when none specified ---
	{
		const auto b = make_bundle();
		const auto* ep = b.entry_point();
		CHECK("XB-10", ep != nullptr && ep->name == "run_a",
			  "entry_point() returns first vsim entry \"run_a\" when not specified");
	}

	// ---------------------------------------------------------------------------
	std::cout << "\n" << PASS << "/" << (PASS + FAIL) << " tests passed";
	if (FAIL == 0) std::cout << "  (Group 70 OK)\n";
	else           std::cout << "  *** " << FAIL << " FAILURES ***\n";
	return FAIL == 0 ? 0 : 1;
}
