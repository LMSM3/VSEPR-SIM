// =============================================================================
// tests/test_sim_metrics.cpp — Group 21: Simulation Health Metrics
// =============================================================================
// Tests all five health metrics and the SimMetrics aggregator.
//
// Scenarios:
//   1.  EnergyDriftTracker — flat energy → zero drift
//   2.  EnergyDriftTracker — linearly growing energy → increasing rel_drift
//   3.  EnergyDriftTracker — is_drifting() threshold
//   4.  EnergyDriftTracker — set_baseline() re-anchors drift to zero
//   5.  RMSDTracker — identical frames → rmsd_ref = 0, rmsd_step = 0
//   6.  RMSDTracker — shifted frame → rmsd_ref matches known value
//   7.  RMSDTracker — converging frames → rmsd_step decreasing
//   8.  DisplacementTracker — identical frames → mean_displacement = 0
//   9.  DisplacementTracker — uniform shift → correct mean_displacement
//   10. DisplacementTracker — max_displacement tracks worst atom
//   11. StructuralResidual — identical → residual = 0, defect_fraction = 0
//   12. StructuralResidual — one-site defect → defect_fraction = 1/N
//   13. StructuralResidual — all sites displaced beyond threshold
//   14. SimMetrics — complete 10-frame settling scenario (output table printed)
//   15. SimMetricsRow::to_tsv() / tsv_header() column count consistency
//   16. Rigid translation — Kabsch RMSD ≈ 0; raw displacement > 0
//   17. Rigid rotation   — Kabsch RMSD ≈ 0; raw displacement > 0
//   18. Non-rigid deformation — RMSD_ref > 0, decays over frames, gate settles
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <sstream>
#include <string>

#include "core/stats/sim_metrics.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr double EPS = 1e-9;

static bool near(double a, double b, double tol = EPS) {
	return std::abs(a - b) < tol;
}

static std::vector<vsepr::Vec3> make_frame(int N, double offset_x = 0.0) {
	std::vector<vsepr::Vec3> f;
	f.reserve(static_cast<std::size_t>(N));
	for (int i = 0; i < N; ++i) {
		f.push_back({static_cast<double>(i) + offset_x,
					 0.0,
					 0.0});
	}
	return f;
}

// Rotate all atoms around Z by angle_rad
static std::vector<vsepr::Vec3> rotate_z(
		const std::vector<vsepr::Vec3>& in, double angle_rad) {
	std::vector<vsepr::Vec3> out;
	out.reserve(in.size());
	const double c = std::cos(angle_rad);
	const double s = std::sin(angle_rad);
	for (const auto& p : in)
		out.push_back({ c * p.x - s * p.y,
						s * p.x + c * p.y,
						p.z });
	return out;
}
// ---------------------------------------------------------------------------
static void test_energy_flat() {
	vsepr::EnergyDriftTracker t;
	for (int i = 0; i < 50; ++i) t.push(-100.0);
	assert(near(t.delta_E, 0.0) && "energy flat: delta_E != 0");
	assert(near(t.rel_drift, 0.0) && "energy flat: rel_drift != 0");
	assert(!t.is_drifting() && "energy flat: should not flag drifting");
	std::puts("PASS  test_energy_flat");
}

// ---------------------------------------------------------------------------
// 2. EnergyDriftTracker — linear drift
// ---------------------------------------------------------------------------
static void test_energy_linear_drift() {
	vsepr::EnergyDriftTracker t;
	t.push(-100.0);                      // E_0
	for (int i = 1; i <= 10; ++i) {
		t.push(-100.0 + static_cast<double>(i));
	}
	// After 10 pushes, E_current = -90.0, delta_E = 10.0
	assert(near(t.delta_E, 10.0, 1e-9) && "energy drift: wrong delta_E");
	assert(t.is_drifting(0.05) && "energy drift: should flag 10% drift");
	std::puts("PASS  test_energy_linear_drift");
}

// ---------------------------------------------------------------------------
// 3. EnergyDriftTracker — is_drifting threshold
// ---------------------------------------------------------------------------
static void test_energy_threshold() {
	vsepr::EnergyDriftTracker t;
	t.push(-1000.0);
	t.push(-1001.0);   // 0.1% drift
	assert(!t.is_drifting(0.005) && "threshold: 0.1% should not exceed 0.5%");
	assert( t.is_drifting(0.0005)&& "threshold: 0.1% should exceed 0.05%");
	std::puts("PASS  test_energy_threshold");
}

