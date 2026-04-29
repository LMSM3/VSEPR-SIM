#pragma once
/**
 * include/vsim/bindings/pbc_bindings.hpp
 * ========================================
 * C++ binding layer for the pbc.* VSIM scripting namespace.
 *
 * These functions are the backend for the six pbc.* script-surface calls
 * introduced in WO-VSEPR-SIM-57B.  They consume the canonical types from
 * include/box/pbc.hpp and the runtime state from VsimDocument.
 *
 * Expression-level dispatch (connecting these to the VSIM interpreter) is
 * wired in WO-57C.  57B provides the C++ implementations and registration
 * declaration so the interpreter has a well-defined target to call.
 *
 * All particle IDs in the pbc.* namespace are 1-indexed (VSIM convention).
 * The bindings apply the 1→0 offset before accessing C++ arrays.
 *
 * Error protocol:
 *   - Configuration errors: throw std::runtime_error with a [PBC ERROR] prefix.
 *   - Range errors: throw std::out_of_range with particle ID and system size.
 *   - Feature-gate errors: throw std::runtime_error with [PBC ERROR] prefix.
 *
 * WO-VSEPR-SIM-57B  |  beta-8
 */

#include "box/pbc.hpp"
#include "vsim/vsim_document.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace vsim::pbc_bindings {

// ── Runtime state that bindings operate on ────────────────────────────────────
// The caller (interpreter or test harness) provides these by reference.
// In the full runtime this will be fields of VsimRuntime; bindings receive
// const/mutable references as needed.

struct PBCBindingContext {
	const vsepr::PeriodicCell&          cell;        // Active periodic cell
	const PBCSection&                   config;      // [pbc] section config
	const std::vector<vsepr::Vec3>&     positions;   // Wrapped positions (0-indexed)
	const std::vector<vsepr::ImageCount>& images;    // Image counters (0-indexed)
};

// ── Validation helpers ────────────────────────────────────────────────────────

// Ensure at least one axis is periodic; throws [PBC ERROR] if not.
inline void require_periodic(const vsepr::PeriodicCell& cell) {
	if (!cell.enabled())
		throw std::runtime_error(
			"[PBC ERROR] pbc.* called but no periodic cell is configured.\n"
			"            Add a [boundary] block with at least one periodic axis.");
}

// Validate 1-indexed particle ID against particle count; return 0-indexed.
inline int require_particle_id(int vsim_id, int n_particles) {
	if (vsim_id < 1)
		throw std::out_of_range(
			"[PBC ERROR] Particle ID " + std::to_string(vsim_id) +
			" is invalid. Particle IDs are 1-indexed.");
	if (vsim_id > n_particles)
		throw std::out_of_range(
			"[PBC ERROR] Particle ID " + std::to_string(vsim_id) +
			" out of range. System has " + std::to_string(n_particles) + " particles.");
	return vsim_id - 1;  // convert to 0-indexed
}

// Ensure track_images is enabled; throws [PBC ERROR] if not.
inline void require_track_images(const PBCSection& cfg, const char* fn_name) {
	if (!cfg.track_images)
		throw std::runtime_error(
			std::string("[PBC ERROR] pbc.") + fn_name +
			" requires track_images = true in [pbc] block.");
}

// ── pbc.wrap(r) ───────────────────────────────────────────────────────────────
// Wraps position r into the active periodic cell.
// Equivalent to wrap_position(r, cell) from include/box/pbc.hpp.

inline vsepr::Vec3 pbc_wrap(const vsepr::Vec3& r, const vsepr::PeriodicCell& cell) {
	require_periodic(cell);
	return vsepr::wrap_position(r, cell);
}

// ── pbc.delta(ri, rj) ────────────────────────────────────────────────────────
// Returns the minimum-image displacement from ri to rj.
// Non-periodic axes carry raw displacement.

inline vsepr::Vec3 pbc_delta(
	const vsepr::Vec3& ri,
	const vsepr::Vec3& rj,
	const vsepr::PeriodicCell& cell)
{
	require_periodic(cell);
	return vsepr::minimum_image_delta(ri, rj, cell);
}

