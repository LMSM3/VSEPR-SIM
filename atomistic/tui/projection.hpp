#pragma once
/**
 * projection.hpp -- 3D \u2192 2D terminal projection layer
 * ======================================================
 * VSEPR-SIM
 *
 * Decouples all coordinate mapping from the renderer.
 * Every function is deterministic: same input state \u2192 same screen coords.
 *
 * Architecture:
 *
 *   Vec3 (Cartesian, Angstrom / reduced units)
 *       \u2193
 *   Projection2D (axis selection, origin, scale)
 *       \u2193
 *   ScreenPoint (column, row in terminal grid)
 *       \u2193
 *   Rect guard (in_viewport)
 *
 * Supported modes:
 *   XY, XZ, YZ -- axis-aligned orthographic slices
 *   ISO        -- lightweight isometric projection (no true perspective)
 */

#include "atomistic/core/state.hpp"   // atomistic::Vec3
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace atomistic {
namespace tui {

// ============================================================================
// Axis selector
// ============================================================================

enum class Axis : uint8_t { X = 0, Y = 1, Z = 2 };

// ============================================================================
// Screen primitives
// ============================================================================

struct ScreenPoint {
	int col{};   // x in terminal columns
	int row{};   // y in terminal rows
};

struct Rect {
	int x{}, y{};     // top-left (inclusive)
	int w{}, h{};     // width and height in cells

	int right()  const { return x + w; }
	int bottom() const { return y + h; }
};

// ============================================================================
// Bounding box helper (world-space)
// ============================================================================

struct Bounds3D {
	double xmin{  1e30 }, xmax{ -1e30 };
	double ymin{  1e30 }, ymax{ -1e30 };
	double zmin{  1e30 }, zmax{ -1e30 };

	void expand(const Vec3& p) {
		if (p.x < xmin) xmin = p.x;  if (p.x > xmax) xmax = p.x;
		if (p.y < ymin) ymin = p.y;  if (p.y > ymax) ymax = p.y;
		if (p.z < zmin) zmin = p.z;  if (p.z > zmax) zmax = p.z;
	}

	void pad(double d) {
		xmin -= d; xmax += d;
		ymin -= d; ymax += d;
		zmin -= d; zmax += d;
	}

	static Bounds3D from_positions(const std::vector<Vec3>& pts, double padding = 1.0) {
		Bounds3D b;
		for (auto& p : pts) b.expand(p);
		b.pad(padding);
		return b;
	}
};

// ============================================================================
// Projection2D descriptor
// ============================================================================

struct Projection2D {
	Axis horiz = Axis::X;
	Axis vert  = Axis::Y;
	bool invert_vert = true;   // terminal rows grow downward; invert so +Y is up
};

// Convenience constructors
inline Projection2D proj_xy() { return {Axis::X, Axis::Y, true}; }
inline Projection2D proj_xz() { return {Axis::X, Axis::Z, true}; }
inline Projection2D proj_yz() { return {Axis::Y, Axis::Z, true}; }

// ============================================================================
// Core projection function
// ============================================================================

/**
 * Project a 3D point onto a terminal viewport.
 *
 * @param p       World-space coordinate (Angstrom or any consistent unit)
 * @param proj    Axis selection and orientation
 * @param b       World-space bounding box (with padding already applied)
 * @param vp      Destination terminal rectangle
 * @return        ScreenPoint in terminal column/row coordinates
 *
 * Mapping:
 *   col = vp.x + 2 + (horiz_frac) * (vp.w - 4)
 *   row = vp.y + 2 + (vert_frac)  * (vp.h - 4)   [inverted if invert_vert]
 */
inline ScreenPoint project(const Vec3& p,
							const Projection2D& proj,
							const Bounds3D& b,
							const Rect& vp)
{
	// Extract the two world components
	auto get_component = [&](Axis ax) -> double {
		switch (ax) {
			case Axis::X: return p.x;
			case Axis::Y: return p.y;
			case Axis::Z: return p.z;
		}
		return 0.0;
	};

	auto get_range = [&](Axis ax) -> std::pair<double,double> {
		switch (ax) {
			case Axis::X: return {b.xmin, b.xmax};
			case Axis::Y: return {b.ymin, b.ymax};
			case Axis::Z: return {b.zmin, b.zmax};
		}
		return {0.0, 1.0};
	};

	double hv = get_component(proj.horiz);
	double vv = get_component(proj.vert);

	auto [hmin, hmax] = get_range(proj.horiz);
	auto [vmin, vmax] = get_range(proj.vert);

	double hspan = hmax - hmin;
	double vspan = vmax - vmin;

	double tf = (hspan > 1e-12) ? (hv - hmin) / hspan : 0.5;
	double tr = (vspan > 1e-12) ? (vv - vmin) / vspan : 0.5;
	if (proj.invert_vert) tr = 1.0 - tr;

	int margin = 2;
	int col = vp.x + margin + static_cast<int>(tf * (vp.w - 2 * margin));
	int row = vp.y + margin + static_cast<int>(tr * (vp.h - 2 * margin));

	return {col, row};
}

// Convenience overloads for common slices
inline ScreenPoint project_xy(const Vec3& p, const Bounds3D& b, const Rect& vp) {
	return project(p, proj_xy(), b, vp);
}
inline ScreenPoint project_xz(const Vec3& p, const Bounds3D& b, const Rect& vp) {
	return project(p, proj_xz(), b, vp);
}
inline ScreenPoint project_yz(const Vec3& p, const Bounds3D& b, const Rect& vp) {
	return project(p, proj_yz(), b, vp);
}

// ============================================================================
// Isometric projection (lightweight -- no perspective)
// ============================================================================
//
//   iso_x = (world.x - world.z) * cos(30deg) * scale_col
//   iso_y = (world.x + world.z) * 0.5 * sin(30deg) * scale_row - world.y * scale_row
//
// Terminal cells are roughly 2:1 (col:row), so scale_col = 1, scale_row = 0.5.

inline ScreenPoint project_iso(const Vec3& p,
								 const Vec3& origin,
								 double scale,
								 const Rect& vp)
{
	constexpr double cos30 = 0.866025403784;
	constexpr double sin30 = 0.5;

	double wx = p.x - origin.x;
	double wy = p.y - origin.y;
	double wz = p.z - origin.z;

	double screen_x = (wx - wz) * cos30 * scale;
	double screen_y = (wx + wz) * sin30 * 0.5 * scale - wy * scale * 0.5;

	int cx = vp.x + vp.w / 2 + static_cast<int>(screen_x);
	int cy = vp.y + vp.h / 2 - static_cast<int>(screen_y);

	return {cx, cy};
}

// ============================================================================
// Viewport guard
// ============================================================================

inline bool in_viewport(const ScreenPoint& sp, const Rect& vp) {
	return sp.col >= vp.x && sp.col < vp.right()
		&& sp.row >= vp.y && sp.row < vp.bottom();
}

} // namespace tui
} // namespace atomistic