// ---------------------------------------------------------------------------
// 4. EnergyDriftTracker — set_baseline re-anchors
// ---------------------------------------------------------------------------
static void test_energy_set_baseline() {
	vsepr::EnergyDriftTracker t;
	t.push(-100.0);
	t.push(-120.0);
	assert(t.is_drifting() && "set_baseline pre: should be drifting");
	t.set_baseline(-120.0);
	assert(near(t.delta_E, 0.0) && "set_baseline: delta_E should be 0 after re-anchor");
	std::puts("PASS  test_energy_set_baseline");
}

// ---------------------------------------------------------------------------
// 5. RMSDTracker — identical frames
// ---------------------------------------------------------------------------
static void test_rmsd_identical() {
	const auto ref = make_frame(5);
	vsepr::RMSDTracker t;
	t.set_reference(ref);
	t.push(ref);
	t.push(ref);
	assert(near(t.rmsd_ref,  0.0, 1e-9) && "rmsd identical: rmsd_ref != 0");
	assert(near(t.rmsd_step, 0.0, 1e-9) && "rmsd identical: rmsd_step != 0");
	std::puts("PASS  test_rmsd_identical");
}

// ---------------------------------------------------------------------------
// 6. RMSDTracker — shifted frame
// ---------------------------------------------------------------------------
static void test_rmsd_shifted() {
	// Shift all atoms by 1 Å along X; with Kabsch alignment the RMSD is 0
	// because a pure translation is removed.  To get a non-zero RMSD we use
	// a non-rigid deformation: stretch the frame.
	const std::vector<vsepr::Vec3> ref  = {{0,0,0},{2,0,0},{4,0,0}};
	const std::vector<vsepr::Vec3> def  = {{0,0,0},{2,0,0},{5,0,0}}; // last atom +1 Å

	vsepr::RMSDTracker t;
	t.set_reference(ref);
	t.push(def);
	// Kabsch-aligned RMSD; rough tolerance — just verify it's positive and
	// plausible (deformation of one of three atoms by 1 Å → RMSD ≈ 0.577 Å)
	assert(t.rmsd_ref > 0.1 && t.rmsd_ref < 2.0 && "rmsd shifted: expected positive RMSD");
	std::puts("PASS  test_rmsd_shifted");
}

// ---------------------------------------------------------------------------
// 7. RMSDTracker — converging frames → rmsd_step decreasing
// ---------------------------------------------------------------------------
static void test_rmsd_converging() {
	// Use a non-rigid deformation that shrinks: stretch the last atom by a
	// decreasing amount.  Kabsch cannot remove a stretch so rmsd_step is > 0
	// and decreases each frame.
	const std::vector<vsepr::Vec3> ref = {{0,0,0},{1,0,0},{2,0,0},{3,0,0}};
	vsepr::RMSDTracker t;
	t.set_reference(ref);

	double stretch = 2.0;
	// First frame — establishes "previous"
	{
		auto f = ref;
		f[3].x += stretch;
		t.push(f);
	}

	double prev_step = 1e10;
	for (int i = 0; i < 5; ++i) {
		stretch *= 0.5;
		auto f = ref;
		f[3].x += stretch;
		t.push(f);
		assert(t.rmsd_step < prev_step && "rmsd converging: rmsd_step not decreasing");
		prev_step = t.rmsd_step;
	}
	std::puts("PASS  test_rmsd_converging");
}

// ---------------------------------------------------------------------------
// 8. DisplacementTracker — identical frames
// ---------------------------------------------------------------------------
static void test_displacement_identical() {
	const auto ref = make_frame(6);
	vsepr::DisplacementTracker t;
	t.set_reference(ref);
	t.push(ref);
	assert(near(t.mean_displacement, 0.0) && "displacement identical: not zero");
	assert(near(t.max_displacement,  0.0) && "displacement identical: max not zero");
	std::puts("PASS  test_displacement_identical");
}

// ---------------------------------------------------------------------------
// 9. DisplacementTracker — uniform shift
// ---------------------------------------------------------------------------
static void test_displacement_uniform() {
	// All atoms shift by 3 Å along X → mean = max = 3
	const auto ref = make_frame(4, 0.0);
	const auto cur = make_frame(4, 3.0);
	vsepr::DisplacementTracker t;
	t.set_reference(ref);
	t.push(cur);
	assert(near(t.mean_displacement, 3.0) && "displacement uniform: wrong mean");
	assert(near(t.max_displacement,  3.0) && "displacement uniform: wrong max");
	std::puts("PASS  test_displacement_uniform");
}

