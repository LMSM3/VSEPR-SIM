#pragma once
/**
 * lattice_builder.hpp -- Supercell builder for metal/alloy generators
 * ====================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Converts a MaterialPreset into an XYZFrame supercell by:
 *   1. Expanding the primitive cell n×n×n times along each axis.
 *   2. Applying Cartesian coordinates from fractional basis positions.
 *   3. Optionally substituting alloy occupancy using a deterministic cycle.
 *   4. Attaching per-atom charge (q), a zero velocity placeholder (v), and
 *      the material reference energy (e) so the frame is .xyza-ready.
 *
 * The XYZBox is set to the supercell diagonal (PBC on all three axes).
 *
 * Unit system: coordinates in Å, consistent with xyz_unified.hpp §1.1.
 */

#include "metal_presets.hpp"
#include "../../src/io/xyz_unified.hpp"   // AtomRecord, XYZFrame, XYZBox, XYZVec3

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <array>

namespace vsepr {
namespace gen {

// ---------------------------------------------------------------------------
// build_fcc_primitive  →  4-atom conventional cell fractional positions
// ---------------------------------------------------------------------------
inline std::vector<std::array<double,3>> fcc_motif() {
	return {
		{0.0, 0.0, 0.0},
		{0.5, 0.5, 0.0},
		{0.5, 0.0, 0.5},
		{0.0, 0.5, 0.5},
	};
}

// ---------------------------------------------------------------------------
// build_bcc_primitive  →  2-atom conventional cell
// ---------------------------------------------------------------------------
inline std::vector<std::array<double,3>> bcc_motif() {
	return {
		{0.0, 0.0, 0.0},
		{0.5, 0.5, 0.5},
	};
}

// ---------------------------------------------------------------------------
// build_b2_primitive  →  2-site ordered BCC (CsCl)
//   site 0 at corner, site 1 at body centre
// ---------------------------------------------------------------------------
inline std::vector<std::array<double,3>> b2_motif() {
	return {
		{0.0, 0.0, 0.0},
		{0.5, 0.5, 0.5},
	};
}

// ---------------------------------------------------------------------------
// build_hcp_primitive  →  2-atom hexagonal cell
//   a-axis along x; b-axis at 120° (= -60° from x); c-axis along z
// ---------------------------------------------------------------------------
//   Cartesian from fractional (hx, hy, hz):
//     x =  a * hx  +  a * cos(60°) * hy  =  a*(hx + 0.5*hy)
//     y =  a * sin(60°) * hy              =  a*(sqrt(3)/2)*hy
//     z =  c * hz
inline std::vector<std::array<double,3>> hcp_motif() {
	return {
		{0.000, 0.000, 0.000},
		{1.0/3, 2.0/3, 0.500},
	};
}

// ---------------------------------------------------------------------------
// Fractional → Cartesian (handles orthorhombic + HCP)
// ---------------------------------------------------------------------------
struct CellVectors {
	double a, c;
	LatticeType type;

