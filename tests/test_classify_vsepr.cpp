#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/vsepr.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using atomistic::Edge;
using atomistic::State;
using atomistic::Vec3;
using atomistic::classify::OrganicClassifier;
using atomistic::classify::OrganicCandidate;
using atomistic::classify::OrganicFamily;
using atomistic::classify::VSEPRElectronGeometry;
using atomistic::classify::VSEPRMolecularShape;
using atomistic::classify::VSEPRReport;
using atomistic::classify::VSEPRSite;
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

static const VSEPRSite& site(const State& state, std::size_t center) {
	static atomistic::classify::VSEPRReport report;
	report = classify_vsepr_sites(state);
	assert(center < report.sites.size());
	return report.sites[center];
}

static void expect_shape(
	const State& state,
	std::size_t center,
	const char* ax,
	VSEPRElectronGeometry electron_geometry,
	VSEPRMolecularShape molecular_shape
) {
	const auto& s = site(state, center);
	assert(s.ax_label == ax);
	assert(s.electron_geometry == electron_geometry);
	assert(s.molecular_shape == molecular_shape);
	assert(s.confidence > 0.55);
}

static State make_co2() {
	return make_state(
		{6, 8, 8},
		{{0.0, 0.0, 0.0}, {1.16, 0.0, 0.0}, {-1.16, 0.0, 0.0}},
		{{0, 1}, {0, 2}}
	);
}

static State make_bf3() {
	const double r = 1.30;
	const double c = -0.5 * r;
	const double s = std::sqrt(3.0) * 0.5 * r;
	return make_state(
		{5, 9, 9, 9},
		{{0.0, 0.0, 0.0}, {r, 0.0, 0.0}, {c, s, 0.0}, {c, -s, 0.0}},
		{{0, 1}, {0, 2}, {0, 3}}
	);
}

static State make_ch4() {
	return make_state(
		{6, 1, 1, 1, 1},
		{
			{0.0, 0.0, 0.0},
			{1.0, 1.0, 1.0},
			{-1.0, -1.0, 1.0},
			{-1.0, 1.0, -1.0},
			{1.0, -1.0, -1.0}
		},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}}
	);
}

static State make_distorted_ch4() {
	return make_state(
		{6, 1, 1, 1, 1},
		{
			{0.0, 0.0, 0.0},
			{2.5, 0.2, 0.1},
			{-1.0, -1.0, 1.0},
			{-1.0, 1.0, -1.0},
			{1.0, -1.0, -1.0}
		},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}}
	);
}

static State make_nh3() {
	return make_state(
		{7, 1, 1, 1},
		{
			{0.0, 0.0, 0.20},
			{0.94, 0.0, -0.20},
			{-0.47, 0.814, -0.20},
			{-0.47, -0.814, -0.20}
		},
		{{0, 1}, {0, 2}, {0, 3}}
	);
}

static State make_h2o() {
	const double angle = 104.5 * 3.14159265358979323846 / 180.0;
	return make_state(
		{8, 1, 1},
		{
			{0.0, 0.0, 0.0},
			{0.96, 0.0, 0.0},
			{0.96 * std::cos(angle), 0.96 * std::sin(angle), 0.0}
		},
		{{0, 1}, {0, 2}}
	);
}

static State make_pcl5() {
	const double r_eq = 2.0;
	const double r_ax = 2.1;
	const double c = -0.5 * r_eq;
	const double s = std::sqrt(3.0) * 0.5 * r_eq;
	return make_state(
		{15, 17, 17, 17, 17, 17},
		{
			{0.0, 0.0, 0.0},
			{0.0, 0.0, r_ax},
			{0.0, 0.0, -r_ax},
			{r_eq, 0.0, 0.0},
			{c, s, 0.0},
			{c, -s, 0.0}
		},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}}
	);
}

static State make_sf6() {
	const double r = 1.56;
	return make_state(
		{16, 9, 9, 9, 9, 9, 9},
		{
			{0.0, 0.0, 0.0},
			{r, 0.0, 0.0},
			{-r, 0.0, 0.0},
			{0.0, r, 0.0},
			{0.0, -r, 0.0},
			{0.0, 0.0, r},
			{0.0, 0.0, -r}
		},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}}
	);
}

static void test_vsepr_core_shapes() {
	expect_shape(
		make_co2(),
		0,
		"AX2",
		VSEPRElectronGeometry::Linear,
		VSEPRMolecularShape::Linear
	);
	expect_shape(
		make_bf3(),
		0,
		"AX3",
		VSEPRElectronGeometry::TrigonalPlanar,
		VSEPRMolecularShape::TrigonalPlanar
	);
	expect_shape(
		make_ch4(),
		0,
		"AX4",
		VSEPRElectronGeometry::Tetrahedral,
		VSEPRMolecularShape::Tetrahedral
	);
	expect_shape(
		make_nh3(),
		0,
		"AX3E",
		VSEPRElectronGeometry::Tetrahedral,
		VSEPRMolecularShape::TrigonalPyramidal
	);
	expect_shape(
		make_h2o(),
		0,
		"AX2E2",
		VSEPRElectronGeometry::Tetrahedral,
		VSEPRMolecularShape::Bent
	);
	expect_shape(
		make_pcl5(),
		0,
		"AX5",
		VSEPRElectronGeometry::TrigonalBipyramidal,
		VSEPRMolecularShape::TrigonalBipyramidal
	);
	expect_shape(
		make_sf6(),
		0,
		"AX6",
		VSEPRElectronGeometry::Octahedral,
		VSEPRMolecularShape::Octahedral
	);
}

static void test_report_counts() {
	const auto report = classify_vsepr_sites(make_ch4());
	assert(report.tetrahedral_count >= 1);
	assert(report.hypervalent_count == 0);
}

static void test_organic_bridge_receives_vsepr() {
	OrganicClassifier classifier;
	const auto candidate = classifier.classify(make_ch4());

	// VSEPR populates sp3_count, strain_score
	assert(candidate.sp3_count >= 1);
	assert(candidate.strain_score >= 0.0);
	assert(candidate.strain_score <= 1.0);
}

static void test_strain_score_tracks_angle_distortion() {
	OrganicClassifier classifier;
	const auto ideal     = classifier.classify(make_ch4());
	const auto distorted = classifier.classify(make_distorted_ch4());

	assert(ideal.strain_score >= 0.0);
	assert(distorted.strain_score > ideal.strain_score);
}

} // namespace

int main() {
	test_vsepr_core_shapes();
	test_report_counts();
	test_organic_bridge_receives_vsepr();
	test_strain_score_tracks_angle_distortion();

	std::cout << "test_classify_vsepr: PASS\n";
	return 0;
}