// ---------------------------------------------------------------------------
// 10. DisplacementTracker — max tracks worst offender
// ---------------------------------------------------------------------------
static void test_displacement_max() {
	const std::vector<vsepr::Vec3> ref = {{0,0,0},{1,0,0},{2,0,0}};
	const std::vector<vsepr::Vec3> cur = {{0,0,0},{1,0,0},{7,0,0}}; // last moves 5
	vsepr::DisplacementTracker t;
	t.set_reference(ref);
	t.push(cur);
	assert(near(t.max_displacement, 5.0) && "displacement max: wrong worst offender");
	std::puts("PASS  test_displacement_max");
}

// ---------------------------------------------------------------------------
// 11. StructuralResidual — identical
// ---------------------------------------------------------------------------
static void test_residual_identical() {
	const auto ref = make_frame(5);
	vsepr::StructuralResidual s;
	s.set_reference(ref);
	s.push(ref);
	assert(near(s.structural_residual, 0.0) && "residual identical: not zero");
	assert(near(s.defect_fraction,     0.0) && "residual identical: defect_fraction not 0");
	std::puts("PASS  test_residual_identical");
}

// ---------------------------------------------------------------------------
// 12. StructuralResidual — one-site defect
// ---------------------------------------------------------------------------
static void test_residual_one_defect() {
	// 5 atoms; last one moves by 1 Å > default threshold 0.5 Å
	const std::vector<vsepr::Vec3> ref = {{0,0,0},{1,0,0},{2,0,0},{3,0,0},{4,0,0}};
		  std::vector<vsepr::Vec3> cur = ref;
	cur[4].x += 1.0;

	vsepr::StructuralResidual s;
	s.set_reference(ref);
	s.push(cur);

	assert(near(s.defect_fraction, 0.2, 1e-9) && "residual one defect: fraction != 0.2");
	assert(s.structural_residual > 0.0         && "residual one defect: residual == 0");
	std::puts("PASS  test_residual_one_defect");
}

// ---------------------------------------------------------------------------
// 13. StructuralResidual — all sites displaced
// ---------------------------------------------------------------------------
static void test_residual_all_defect() {
	const auto ref = make_frame(4, 0.0);
	const auto cur = make_frame(4, 1.0);   // every atom moves 1 Å > 0.5 threshold

	vsepr::StructuralResidual s;
	s.defect_radius_threshold = 0.5;
	s.set_reference(ref);
	s.push(cur);

	assert(near(s.defect_fraction, 1.0) && "residual all defect: fraction != 1.0");
	assert(near(s.structural_residual, 1.0) && "residual all defect: wrong RMS");
	std::puts("PASS  test_residual_all_defect");
}

// ---------------------------------------------------------------------------
// 14. SimMetrics — 10-frame settling scenario
// ---------------------------------------------------------------------------
static void test_sim_metrics_full() {
	const int N = 6;
	const auto ref = make_frame(N, 0.0);
	const double E0 = -500.0;

	vsepr::SimMetrics metrics;
	// Reduce gate requirements so the test doesn't need 100 frames
	metrics.gate_rmsd_ref.min_samples       = 5;
	metrics.gate_rmsd_step.min_samples      = 5;
	metrics.gate_displacement.min_samples   = 5;
	metrics.gate_residual.min_samples       = 5;
	metrics.gate_rmsd_ref.relative_tolerance       = 1.0;  // very loose
	metrics.gate_rmsd_step.relative_tolerance      = 1.0;
	metrics.gate_displacement.relative_tolerance   = 1.0;
	metrics.gate_residual.relative_tolerance       = 1.0;
	metrics.gate_rmsd_step.absolute_tolerance      = 1.0;
	metrics.gate_residual.absolute_tolerance       = 1.0;
	metrics.gate_displacement.absolute_tolerance   = 1.0;
	metrics.gate_rmsd_ref.absolute_tolerance       = 1.0;

	metrics.set_reference(ref, E0);

	vsepr::SimMetricsLog log;
	std::printf("\n%s\n", vsepr::SimMetricsRow::tsv_header().c_str());

	for (int frame = 1; frame <= 10; ++frame) {
		// Energy stabilises toward -500
		const double E = E0 + 10.0 * std::exp(-0.5 * frame);
		// Positions relax toward ref
		const double offset = 2.0 * std::exp(-0.4 * frame);
		const auto pos = make_frame(N, offset);

		metrics.push(static_cast<uint64_t>(frame),
					 frame * 0.002,  // 2 fs timestep
					 E, pos);

		vsepr::SimMetricsRow row = metrics.current_row();
		log.push_back(row);
		std::printf("%s\n", row.to_tsv().c_str());
	}

	// Last frame should have settled (gates wide-open in this test)
	assert(log.back().stationary_flag && "sim_metrics full: should be stationary at frame 10");

	// Energy was drifting (started above E0, then settled)
	// At frame 1 drift is positive; at frame 10 it is near zero
	assert(log.front().E_drift > 0.0 && "sim_metrics full: initial drift should be positive");

	std::puts("PASS  test_sim_metrics_full");
}

