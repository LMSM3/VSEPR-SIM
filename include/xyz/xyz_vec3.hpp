#pragma once
/**
 * include/xyz/xyz_vec3.hpp
 * ========================
 * Public XYZVec3 type — the script-facing, file-facing, state-facing
 * coordinate type for the VSIM interpreter and xyz pipeline.
 *
 * XYZVec3 is the same layout as vsepr::Vec3. The two types are kept
 * distinct to enforce the boundary: XYZVec3 travels through scripts and
 * state pipelines; vsepr::Vec3 is the numerical backend inside pbc.hpp.
 * The conversion helpers below are the only bridge.
 *
 * No circular dependency: this header includes only core/math_vec3.hpp.
 * pbc_bindings.cpp includes both this header and box/pbc.hpp.
 *
 * WO-VSEPR-SIM-57D  |  beta-8
 */

#include "core/math_vec3.hpp"  // vsepr::Vec3

// ── XYZVec3 ──────────────────────────────────────────────────────────────────
// Script/state-facing 3-D coordinate.  Fields are .x, .y, .z.
// Coordinates are in angstrom unless otherwise noted at the call site.

using XYZVec3 = vsepr::Vec3;

// ── Int3 ─────────────────────────────────────────────────────────────────────
// Script-facing integer triple.  Used for pbc.image_count return values.
// Fields are .x, .y, .z  (NOT .ix, .iy, .iz — scripts don't know about
// internal ImageCount naming).

struct Int3 {
	int x = 0;
	int y = 0;
	int z = 0;
};

// ── XYZVec3 ↔ vsepr::Vec3 conversion helpers ─────────────────────────────────
// These are the ONLY place where XYZVec3 and vsepr::Vec3 meet.
// They must not appear in the math layer or interpreter core.

inline vsepr::Vec3 to_pbc_vec3(const XYZVec3& v) {
	return {v.x, v.y, v.z};
}

inline XYZVec3 from_pbc_vec3(const vsepr::Vec3& v) {
	return {v.x, v.y, v.z};
}
