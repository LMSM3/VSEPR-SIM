/**
 * tests/test_output_format.cpp
 * =====================================
 * Group 94  |  WO-85B  |  output-format selection + relative reactivity
 *
 * Unit tests covering the Day 85B viewer redesign:
 *
 *   Part 1  -  FeaturesSection::normalize()
 *     - single letters, mixed case, and prefixed tokens map to A/B/C/D
 *     - the reserved "ideal" token (WO-85G) is preserved
 *     - unknown tokens degrade gracefully to the default "A"
 *
 *   Part 2  -  parser routing for [features] / [features.outputformat]
 *     - [features] output_format = "B" selects Format B
 *     - the [features.outputformat] alias with mode = "D" selects Format D
 *     - absence of the section leaves the non-breaking default ("A", not present)
 *
 *   Part 3  -  relative_reactivity.hpp inference module (Part E)
 *     - the first observation has no derivative reference (frame.first)
 *     - a decaying species yields a negative rate and non-zero turnover
 *     - relative reactivity is normalised to [0,1] against the running peak
 *     - the dominant species is the fastest-changing one
 */

#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_document.hpp"
#include "vsim/relative_reactivity.hpp"

#include <cassert>
#include <cmath>
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

static bool approx(double a, double b, double eps = 1e-9)
{
	return std::fabs(a - b) <= eps;
}

// ---------------------------------------------------------------------------
// Part 1: FeaturesSection::normalize()
// ---------------------------------------------------------------------------
static void test_normalize_letters()
{
	using F = vsim::FeaturesSection;
	check(F::normalize("A") == "A", "normalize: 'A' -> A");
	check(F::normalize("b") == "B", "normalize: 'b' -> B");
	check(F::normalize("C") == "C", "normalize: 'C' -> C");
	check(F::normalize("d") == "D", "normalize: 'd' -> D");
}

static void test_normalize_prefixed()
{
	using F = vsim::FeaturesSection;
	check(F::normalize("format_b") == "B", "normalize: 'format_b' -> B");
	check(F::normalize("formatB")  == "B", "normalize: 'formatB' -> B");
	check(F::normalize("mode_d")   == "D", "normalize: 'mode_d' -> D");
}

static void test_normalize_reserved_and_unknown()
{
	using F = vsim::FeaturesSection;
	check(F::normalize("ideal")  == "ideal", "normalize: 'ideal' preserved (WO-85G)");
	check(F::normalize("IDEAL")  == "ideal", "normalize: 'IDEAL' case-folded to ideal");
	check(F::normalize("xyz123") == "A",     "normalize: unknown -> default A");
	check(F::normalize("")       == "A",     "normalize: empty -> default A");
}

// ---------------------------------------------------------------------------
// Part 2: parser routing for [features] / [features.outputformat]
// ---------------------------------------------------------------------------
static void test_features_section_selects_b()
{
	auto doc = parse(
		"[features]\n"
		"output_format = \"B\"\n");
	check(doc.features.present,               "features: [features] marks present");
	check(doc.features.output_format == "B",  "features: output_format = B");
	check(doc.features.is_format_b(),         "features: is_format_b() true");
}

static void test_features_outputformat_alias_selects_d()
{
	auto doc = parse(
		"[features.outputformat]\n"
		"mode = \"D\"\n");
	check(doc.features.present,               "features.outputformat: marks present");
	check(doc.features.output_format == "D",  "features.outputformat: mode = D");
	check(doc.features.is_format_d(),         "features.outputformat: is_format_d() true");
}

static void test_features_default_when_absent()
{
	auto doc = parse(
		"[project]\n"
		"name = plain\n");
	check(!doc.features.present,              "features: absent -> not present");
	check(doc.features.output_format == "A", "features: absent -> default A");
	check(doc.features.is_default(),         "features: is_default() true");
}

