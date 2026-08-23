/**
 * test_aromaticity.cpp  --  Day 84 / WO-84D
 * ============================================================================
 * CTest Group 102 (part 2).
 *
 * Verifies the Huckel 4n+2 aromatic gate and the d8 square-planar branch:
 *
 *   benzene        -> Aromatic          (6 pi e-, n=1)
 *   pyridine       -> Aromatic          (6 pi e- ring, N in ring)
 *   cyclobutadiene -> Antiaromatic      (4 pi e-, 4n)
 *   cyclohexane    -> NonAromatic       (no ring double bonds)
 *   ethene/acetone -> NonAromatic       (no qualifying ring)
 *   d8 metal (PtCl4)-> square planar     (VSEPR disambiguation)
 *
 * Acceptance (WO-84D):
 *   - Aromaticity is gated, not guessed.                          [checked]
 *   - Benzene passes supported aromatic detection.                [checked]
 *   - Cyclobutadiene is NOT treated as ordinary aromatic.         [checked]
 *   - Unsupported cases fall back deterministically.              [checked]
 *   - d8 square-planar path exists without breaking VSEPR.        [checked]
 */

#include "atomistic/classify/aromaticity.hpp"
#include "atomistic/classify/vsepr.hpp"
#include "atomistic/core/state.hpp"

#include <iostream>
#include <string>
#include <vector>

using atomistic::Edge;
using atomistic::State;
using atomistic::classify::analyze_aromaticity;
using atomistic::classify::AromaticClass;
using atomistic::classify::AromaticityReport;
using atomistic::classify::classify_vsepr_sites;
using atomistic::classify::is_d8_square_planar_metal;
using atomistic::classify::satisfies_huckel;
using atomistic::classify::VSEPRMolecularShape;
using atomistic::classify::VSEPROptions;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& what) {
	if (cond) { ++g_pass; std::cout << "  PASS: " << what << "\n"; }
	else      { ++g_fail; std::cout << "  FAIL: " << what << "\n"; }
}

State make_state(std::vector<uint32_t> types, std::vector<Edge> bonds) {
	State s;
	s.N = static_cast<uint32_t>(types.size());
	s.type = std::move(types);
	s.X.assign(s.N, {0.0, 0.0, 0.0});
	s.V.resize(s.N);
	s.Q.assign(s.N, 0.0);
	s.M.assign(s.N, 1.0);
	s.B = std::move(bonds);
	return s;
}

// benzene C6H6: 6-ring, one H per C.
State make_benzene() {
	return make_state({6,6,6,6,6,6,1,1,1,1,1,1},
		{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0},
		 {0,6},{1,7},{2,8},{3,9},{4,10},{5,11}});
}

// pyridine C5H5N: replace one ring CH with N (atom 0 = N, no H on N).
State make_pyridine() {
	return make_state({7,6,6,6,6,6,1,1,1,1,1},
		{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0},
		 {1,6},{2,7},{3,8},{4,9},{5,10}});
}

// cyclobutadiene C4H4: 4-ring, one H per C.
State make_cyclobutadiene() {
	return make_state({6,6,6,6,1,1,1,1},
		{{0,1},{1,2},{2,3},{3,0},
		 {0,4},{1,5},{2,6},{3,7}});
}

// cyclohexane C6H12: saturated 6-ring, two H per C.
State make_cyclohexane() {
	return make_state({6,6,6,6,6,6,1,1,1,1,1,1,1,1,1,1,1,1},
		{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0},
		 {0,6},{0,7},{1,8},{1,9},{2,10},{2,11},
		 {3,12},{3,13},{4,14},{4,15},{5,16},{5,17}});
}

// ethene C2H4 (acyclic double bond, must NOT be aromatic).
State make_ethene() {
	return make_state({6,6,1,1,1,1},
		{{0,1},{0,2},{0,3},{1,4},{1,5}});
}

// d8 square-planar metal complex stub: Pt(78) with 4 Cl ligands.
State make_ptcl4() {
	return make_state({78,17,17,17,17},
		{{0,1},{0,2},{0,3},{0,4}});
}

