#include "atomistic/classify/vsepr.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using atomistic::Edge;
using atomistic::State;
using atomistic::Vec3;
using atomistic::classify::VSEPRElectronGeometry;
using atomistic::classify::VSEPRMolecularShape;
using atomistic::classify::VSEPROptions;
using atomistic::classify::classify_vsepr_sites;

namespace {

static State make_state(
	std::vector<uint32_t> types,
	std::vector<Vec3> positions,
	std::vector<Edge> bonds
) {
	State s;
	s.N = static_cast<uint32_t>(types.size());
	s.type = std::move(types);
	s.X = std::move(positions);
	s.B = std::move(bonds);
	s.V.resize(s.N);
	s.Q.resize(s.N, 0.0);
	s.M.resize(s.N, 1.0);
	return s;
}

static State make_nh3_planar_connectivity() {
	const double r = 1.0;
	const double c = -0.5 * r;
	const double s = std::sqrt(3.0) * 0.5 * r;
	return make_state(
		{7, 1, 1, 1},
		{{0.0, 0.0, 0.0}, {r, 0.0, 0.0}, {c, s, 0.0}, {c, -s, 0.0}},
		{{0, 1}, {0, 2}, {0, 3}}
	);
}

static State make_h2o_linear_connectivity() {
	return make_state(
		{8, 1, 1},
		{{0.0, 0.0, 0.0}, {0.96, 0.0, 0.0}, {-0.96, 0.0, 0.0}},
		{{0, 1}, {0, 2}}
	);
}

static State make_co2() {
	return make_state(
		{6, 8, 8},
		{{0.0, 0.0, 0.0}, {1.16, 0.0, 0.0}, {-1.16, 0.0, 0.0}},
		{{0, 1}, {0, 2}}
	);
}

static void test_element_inference_can_override_ambiguous_geometry() {
	const auto nh3 = classify_vsepr_sites(make_nh3_planar_connectivity());
	const auto& n = nh3.sites[0];

	assert(n.used_element_lone_pair_inference);
	assert(!n.used_geometry_lone_pair_fallback);
	assert(n.lone_pair_domain_count == 1);
	assert(n.ax_label == "AX3E");
	assert(n.electron_geometry == VSEPRElectronGeometry::Tetrahedral);
	assert(n.molecular_shape == VSEPRMolecularShape::TrigonalPyramidal);

	const auto h2o = classify_vsepr_sites(make_h2o_linear_connectivity());
	const auto& o = h2o.sites[0];

	assert(o.used_element_lone_pair_inference);
	assert(o.lone_pair_domain_count == 2);
	assert(o.ax_label == "AX2E2");
	assert(o.electron_geometry == VSEPRElectronGeometry::Tetrahedral);
	assert(o.molecular_shape == VSEPRMolecularShape::Bent);
}

static void test_geometry_fallback_remains_available() {
	VSEPROptions options;
	options.allow_element_lone_pair_inference = false;

	const auto nh3 = classify_vsepr_sites(make_nh3_planar_connectivity(), options);
	const auto& n = nh3.sites[0];

	assert(!n.used_element_lone_pair_inference);
	assert(n.ax_label == "AX3");
	assert(n.electron_geometry == VSEPRElectronGeometry::TrigonalPlanar);
	assert(n.molecular_shape == VSEPRMolecularShape::TrigonalPlanar);
}

static void test_carbon_two_domain_stays_linear_without_bond_order() {
	const auto report = classify_vsepr_sites(make_co2());
	const auto& c = report.sites[0];

	assert(c.used_element_lone_pair_inference);
	assert(c.lone_pair_domain_count == 0);
	assert(c.ax_label == "AX2");
	assert(c.molecular_shape == VSEPRMolecularShape::Linear);
}

} // namespace

int main() {
	test_element_inference_can_override_ambiguous_geometry();
	test_geometry_fallback_remains_available();
	test_carbon_two_domain_stays_linear_without_bond_order();

	std::cout << "test_classify_lone_pair_inference: PASS\n";
	return 0;
}
