#pragma once
/**
 * include/vsim/analysis/ikk_identity_vector.hpp
 * ================================================
 * WO-75B Phase 1  |  IKK Identity Vector (I-vector)
 *
 * Defines `IKKIdentityVector` — a 5-component identity vector in ℝ⁵
 * representing the IKK identity content of a particle at a given scale.
 *
 * ── Mathematical specification ──────────────────────────────────────────────
 *
 * Per-particle per-frame vector (ordered, non-permutable):
 *
 *   I_{p,f} = [ I^x_{p,f}, I^y_{p,f}, I^z_{p,f}, I^t_{p,f}, I^w_{p,f} ]ᵀ
 *
 * Axis semantics (IKK Notation Registry v1.0):
 *   x  →  existence     (S0)          I_existence = 1 - dataloss
 *   y  →  EM channel    (S2/S3)       I_EM        = 1 - hidden_channel
 *   z  →  spatial       (S3)          I_spatial   = recoverable_info
 *   t  →  temporal      (S4)          I_temporal  = 1 - projection_loss
 *   w  →  internal      (spin/color)  I_internal  = 1 - identity_residual
 *
 * Ordering rule (strict):
 *   (x, y, z, t, w) — THIS ORDER IS NON-PERMUTABLE.
 *   Do NOT reorder. The axes are labelled by their IKK role, not by
 *   spatial coordinates. t and w are NOT time and a 5th spatial axis.
 *
 * Frame-mean Ī_f:
 *   Ī_f = (1/N_f) Σ_{p=0}^{N_f-1} I_{p,f}
 *   Ī_f ∈ conv({ I_{p,f} })  (convex hull of frame set)
 *   Ī_f is permutation-invariant in particle index.
 *   Ī_f does NOT recover { I_{p,f} } (first-moment compression only).
 *
 * Frame-variance diag(Var_f):
 *   σ²_{a,f} = (1/N_f) Σ_p (I^a_{p,f} - Ī^a_f)²  for a ∈ {x,y,z,t,w}
 *
 * Frame-to-frame delta:
 *   ΔĪ_f = Ī_{f+1} - Ī_f
 *   Defined only when N_{f+1} = N_f (particle count conserved).
 *   When N changes, the comparison is NOT particle-wise.
 *
 * Magnitude (normalised):
 *   |I|_{p,f} = sqrt(I^x² + I^y² + I^z² + I^t² + I^w²) / sqrt(5)
 *   = 1.0 → full identity content at this scale
 *   = 0.0 → identity dissolved (D = 0, maximum entropy)
 *
 * ── Derivation from IdentitySidecarRecord ──────────────────────────────────
 *   Phase 1 operates on aggregated sidecar scalars (not raw per-particle data):
 *     x  =  1.0 - dataloss
 *     y  =  1.0 - hidden_channel
 *     z  =  recoverable_info
 *     t  =  1.0 - projection_loss
 *     w  =  1.0 - identity_residual
 *
 * ── Output ─────────────────────────────────────────────────────────────────
 *   Written to <output_dir>/<run_id>.identity.json as an array of
 *   IKKIdentityFrameRecord objects (one per sidecar frame).
 *
 * ── Doctrine ────────────────────────────────────────────────────────────────
 *   These vectors are DERIVED from sidecar data only.
 *   They must NEVER be written back to truth-state (.xyz / .xyzFull).
 *   They must NEVER be used as force inputs.
 *   They live in the analysis / report / overlay layer only.
 *
 * Added: WO-75B Phase 1 (Day 76)
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace vsim {
namespace analysis {

// Forward declaration
struct IdentitySidecarRecord;
struct IdentitySidecarSeries;

// ============================================================================
// IKKIdentityVector  —  one I-vector in ℝ⁵
// ============================================================================
/**
 * A single identity vector I_{p,f} ∈ ℝ⁵.
 *
 * Component ordering is FIXED and NON-PERMUTABLE:
 *   [0] = x = existence (S0)
 *   [1] = y = EM        (S2/S3)
 *   [2] = z = spatial   (S3)
 *   [3] = t = temporal  (S4)
 *   [4] = w = internal  (spin/color)
 *
 * Index access via operator[] or named accessors x()/y()/z()/t()/w().
 */