// ── pbc.distance(ri, rj) ─────────────────────────────────────────────────────
// Returns the scalar minimum-image distance between ri and rj.

inline double pbc_distance(
	const vsepr::Vec3& ri,
	const vsepr::Vec3& rj,
	const vsepr::PeriodicCell& cell)
{
	require_periodic(cell);
	return vsepr::pbc_distance(ri, rj, cell);
}

// ── pbc.crossed_boundary(particle_id) ────────────────────────────────────────
// Returns true if the particle's image count changed this step.
// Compares current_images vs prev_images.
// particle_id is 1-indexed (VSIM convention).

inline bool pbc_crossed_boundary(
	int vsim_id,
	const PBCSection& cfg,
	const std::vector<vsepr::ImageCount>& current_images,
	const std::vector<vsepr::ImageCount>& prev_images)
{
	require_track_images(cfg, "crossed_boundary");
	int idx = require_particle_id(vsim_id, static_cast<int>(current_images.size()));
	const auto& cur  = current_images[idx];
	const auto& prev = prev_images[idx];
	return cur.ix != prev.ix || cur.iy != prev.iy || cur.iz != prev.iz;
}

// ── pbc.image_count(particle_id) → (ix, iy, iz) ──────────────────────────────
// Returns the cumulative image counter for a particle.
// particle_id is 1-indexed (VSIM convention).

inline vsepr::ImageCount pbc_image_count(
	int vsim_id,
	const PBCSection& cfg,
	const std::vector<vsepr::ImageCount>& images)
{
	require_track_images(cfg, "image_count");
	int idx = require_particle_id(vsim_id, static_cast<int>(images.size()));
	return images[idx];
}

// ── pbc.unwrap(particle_id) ───────────────────────────────────────────────────
// Returns the continuous (unwrapped) position reconstructed from the wrapped
// position and image count.
// particle_id is 1-indexed (VSIM convention).

inline vsepr::Vec3 pbc_unwrap(
	int vsim_id,
	const PBCSection& cfg,
	const vsepr::PeriodicCell& cell,
	const std::vector<vsepr::Vec3>& positions,
	const std::vector<vsepr::ImageCount>& images)
{
	require_track_images(cfg, "unwrap");
	require_periodic(cell);
	int idx = require_particle_id(vsim_id, static_cast<int>(positions.size()));
	return vsepr::unwrap_position(positions[idx], images[idx], cell);
}

// ── PeriodicCell construction from VsimDocument ───────────────────────────────
// Constructs the canonical vsepr::PeriodicCell from the parsed document sections.
// Throws [PBC ERROR] if cell lengths are missing when a periodic axis is requested.

inline vsepr::PeriodicCell make_periodic_cell(
	const CellSection& cell_sec,
	const BoundarySection& bnd_sec)
{
	bool px = (bnd_sec.x == "periodic");
	bool py = (bnd_sec.y == "periodic");
	bool pz = (bnd_sec.z == "periodic");

	if ((px || py || pz) && !cell_sec.has_cell())
		throw std::runtime_error(
			"[PBC ERROR] [boundary] requests periodic axes but [cell] lengths are not set.");

	if (px && cell_sec.lx <= 0.0)
		throw std::runtime_error("[PBC ERROR] Cell length Lx is zero. Check [cell] lengths field.");
	if (py && cell_sec.ly <= 0.0)
		throw std::runtime_error("[PBC ERROR] Cell length Ly is zero. Check [cell] lengths field.");
	if (pz && cell_sec.lz <= 0.0)
		throw std::runtime_error("[PBC ERROR] Cell length Lz is zero. Check [cell] lengths field.");

	vsepr::PeriodicCell pc;
	pc.lengths     = { cell_sec.lx, cell_sec.ly, cell_sec.lz };
	pc.periodic_x  = px;
	pc.periodic_y  = py;
	pc.periodic_z  = pz;
	return pc;
}

} // namespace vsim::pbc_bindings