// ---------------------------------------------------------------------------
// Part 3: relative_reactivity.hpp inference module
// ---------------------------------------------------------------------------
static void test_reactivity_first_frame_has_no_derivative()
{
	vsim::reactivity::RelativeReactivity rr;
	auto f = rr.observe({ { "CH4", 100.0 } }, 1.0);
	check(f.first,                    "reactivity: first observation flagged first");
	check(approx(f.turnover, 0.0),    "reactivity: first turnover is 0 (no prior)");
	check(approx(f.reactivity, 0.0),  "reactivity: first reactivity is 0");
}

static void test_reactivity_decay_gives_negative_rate()
{
	vsim::reactivity::RelativeReactivity rr;
	rr.observe({ { "CH4", 100.0 } }, 1.0);           // prime
	auto f = rr.observe({ { "CH4", 90.0 } }, 1.0);   // decayed by 10 / step

	check(!f.first,                        "reactivity: second observation not first");
	check(f.species.size() == 1,           "reactivity: one species tracked");
	check(f.species.size() == 1 &&
		  approx(f.species[0].rate, -10.0),"reactivity: dx/dt = -10 for decay");
	check(approx(f.turnover, 10.0),        "reactivity: turnover = |dx/dt| = 10");
	check(approx(f.reactivity, 1.0),       "reactivity: first non-zero step is peak (1.0)");
	check(f.dominant == "CH4",             "reactivity: dominant species is CH4");
}

static void test_reactivity_normalised_to_running_peak()
{
	vsim::reactivity::RelativeReactivity rr;
	rr.observe({ { "CH4", 100.0 } }, 1.0);           // prime
	auto fast = rr.observe({ { "CH4", 80.0 } }, 1.0);// turnover 20 -> peak
	auto slow = rr.observe({ { "CH4", 75.0 } }, 1.0);// turnover 5  -> 5/20 = 0.25

	check(approx(fast.reactivity, 1.0),   "reactivity: steepest step normalises to 1.0");
	check(approx(rr.peak_turnover(), 20.0),"reactivity: running peak retained at 20");
	check(approx(slow.reactivity, 0.25),  "reactivity: later gentler step -> 0.25");
}

static void test_reactivity_dominant_is_fastest()
{
	vsim::reactivity::RelativeReactivity rr;
	// Prime with a CH4 combustion-style snapshot.
	rr.observe({ { "CH4", 50.0 }, { "O2", 50.0 }, { "CO2", 0.0 }, { "H2O", 0.0 } }, 1.0);
	// O2 falls fastest (by 20); CH4 falls by 10; products rise.
	auto f = rr.observe({ { "CH4", 40.0 }, { "O2", 30.0 }, { "CO2", 15.0 }, { "H2O", 15.0 } }, 1.0);

	check(f.dominant == "O2",          "reactivity: dominant is fastest-changing (O2)");
	check(f.dominant_rate < 0.0,       "reactivity: dominant O2 rate is negative");
	// turnover = |−10| + |−20| + |15| + |15| = 60
	check(approx(f.turnover, 60.0),    "reactivity: multi-species turnover summed");
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
	std::printf("\n=== Group 94: output-format selection + relative reactivity (WO-85B) ===\n\n");

	// Part 1
	test_normalize_letters();
	test_normalize_prefixed();
	test_normalize_reserved_and_unknown();

	// Part 2
	test_features_section_selects_b();
	test_features_outputformat_alias_selects_d();
	test_features_default_when_absent();

	// Part 3
	test_reactivity_first_frame_has_no_derivative();
	test_reactivity_decay_gives_negative_rate();
	test_reactivity_normalised_to_running_peak();
	test_reactivity_dominant_is_fastest();

	std::printf("\n  Result: %d PASS / %d FAIL\n", _pass, _fail);
	if (_fail == 0)
		std::printf("  PASS: all Group 94 output-format checks satisfied\n\n");
	return (_fail == 0) ? 0 : 1;
}