// ---------------------------------------------------------------------------
// 15. TSV column count consistency
// ---------------------------------------------------------------------------
static void test_tsv_columns() {
	const auto header = vsepr::SimMetricsRow::tsv_header();
	vsepr::SimMetricsRow row;
	row.frame = 1;
	const auto line = row.to_tsv();

	// Count tabs in header and data line — should be identical
	auto count_tabs = [](const std::string& s) {
		std::size_t n = 0;
		for (char c : s) if (c == '\t') ++n;
		return n;
	};

	assert(count_tabs(header) == count_tabs(line)
		   && "tsv columns: header and data tab count mismatch");
	std::puts("PASS  test_tsv_columns");
}

// ---------------------------------------------------------------------------
// 16. Rigid translation — Kabsch RMSD ≈ 0; raw displacement > 0
// ---------------------------------------------------------------------------
static void test_rigid_translation() {
	// Kabsch removes translations: RMSD_ref and RMSD_step must both be ~0.
	// DisplacementTracker has no alignment, so it should still see the shift.
	const std::vector<vsepr::Vec3> ref = {{0,0,0},{1,1,0},{2,0,0},{0,2,0},{3,1,0}};
	const double shift = 7.5;
	std::vector<vsepr::Vec3> translated;
	for (const auto& p : ref)
		translated.push_back({p.x + shift, p.y, p.z});

	vsepr::RMSDTracker        rmsd;
	vsepr::DisplacementTracker disp;
	rmsd.set_reference(ref);
	disp.set_reference(ref);

	// First push (establishes "previous" for step RMSD)
	rmsd.push(ref);
	// Second push with translated frame
	rmsd.push(translated);
	disp.push(translated);

	assert(rmsd.rmsd_ref  < 1e-6 && "rigid translation: RMSD_ref should be ~0");
	assert(rmsd.rmsd_step < 1e-6 && "rigid translation: RMSD_step should be ~0");
	assert(disp.mean_displacement > 0.1 && "rigid translation: mean_displacement should be >0");

	// motion_class test via SimMetricsRow helper
	vsepr::SimMetricsRow row;
	row.RMSD_ref          = rmsd.rmsd_ref;
	row.RMSD_step         = rmsd.rmsd_step;
	row.mean_displacement = disp.mean_displacement;
	row.stationary_flag   = false;
	row.E_rel_drift       = 0.0;
	row.motion_class      = vsepr::classify_motion(row);
	assert(row.motion_class == "rigid_motion" && "rigid translation: wrong motion_class");

	std::puts("PASS  test_rigid_translation");
}

// ---------------------------------------------------------------------------
// 17. Rigid rotation — Kabsch RMSD ≈ 0; raw displacement > 0
// ---------------------------------------------------------------------------
static void test_rigid_rotation() {
	// Use a non-collinear reference so rotation produces real displacement.
	const std::vector<vsepr::Vec3> ref = {{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}};
	const double theta = 3.14159265358979323846 / 3.0;  // 60 degrees
	const auto rotated = rotate_z(ref, theta);

	vsepr::RMSDTracker        rmsd;
	vsepr::DisplacementTracker disp;
	rmsd.set_reference(ref);
	disp.set_reference(ref);

	rmsd.push(ref);      // establishes previous
	rmsd.push(rotated);
	disp.push(rotated);

	assert(rmsd.rmsd_ref  < 1e-6 && "rigid rotation: RMSD_ref should be ~0");
	assert(rmsd.rmsd_step < 1e-6 && "rigid rotation: RMSD_step should be ~0");
	assert(disp.mean_displacement > 0.1 && "rigid rotation: mean_displacement should be >0");

	vsepr::SimMetricsRow row;
	row.RMSD_ref          = rmsd.rmsd_ref;
	row.RMSD_step         = rmsd.rmsd_step;
	row.mean_displacement = disp.mean_displacement;
	row.stationary_flag   = false;
	row.E_rel_drift       = 0.0;
	row.motion_class      = vsepr::classify_motion(row);
	assert(row.motion_class == "rigid_motion" && "rigid rotation: wrong motion_class");

	std::puts("PASS  test_rigid_rotation");
}