struct IKKIdentityVector {
	// Storage — order MUST match the axis semantics above.
	std::array<float, 5> v {{ 0.f, 0.f, 0.f, 0.f, 0.f }};

	// Named accessors (read/write) — compile-time-checked ordering.
	float& x() noexcept { return v[0]; }  // existence  (S0)
	float& y() noexcept { return v[1]; }  // EM         (S2/S3)
	float& z() noexcept { return v[2]; }  // spatial    (S3)
	float& t() noexcept { return v[3]; }  // temporal   (S4)
	float& w() noexcept { return v[4]; }  // internal

	float x() const noexcept { return v[0]; }
	float y() const noexcept { return v[1]; }
	float z() const noexcept { return v[2]; }
	float t() const noexcept { return v[3]; }
	float w() const noexcept { return v[4]; }

	// Index access.
	float  operator[](std::size_t i) const noexcept { return v[i]; }
	float& operator[](std::size_t i)       noexcept { return v[i]; }

	// Normalised magnitude: ||I|| / sqrt(5). ∈ [0, 1] when all axes ∈ [0, 1].
	[[nodiscard]] float mag() const noexcept {
		float s = 0.f;
		for (float c : v) s += c * c;
		return std::sqrt(s / 5.f);
	}

	// Arithmetic helpers used by frame-mean and delta computations.
	IKKIdentityVector& operator+=(const IKKIdentityVector& o) noexcept {
		for (int i = 0; i < 5; ++i) v[i] += o.v[i];
		return *this;
	}
	IKKIdentityVector& operator-=(const IKKIdentityVector& o) noexcept {
		for (int i = 0; i < 5; ++i) v[i] -= o.v[i];
		return *this;
	}
	IKKIdentityVector& operator*=(float s) noexcept {
		for (float& c : v) c *= s;
		return *this;
	}
	IKKIdentityVector operator-(const IKKIdentityVector& o) const noexcept {
		IKKIdentityVector r = *this;
		r -= o;
		return r;
	}

	// Euclidean norm of (this - other) = ||ΔĪ_f||₂.
	[[nodiscard]] float dist(const IKKIdentityVector& o) const noexcept {
		float s = 0.f;
		for (int i = 0; i < 5; ++i) { float d = v[i] - o.v[i]; s += d * d; }
		return std::sqrt(s);
	}
};

// ============================================================================
// IKKIdentityFrameRecord  —  per-frame aggregated identity statistics
// ============================================================================
/**
 * Stores the frame-level identity statistics computed from N_f particles
 * for frame f:
 *
 *   mean_ivec   = Ī_f       (frame mean, ∈ conv({I_{p,f}}))
 *   var_diag    = diag(Var_f) (per-axis variance)
 *   N_f         = particle count this frame
 *   delta_ivec  = ΔĪ_f = Ī_{f+1} - Ī_f  (set by build_series_stats())
 *   delta_valid = true when N_f == N_{f-1}  (count preserved)
 *
 * These are the values stored in <run_id>.identity.json per frame.
 *
 * DERIVATION (Phase 1 — scalar sidecar proxy):
 *   Since per-particle I_{p,f} vectors are not yet available, Phase 1
 *   treats the entire frame as a single aggregate "particle" (N_f = 1)
 *   derived from IdentitySidecarRecord scalar fields.  Variance = zero.
 *   When per-particle data is added in Phase 4, N_f and var_diag
 *   become meaningful.
 */
struct IKKIdentityFrameRecord {
	int64_t             frame_index  { -1 };
	double              time_fs      { 0.0 };
	int32_t             particle_count { 1 };  // N_f; Phase 1 = 1 aggregate