	// Returns {x,y,z} Angstrom from fractional (fx,fy,fz)
	std::array<double,3> to_cart(double fx, double fy, double fz) const {
		if (type == LatticeType::HCP) {
			constexpr double cos60 = 0.5;
			constexpr double sin60 = 0.8660254037844387;
			return { a*(fx + cos60*fy), a*sin60*fy, c*fz };
		}
		// Cubic / orthorhombic (BCC, FCC, B2)
		return { a*fx, a*fy, a*fz };
	}
};

// ---------------------------------------------------------------------------
// Build supercell XYZFrame from a MaterialPreset
//   nx, ny, nz  — repetitions along a/b/c
//   apply_cycle — replace element symbols with alloy occupancy cycle
// ---------------------------------------------------------------------------
inline io::XYZFrame build_supercell(const MaterialPreset& mat,
									int nx, int ny, int nz,
									bool apply_cycle = true)
{
	// Choose Cartesian helper
	CellVectors cell{ mat.a, mat.c, mat.lattice };

	// Choose primitive-cell fractional positions (motif)
	std::vector<std::array<double,3>> motif;
	switch (mat.lattice) {
		case LatticeType::BCC: motif = bcc_motif(); break;
		case LatticeType::B2:  motif = b2_motif();  break;
		case LatticeType::FCC: motif = fcc_motif(); break;
		case LatticeType::HCP: motif = hcp_motif(); break;
	}

	// Alloy substitution cycle (empty → use basis symbols directly)
	std::vector<OccupancyCycle> cycle;
	if (apply_cycle) cycle = occupancy_cycle(mat.tag);

	// For B2 the basis provides two distinct species; we keep them in order.
	// For FCC/BCC with a cycle we substitute every atom index in round-robin.
	// For BCC pure (no cycle) we use basis[0] for every atom.

	io::XYZFrame frame;
	frame.energy      = 0.0;   // will accumulate
	frame.temperature = 300.0; // nominal build temperature

	const int n_basis = static_cast<int>(mat.basis.size());

	int atom_idx = 0;
	for (int ix = 0; ix < nx; ++ix)
	for (int iy = 0; iy < ny; ++iy)
	for (int iz = 0; iz < nz; ++iz)
	{
		for (int m = 0; m < static_cast<int>(motif.size()); ++m)
		{
			// Fractional coordinate in supercell
			double fx = (ix + motif[m][0]) / nx;
			double fy = (iy + motif[m][1]) / ny;
			double fz = (iz + motif[m][2]) / nz;

			// Absolute Cartesian (Å)
			auto [cx, cy, cz] = cell.to_cart(fx * nx, fy * ny, fz * nz);

			// Determine species + charge
			std::string sym;
			int         Z;
			double      q;

			if (!cycle.empty()) {
				// Random-alloy / substitutional: round-robin over cycle
				const auto& occ = cycle[atom_idx % static_cast<int>(cycle.size())];
				sym = occ.symbol;
				Z   = occ.Z;
				q   = occ.q;
			} else if (mat.lattice == LatticeType::B2) {
				// Ordered B2: alternating basis by motif index
				const auto& site = mat.basis[m % n_basis];
				sym = site.symbol;
				Z   = site.Z;
				q   = site.q_default;
			} else {
				// Pure metal: always basis[0]
				const auto& site = mat.basis[0];
				sym = site.symbol;
				Z   = site.Z;
				q   = site.q_default;
			}

			io::AtomRecord rec(Z, sym, cx, cy, cz);
			rec.q = q;
			rec.v = io::XYZVec3{0.0, 0.0, 0.0};   // zero velocity at build time
			rec.e = mat.ref_energy_per_atom;

			frame.atoms.push_back(rec);
			*frame.energy += mat.ref_energy_per_atom;
			++atom_idx;
		}
	}

	frame.N = static_cast<int>(frame.atoms.size());

	// Bounding box (supercell diagonal, PBC on all axes)
	io::XYZBox box;
	if (mat.lattice == LatticeType::HCP) {
		// HCP supercell: a-axis full length, b-axis = a*sqrt(3)*ny, c-axis
		constexpr double sqrt3 = 1.7320508075688772;
		box.ax = mat.a * nx;
		box.ay = mat.a * sqrt3 * ny;
		box.az = mat.c * nz;
		box.lattice = {
			box.ax, 0.0,   0.0,
			0.0,    box.ay, 0.0,
			0.0,    0.0,   box.az
		};
	} else {
		box.ax = mat.a * nx;
		box.ay = mat.a * ny;
		box.az = mat.a * nz;
		box.lattice = {
			box.ax, 0.0,   0.0,
			0.0,    box.ay, 0.0,
			0.0,    0.0,   box.az
		};
	}
	for (int i = 0; i < 3; ++i) box.pbc[i] = true;
	frame.box = box;

	// has_* flags
	frame.has_charge    = true;
	frame.has_velocity  = true;
	frame.has_energy_col = true;

	return frame;
}

// ---------------------------------------------------------------------------
// Build a trivial 3-frame trajectory (0K, 300K, 600K scaled displacements)
// for .xyzf demo output.  Velocities are non-physical thermal stand-ins.
// ---------------------------------------------------------------------------
inline std::vector<io::XYZFrame> build_thermal_trajectory(
	const MaterialPreset& mat, int nx, int ny, int nz,
	const std::vector<double>& temps = {0.0, 300.0, 600.0})
{
	// kB in kcal/mol/K units: 0.001987 kcal/(mol·K)
	constexpr double kB_kcal = 0.001987;

	auto base = build_supercell(mat, nx, ny, nz, true);

	std::vector<io::XYZFrame> traj;
	for (int fi = 0; fi < static_cast<int>(temps.size()); ++fi)
	{
		io::XYZFrame f = base;
		f.frame_index  = fi;
		f.temperature  = temps[fi];
		double T = temps[fi];

		// Assign isotropic thermal velocity magnitude:
		//   v_rms = sqrt(kB*T / m_amu)  in Å/fs (non-physical placeholder)
		//   We use m = 50 amu as a representative average.
		double v_rms = (T > 0.0) ? std::sqrt(kB_kcal * T / 50.0) * 0.01 : 0.0;

		// Cycle through ±x ±y ±z to avoid all atoms getting the same vector
		static const double dirs[6][3] = {
			{1,0,0},{0,1,0},{0,0,1},{-1,0,0},{0,-1,0},{0,0,-1}
		};
		for (int i = 0; i < static_cast<int>(f.atoms.size()); ++i) {
			const auto& d = dirs[i % 6];
			f.atoms[i].v = io::XYZVec3{ v_rms*d[0], v_rms*d[1], v_rms*d[2] };
		}

		// Scale comment
		std::ostringstream cmt;
		cmt << mat.name << " supercell " << nx << "x" << ny << "x" << nz
			<< " | E = " << std::fixed << std::setprecision(2) << *f.energy
			<< " kcal/mol | T = " << T << " K";
		f.comment = cmt.str();

		traj.push_back(std::move(f));
	}
	return traj;
}

} // namespace gen
} // namespace vsepr
