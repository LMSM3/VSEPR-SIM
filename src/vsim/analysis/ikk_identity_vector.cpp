/**
 * src/vsim/analysis/ikk_identity_vector.cpp
 * ============================================
 * WO-75B Phase 1  |  IKK Identity Vector -- implementation
 *
 * Implements:
 *   from_sidecar_record()  -- derive IKKIdentityVector from IdentitySidecarRecord
 *   build_ivec_series()    -- construct IKKIdentitySeries from IdentitySidecarSeries
 *   write_identity_json()  -- serialise IKKIdentitySeries to .identity.json
 *
 * Phase 1 proxy mapping (one aggregate vector per frame, particle_count = 1):
 *   I^x = 1 - dataloss          (existence: complement of information loss)
 *   I^y = 1 - hidden_channel    (EM: complement of hidden interaction proxy)
 *   I^z = recoverable_info      (spatial: recoverable fraction)
 *   I^t = 1 - projection_loss   (temporal: complement of scale-crossing loss)
 *   I^w = 1 - identity_residual (internal: complement of prototype deviation)
 *
 * All component values are clamped to [0, 1].
 *
 * DOCTRINE:
 *   All output is DERIVED from IdentitySidecarSeries.
 *   Nothing here mutates SimState or truth-state files (.xyz / .xyzFull).
 *   Output lives at <output_dir>/<run_id>.identity.json.
 *
 * Added: WO-75B Phase 1 (Day 76)
 */

#include "vsim/analysis/ikk_identity_vector.hpp"
#include "vsim/analysis/identity_sidecar.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>

namespace vsim {
namespace analysis {

// ============================================================================
// Helper: clamp double to [0, 1] then cast to float
// ============================================================================

static inline float c01(double v) noexcept {
	return static_cast<float>(v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v));
}

// ============================================================================
// from_sidecar_record()
// Phase 1 proxy: derives one aggregate I-vector from a sidecar record.
// Axis order is FIXED: x=existence, y=EM, z=spatial, t=temporal, w=internal.
// ============================================================================

IKKIdentityVector from_sidecar_record(const IdentitySidecarRecord& r) noexcept {
	IKKIdentityVector iv;
	iv.x() = c01(1.0 - r.dataloss);
	iv.y() = c01(1.0 - r.hidden_channel);
	iv.z() = c01(r.recoverable_info);
	iv.t() = c01(1.0 - r.projection_loss);
	iv.w() = c01(1.0 - r.identity_residual);
	return iv;
}

// ============================================================================
// build_ivec_series()
// ============================================================================

IKKIdentitySeries build_ivec_series(const IdentitySidecarSeries& series) {
	IKKIdentitySeries out;
	out.run_id      = series.run_id;
	out.source_vsim = series.source_vsim;

	if (series.frames.empty()) return out;

	out.frames.reserve(series.frames.size());

	// --- Build per-frame records (first pass) ---
	for (const auto& sf : series.frames) {
		IKKIdentityFrameRecord fr;
		fr.frame_index    = sf.frame_index;
		fr.time_fs        = sf.time_fs;
		fr.particle_count = 1;          // Phase 1: aggregate; Phase 4 adds N_f
		fr.mean_ivec      = from_sidecar_record(sf);
		// var_diag stays zero in Phase 1 (single aggregate point)
		out.frames.push_back(fr);
	}

	// --- Frame-to-frame deltas (second pass): DeltaI_f = I_f - I_{f-1} ---
	// delta_valid only when particle_count is conserved (always 1 in Phase 1)
	for (std::size_t i = 1; i < out.frames.size(); ++i) {
		out.frames[i].delta_ivec  =
			out.frames[i].mean_ivec - out.frames[i - 1].mean_ivec;
		out.frames[i].delta_valid =
			(out.frames[i].particle_count == out.frames[i - 1].particle_count);
	}

	// --- Run-level mean ---
	IKKIdentityVector sum{};
	for (const auto& fr : out.frames) sum += fr.mean_ivec;
	const float nf = static_cast<float>(out.frames.size());
	out.run_mean = sum;
	out.run_mean *= (1.0f / nf);

	// --- Run-level var (variance of frame means) ---
	IKKIdentityVector vsum{};
	for (const auto& fr : out.frames) {
		IKKIdentityVector d = fr.mean_ivec - out.run_mean;
		for (int a = 0; a < 5; ++a) d.v[a] = d.v[a] * d.v[a];
		vsum += d;
	}
	out.run_var_diag = vsum;
	out.run_var_diag *= (1.0f / nf);

	// --- Trajectory drift per frame: (I_last - I_first) / (N - 1) ---
	if (out.frames.size() >= 2) {
		out.drift_per_frame  = out.frames.back().mean_ivec
							 - out.frames.front().mean_ivec;
		out.drift_per_frame *= (1.0f / (nf - 1.0f));
		out.drift_valid = true;
	}

	return out;
}

// ============================================================================
// write_identity_json()
// ============================================================================

static void write_vec_field(std::ostream& o, const char* name,
							 const IKKIdentityVector& v, int indent) {
	const std::string pad(static_cast<std::size_t>(indent), ' ');
	o << pad << '"' << name << "\": { "
	  << "\"x\": " << v.x() << ", "
	  << "\"y\": " << v.y() << ", "
	  << "\"z\": " << v.z() << ", "
	  << "\"t\": " << v.t() << ", "
	  << "\"w\": " << v.w() << ", "
	  << "\"mag\": " << v.mag()
	  << " }";
}

bool write_identity_json(const IKKIdentitySeries& s, const std::string& path) {
	std::ofstream f(path);
	if (!f.is_open()) return false;

	f << std::fixed << std::setprecision(6);
	f << "{\n";
	f << "  \"run_id\": \""      << s.run_id      << "\",\n";
	f << "  \"source_vsim\": \"" << s.source_vsim << "\",\n";
	f << "  \"frame_count\": "   << s.frames.size() << ",\n";
	write_vec_field(f, "run_mean",     s.run_mean,     2); f << ",\n";
	write_vec_field(f, "run_var_diag", s.run_var_diag, 2); f << ",\n";
	if (s.drift_valid) {
		write_vec_field(f, "drift_per_frame", s.drift_per_frame, 2);
		f << ",\n";
	}
	f << "  \"frames\": [\n";
	for (std::size_t i = 0; i < s.frames.size(); ++i) {
		const auto& fr = s.frames[i];
		f << "    {\n";
		f << "      \"frame_index\": " << fr.frame_index    << ",\n";
		f << "      \"time_fs\": "     << fr.time_fs         << ",\n";
		f << "      \"particle_count\": " << fr.particle_count << ",\n";
		write_vec_field(f, "mean", fr.mean_ivec, 6); f << ",\n";
		write_vec_field(f, "var",  fr.var_diag,  6); f << ",\n";
		f << "      \"delta\": { "
		  << "\"x\": " << fr.delta_ivec.x() << ", "
		  << "\"y\": " << fr.delta_ivec.y() << ", "
		  << "\"z\": " << fr.delta_ivec.z() << ", "
		  << "\"t\": " << fr.delta_ivec.t() << ", "
		  << "\"w\": " << fr.delta_ivec.w() << ", "
		  << "\"norm\": " << fr.delta_norm() << ", "
		  << "\"valid\": " << (fr.delta_valid ? "true" : "false")
		  << " }\n";
		f << "    }" << (i + 1 < s.frames.size() ? "," : "") << "\n";
	}
	f << "  ]\n}\n";
	return f.good();
}

} // namespace analysis
} // namespace vsim
