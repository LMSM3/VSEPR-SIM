/**
 * tests/test_print_console.cpp
 * =====================================
 * Group 93  |  WO-85A  |  print_console kernel primitive
 *
 * Unit tests for the `print_console "<message>"` VSIM directive:
 *   - quoted message parses and strips the surrounding quote pair
 *   - unquoted message parses verbatim (quotations optional)
 *   - inner whitespace is preserved inside a quoted message
 *   - empty payloads (`print_console` / `print_console ""`) emit blank lines
 *   - declaration order is preserved across multiple directives
 *   - trailing comments after an unquoted message are stripped
 *   - `#` inside a quoted message is preserved (quote-aware comment strip)
 *   - directives are recognised regardless of surrounding sections
 *   - key lines that merely start with `print_console` are NOT captured
 */

#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_document.hpp"

#include <cassert>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int _pass = 0;
static int _fail = 0;

static void check(bool cond, const char* label)
{
	if (cond) {
		++_pass;
		std::printf("  PASS: %s\n", label);
	} else {
		++_fail;
		std::printf("  FAIL: %s\n", label);
	}
}

static vsim::VsimDocument parse(const std::string& s)
{
	return vsim::VsimParser::parse_string(s);
}

// ---------------------------------------------------------------------------
// Test: quoted message strips the surrounding quotes
// ---------------------------------------------------------------------------
static void test_quoted_message()
{
	auto doc = parse("print_console \"Hello world\"\n");
	check(doc.console_prints.size() == 1,                 "quoted: one directive");
	check(doc.console_prints.size() == 1 &&
		  doc.console_prints[0].message == "Hello world", "quoted: quotes stripped");
}

// ---------------------------------------------------------------------------
// Test: unquoted message parses verbatim (quotations optional)
// ---------------------------------------------------------------------------
static void test_unquoted_message()
{
	auto doc = parse("print_console Hello world\n");
	check(doc.console_prints.size() == 1,                 "unquoted: one directive");
	check(doc.console_prints.size() == 1 &&
		  doc.console_prints[0].message == "Hello world", "unquoted: verbatim payload");
}

// ---------------------------------------------------------------------------
// Test: inner whitespace preserved inside quotes
// ---------------------------------------------------------------------------
static void test_inner_spacing_preserved()
{
	auto doc = parse("print_console \"a    b\"\n");
	check(doc.console_prints.size() == 1 &&
		  doc.console_prints[0].message == "a    b", "quoted: inner spacing preserved");
}

// ---------------------------------------------------------------------------
// Test: empty payloads emit blank messages
// ---------------------------------------------------------------------------
static void test_empty_payloads()
{
	auto doc = parse("print_console\nprint_console \"\"\n");
	check(doc.console_prints.size() == 2,             "empty: two directives");
	check(doc.console_prints.size() == 2 &&
		  doc.console_prints[0].message.empty() &&
		  doc.console_prints[1].message.empty(),      "empty: both blank");
}

// ---------------------------------------------------------------------------
// Test: declaration order preserved
// ---------------------------------------------------------------------------
static void test_order_preserved()
{
	auto doc = parse(
		"print_console first\n"
		"print_console second\n"
		"print_console third\n");
	check(doc.console_prints.size() == 3,                "order: three directives");
	check(doc.console_prints.size() == 3 &&
		  doc.console_prints[0].message == "first"  &&
		  doc.console_prints[1].message == "second" &&
		  doc.console_prints[2].message == "third",      "order: sequence preserved");
}

// ---------------------------------------------------------------------------
// Test: trailing comment stripped from an unquoted message
// ---------------------------------------------------------------------------
static void test_trailing_comment_stripped()
{
	auto doc = parse("print_console hello   # a trailing comment\n");
	check(doc.console_prints.size() == 1 &&
		  doc.console_prints[0].message == "hello", "comment: trailing comment removed");
}

// ---------------------------------------------------------------------------
// Test: '#' inside a quoted message is preserved
// ---------------------------------------------------------------------------
static void test_hash_in_quotes_preserved()
{
	auto doc = parse("print_console \"channel #42 ready\"\n");
	check(doc.console_prints.size() == 1 &&
		  doc.console_prints[0].message == "channel #42 ready",
		  "comment: '#' inside quotes preserved");
}

// ---------------------------------------------------------------------------
// Test: directives recognised alongside sections
// ---------------------------------------------------------------------------
static void test_recognised_with_sections()
{
	auto doc = parse(
		"print_console top\n"
		"[project]\n"
		"name = demo\n"
		"[formula]\n"
		"formula = CH4\n"
		"print_console bottom\n");
	check(doc.console_prints.size() == 2,                "sections: two directives");
	check(doc.console_prints.size() == 2 &&
		  doc.console_prints[0].message == "top" &&
		  doc.console_prints[1].message == "bottom",     "sections: both captured");
	check(doc.project.name == "demo",                    "sections: key still parsed");
}

// ---------------------------------------------------------------------------
// Test: a `print_console`-prefixed key assignment is NOT captured as a directive
// ---------------------------------------------------------------------------
static void test_prefixed_key_not_captured()
{
	// `print_console_note = ...` is a key line, not the directive.
	auto doc = parse(
		"[project]\n"
		"print_console_note = hi\n");
	check(doc.console_prints.empty(), "guard: prefixed key not captured as directive");
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
	std::printf("\n=== Group 93: print_console kernel primitive (WO-85A) ===\n\n");

	test_quoted_message();
	test_unquoted_message();
	test_inner_spacing_preserved();
	test_empty_payloads();
	test_order_preserved();
	test_trailing_comment_stripped();
	test_hash_in_quotes_preserved();
	test_recognised_with_sections();
	test_prefixed_key_not_captured();

	std::printf("\n  Result: %d PASS / %d FAIL\n", _pass, _fail);
	if (_fail == 0)
		std::printf("  PASS: all Group 93 print_console checks satisfied\n\n");
	return (_fail == 0) ? 0 : 1;
}