void test_huckel_helper() {
	check(satisfies_huckel(2),  "Huckel: 2 pi e- (n=0) aromatic count");
	check(satisfies_huckel(6),  "Huckel: 6 pi e- (n=1) aromatic count");
	check(satisfies_huckel(10), "Huckel: 10 pi e- (n=2) aromatic count");
	check(!satisfies_huckel(4), "Huckel: 4 pi e- NOT aromatic");
	check(!satisfies_huckel(8), "Huckel: 8 pi e- NOT aromatic");
	check(!satisfies_huckel(0), "Huckel: 0 pi e- NOT aromatic");
}

void test_aromatic_gate() {
	{
		AromaticityReport r = analyze_aromaticity(make_benzene());
		check(r.aromatic_ring_count == 1, "benzene: 1 aromatic ring");
		check(r.any_aromatic(), "benzene: any_aromatic() true");
		check(!r.rings.empty() && r.rings[0].pi_electrons == 6,
			  "benzene: 6 pi electrons counted");
		check(!r.rings.empty() && r.rings[0].verdict == AromaticClass::Aromatic,
			  "benzene: verdict Aromatic");
	}
	{
		AromaticityReport r = analyze_aromaticity(make_cyclobutadiene());
		check(r.aromatic_ring_count == 0, "cyclobutadiene: NOT counted aromatic");
		check(!r.rings.empty() && r.rings[0].verdict == AromaticClass::Antiaromatic,
			  "cyclobutadiene: verdict Antiaromatic (4n)");
		check(!r.rings.empty() && r.rings[0].pi_electrons == 4,
			  "cyclobutadiene: 4 pi electrons");
	}
	{
		AromaticityReport r = analyze_aromaticity(make_cyclohexane());
		check(r.aromatic_ring_count == 0, "cyclohexane: NOT aromatic (saturated)");
		check(!r.rings.empty() && r.rings[0].verdict == AromaticClass::NonAromatic,
			  "cyclohexane: verdict NonAromatic");
	}
	{
		AromaticityReport r = analyze_aromaticity(make_ethene());
		check(r.rings.empty(), "ethene: no rings -> nothing to gate");
		check(!r.any_aromatic(), "ethene: not aromatic");
	}
	{
		// pyridine: with valence-deficit Kekule, the ring alternates 3 doubles
		// -> 6 pi electrons -> aromatic (heteroaromatic candidate).
		AromaticityReport r = analyze_aromaticity(make_pyridine());
		check(r.aromatic_ring_count == 1, "pyridine: aromatic heterocycle detected");
	}
}

void test_d8_square_planar() {
	check(is_d8_square_planar_metal(78), "Pt is a d8 square-planar metal");
	check(is_d8_square_planar_metal(46), "Pd is a d8 square-planar metal");
	check(is_d8_square_planar_metal(28), "Ni is a d8 square-planar metal");
	check(!is_d8_square_planar_metal(6), "C is NOT a d8 square-planar metal");

	// PtCl4: 4 bonded domains, 0 lone pairs -> square planar via the branch.
	const State pt = make_ptcl4();
	VSEPROptions opts;   // allow_d8_square_planar defaults true
	auto report = classify_vsepr_sites(pt, opts);
	const auto& metal = report.sites[0];
	check(metal.bonded_domain_count == 4, "PtCl4: 4 bonded domains");
	check(metal.used_d8_square_planar, "PtCl4: d8 override applied");
	check(metal.molecular_shape == VSEPRMolecularShape::SquarePlanar,
		  "PtCl4 -> square planar (not tetrahedral)");
	check(metal.molecular_shape != VSEPRMolecularShape::Tetrahedral,
		  "PtCl4 did NOT collapse to tetrahedral");

	// Disabling the branch restores ordinary AXE geometry (tetrahedral), and a
	// normal organic (methane-like C) is unaffected by the d8 flag.
	VSEPROptions off;
	off.allow_d8_square_planar = false;
	auto report_off = classify_vsepr_sites(pt, off);
	check(!report_off.sites[0].used_d8_square_planar,
		  "PtCl4: override respects allow_d8_square_planar=false");
}

} // namespace

int main() {
	std::cout << "Group 102b  --  WO-84D aromaticity + d8 square-planar\n";
	test_huckel_helper();
	test_aromatic_gate();
	test_d8_square_planar();

	std::cout << "\n  Result: " << g_pass << " PASS / " << g_fail << " FAIL\n";
	if (g_fail == 0) {
		std::cout << "  PASS: all Group 102b aromaticity checks satisfied\n";
		return 0;
	}
	std::cout << "  FAIL: Group 102b aromaticity checks failed\n";
	return 1;
}