// ---------------------------------------------------------------------------
// 18. Non-rigid deformation — RMSD_ref > 0 early, decays, gate settles
// ---------------------------------------------------------------------------
static void test_nonrigid_deformation() {
	// Deformation: p_i.x += A(t)*0.02*i  p_i.y += A(t)*((i%2==0)?0.03:-0.03)  p_i.z += A(t)*sin(i)
	// A(t) = 4.0 * exp(-0.6 * frame) — decays to ~0
	const int N = 8;
	std::vector<vsepr::Vec3> ref;
	for (int i = 0; i < N; ++i)
		ref.push_back({static_cast<double>(i), 0.0, 0.0});

	const double E0 = -200.0;
	vsepr::SimMetrics metrics;
	metrics.gate_rmsd_ref.min_samples      = 8;
	metrics.gate_rmsd_step.min_samples     = 8;
	metrics.gate_displacement.min_samples  = 8;
	metrics.gate_residual.min_samples      = 8;
	metrics.gate_rmsd_ref.relative_tolerance      = 1.0;
	metrics.gate_rmsd_step.relative_tolerance     = 1.0;
	metrics.gate_displacement.relative_tolerance  = 1.0;
	metrics.gate_residual.relative_tolerance      = 1.0;
	metrics.gate_rmsd_ref.absolute_tolerance      = 1.0;
	metrics.gate_rmsd_step.absolute_tolerance     = 1.0;
	metrics.gate_displacement.absolute_tolerance  = 1.0;
	metrics.gate_residual.absolute_tolerance      = 1.0;
	metrics.set_reference(ref, E0);

	vsepr::SimMetricsLog log;
	std::printf("\n[test_nonrigid_deformation]\n%s\n",
		vsepr::SimMetricsRow::tsv_header().c_str());

	for (int frame = 1; frame <= 20; ++frame) {
		const double A = 4.0 * std::exp(-0.6 * frame);
		std::vector<vsepr::Vec3> pos;
		for (int i = 0; i < N; ++i) {
			pos.push_back({
				ref[i].x + A * 0.02 * i,
				ref[i].y + A * ((i % 2 == 0) ? 0.03 : -0.03),
				ref[i].z + A * std::sin(static_cast<double>(i))
			});
		}
		const double E = E0 + 5.0 * std::exp(-0.4 * frame);
		metrics.push(static_cast<uint64_t>(frame), frame * 0.002, E, pos);
		vsepr::SimMetricsRow row = metrics.current_row();
		log.push_back(row);
		std::printf("%s\n", row.to_tsv().c_str());
	}

	// RMSD_ref must have been > 0 early on
	assert(log.front().RMSD_ref > 1e-6 && "nonrigid: initial RMSD_ref should be > 0");
	// RMSD_ref must have decayed: last frame < first frame
	assert(log.back().RMSD_ref < log.front().RMSD_ref && "nonrigid: RMSD_ref should decay");
	// mean_displacement must also have decayed
	assert(log.back().mean_displacement < log.front().mean_displacement
		&& "nonrigid: mean_displacement should decay");
	// stationary_flag must eventually be true
	bool ever_settled = false;
	for (const auto& r : log) if (r.stationary_flag) { ever_settled = true; break; }
	assert(ever_settled && "nonrigid: should settle by frame 20");
	// motion_class on the first frame must be nonrigid_deformation
	assert(log.front().motion_class == "nonrigid_deformation"
		&& "nonrigid: first frame motion_class wrong");

	std::puts("PASS  test_nonrigid_deformation");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
	test_energy_flat();
	test_energy_linear_drift();
	test_energy_threshold();
	test_energy_set_baseline();
	test_rmsd_identical();
	test_rmsd_shifted();
	test_rmsd_converging();
	test_displacement_identical();
	test_displacement_uniform();
	test_displacement_max();
	test_residual_identical();
	test_residual_one_defect();
	test_residual_all_defect();
	test_sim_metrics_full();
	test_tsv_columns();
	test_rigid_translation();
	test_rigid_rotation();
	test_nonrigid_deformation();

	std::puts("\nAll simulation health metric tests passed.");
	return 0;
}
