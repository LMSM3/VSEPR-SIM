/**
 * src/vsim/register_pbc_builtins.cpp
 * =====================================
 * Registers all six pbc.* built-in functions with VsimInterpreter.
 *
 * All functions accept and return XYZVec3 (script/state-facing vector type).
 * Conversion to/from vsepr::Vec3 (math backend) happens inside each binding
 * via to_pbc_vec3() / from_pbc_vec3() — the only place these types meet.
 *
 * Particle IDs are 1-indexed in scripts; all ID-accepting functions convert
 * to 0-indexed before accessing runtime arrays.
 *
 * Guard semantics:
 *   require_pbc()          — raised when no periodic axis is configured
 *   require_track_images() — raised when pbc_config.track_images is false
 *   to_particle_index()    — validates 1-indexed ID and returns 0-indexed
 *
 * WO-VSEPR-SIM-57D  |  beta-8
 */

#include "vsim/vsim_interpreter.hpp"
#include "xyz/xyz_vec3.hpp"
#include "box/pbc.hpp"

namespace vsim {

// ── Guards ────────────────────────────────────────────────────────────────────

static void require_pbc(const PBCInterpreterRuntime& rt, const char* fn) {
	if (!rt.cell.enabled())
		throw VsimRuntimeError(
			std::string("[PBC ERROR] pbc.") + fn +
			" called but no periodic cell is configured.\n"
			"            Add a [boundary] block with at least one periodic axis.");
}

static void require_track_images(const PBCInterpreterRuntime& rt, const char* fn) {
	if (!rt.pbc_config.track_images)
		throw VsimRuntimeError(
			std::string("[PBC ERROR] pbc.") + fn +
			" requires track_images = true in [pbc] block.");
}

static int to_particle_index(const Value& v, const PBCInterpreterRuntime& rt) {
	if (!value_is_int(v))
		throw VsimRuntimeError(
			"[PBC ERROR] Particle ID argument must be an integer.");
	int id = static_cast<int>(as_int(v));
	if (id < 1)
		throw VsimRuntimeError(
			"[PBC ERROR] Particle ID 0 is invalid. Particle IDs are 1-indexed.");
	if (id > rt.particle_count())
		throw VsimRuntimeError(
			"[PBC ERROR] Particle ID " + std::to_string(id) +
			" out of range. System has " +
			std::to_string(rt.particle_count()) + " particles.");
	return id - 1;  // 1-indexed → 0-indexed
}

// ── ImageCount → Int3 conversion ──────────────────────────────────────────────

static Int3 to_int3(const vsepr::ImageCount& img) {
	return {img.ix, img.iy, img.iz};
}

// ── Registration ──────────────────────────────────────────────────────────────

void register_pbc_builtins(VsimInterpreter& interp) {

	// ── pbc.wrap(r) → XYZVec3 ──────────────────────────────────────────────
	interp.register_builtin("pbc.wrap",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		require_pbc(rt, "wrap");
		if (args.size() != 1)
			throw VsimRuntimeError("pbc.wrap expects 1 argument (XYZVec3), got "
				+ std::to_string(args.size()) + ".");
		XYZVec3 r = as_xyz_vec3(args[0]);
		return from_pbc_vec3(vsepr::wrap_position(to_pbc_vec3(r), rt.cell));
	});

	// ── pbc.delta(r_i, r_j) → XYZVec3 ────────────────────────────────────
	interp.register_builtin("pbc.delta",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		require_pbc(rt, "delta");
		if (args.size() != 2)
			throw VsimRuntimeError("pbc.delta expects 2 arguments (XYZVec3, XYZVec3), got "
				+ std::to_string(args.size()) + ".");
		XYZVec3 ri = as_xyz_vec3(args[0]);
		XYZVec3 rj = as_xyz_vec3(args[1]);
		return from_pbc_vec3(
			vsepr::minimum_image_delta(to_pbc_vec3(ri), to_pbc_vec3(rj), rt.cell));
	});

	// ── pbc.distance(r_i, r_j) → double ───────────────────────────────────
	interp.register_builtin("pbc.distance",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		require_pbc(rt, "distance");
		if (args.size() != 2)
			throw VsimRuntimeError("pbc.distance expects 2 arguments (XYZVec3, XYZVec3), got "
				+ std::to_string(args.size()) + ".");
		XYZVec3 ri = as_xyz_vec3(args[0]);
		XYZVec3 rj = as_xyz_vec3(args[1]);
		return vsepr::pbc_distance(to_pbc_vec3(ri), to_pbc_vec3(rj), rt.cell);
	});

	// ── pbc.crossed_boundary(id) → bool ───────────────────────────────────
	interp.register_builtin("pbc.crossed_boundary",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		require_pbc(rt, "crossed_boundary");
		require_track_images(rt, "crossed_boundary");
		if (args.size() != 1)
			throw VsimRuntimeError("pbc.crossed_boundary expects 1 argument (int id).");
		int idx = to_particle_index(args[0], rt);
		const auto& cur  = rt.image_counts[idx];
		const auto& prev = rt.image_counts_prev[idx];
		return static_cast<bool>(
			cur.ix != prev.ix || cur.iy != prev.iy || cur.iz != prev.iz);
	});

	// ── pbc.image_count(id) → Int3 ────────────────────────────────────────
	interp.register_builtin("pbc.image_count",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		require_pbc(rt, "image_count");
		require_track_images(rt, "image_count");
		if (args.size() != 1)
			throw VsimRuntimeError("pbc.image_count expects 1 argument (int id).");
		int idx = to_particle_index(args[0], rt);
		return to_int3(rt.image_counts[idx]);
	});

	// ── pbc.unwrap(id) → XYZVec3 ──────────────────────────────────────────
	interp.register_builtin("pbc.unwrap",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		require_pbc(rt, "unwrap");
		require_track_images(rt, "unwrap");
		if (args.size() != 1)
			throw VsimRuntimeError("pbc.unwrap expects 1 argument (int id).");
		int idx = to_particle_index(args[0], rt);
		const auto& p   = rt.particles[idx];
		const auto& img = rt.image_counts[idx];
		const auto& L   = rt.cell.lengths;
		XYZVec3 unwrapped {
			p.position.x + img.ix * L.x,
			p.position.y + img.iy * L.y,
			p.position.z + img.iz * L.z
		};
		return unwrapped;
	});
}

} // namespace vsim