	IKKIdentityVector   mean_ivec;     // Ī_f — frame mean
	IKKIdentityVector   var_diag;      // diag(Var_f) — per-axis variance
	IKKIdentityVector   delta_ivec;    // ΔĪ_f = Ī_f - Ī_{f-1}
	bool                delta_valid { false }; // false for frame 0 or N change

	// Convenience: magnitude of the frame mean.
	[[nodiscard]] float mean_mag() const noexcept { return mean_ivec.mag(); }

	// Convenience: L2 norm of ΔĪ_f (only meaningful when delta_valid).
	[[nodiscard]] float delta_norm() const noexcept { return delta_ivec.dist({}); }
};

// ============================================================================
// IKKIdentitySeries  —  full per-run collection of frame records
// ============================================================================
/**
 * Holds all per-frame IKKIdentityFrameRecord objects for one run, plus a
 * run-level mean and trajectory delta summary.
 *
 * Built by `build_ivec_series(series)` from an `IdentitySidecarSeries`.
 * Written to JSON by `write_identity_json(ivec_series, path)`.
 */
struct IKKIdentitySeries {
	std::string  run_id;
	std::string  source_vsim;

	std::vector<IKKIdentityFrameRecord> frames;

	// Run-level summary (mean of per-frame means)
	IKKIdentityVector run_mean;
	IKKIdentityVector run_var_diag;  // variance of the frame means

	// Trajectory drift: (Ī_last - Ī_first) / (N_frames - 1) per axis
	// Valid when frames.size() >= 2.
	IKKIdentityVector drift_per_frame;
	bool              drift_valid { false };

	std::size_t frame_count() const noexcept { return frames.size(); }
};

// ============================================================================
// Free functions
// ============================================================================

/**
 * from_sidecar_record()
 *
 * Derives one IKKIdentityVector from a single IdentitySidecarRecord using
 * the Phase 1 proxy mapping:
 *
 *   I^x = 1.0 - dataloss           (existence: complement of information loss)
 *   I^y = 1.0 - hidden_channel     (EM: complement of hidden interaction)
 *   I^z = recoverable_info         (spatial: recoverable fraction)
 *   I^t = 1.0 - projection_loss    (temporal: complement of scale-crossing loss)
 *   I^w = 1.0 - identity_residual  (internal: complement of prototype deviation)
 *
 * All values clamped to [0, 1].
 */
IKKIdentityVector from_sidecar_record(const IdentitySidecarRecord& r) noexcept;

/**
 * build_ivec_series()
 *
 * Constructs a complete IKKIdentitySeries from an IdentitySidecarSeries.
 * Computes per-frame means, frame-to-frame deltas, and the run-level summary.
 *
 * Phase 1 note: each frame maps to exactly one aggregate IKKIdentityVector
 * (particle_count = 1, var_diag = 0).  var_diag and true frame-mean over
 * N_f > 1 particles requires Phase 4 per-particle storage.
 */
IKKIdentitySeries build_ivec_series(const IdentitySidecarSeries& series);

/**
 * write_identity_json()
 *
 * Serialises an IKKIdentitySeries to `<path>` as a JSON document.
 *
 * Output structure:
 * {
 *   "run_id": "...",
 *   "source_vsim": "...",
 *   "frame_count": N,
 *   "run_mean": { "x": 0.0, "y": 0.0, "z": 0.0, "t": 0.0, "w": 0.0, "mag": 0.0 },
 *   "drift_per_frame": { ... },
 *   "frames": [
 *     {
 *       "frame_index": 0,
 *       "time_fs": 0.0,
 *       "particle_count": 1,
 *       "mean": { "x": ..., "y": ..., "z": ..., "t": ..., "w": ..., "mag": ... },
 *       "var": { "x": ..., ... },
 *       "delta": { "x": ..., ..., "norm": ..., "valid": true }
 *     }, ...
 *   ]
 * }
 *
 * Returns true on success, false on file-write failure.
 */
bool write_identity_json(const IKKIdentitySeries& s, const std::string& path);

} // namespace analysis
} // namespace vsim
