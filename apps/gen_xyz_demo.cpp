/**
 * gen_xyz_demo.cpp
 * ----------------
 * Generates all 8 XYZ-family demo files used for TUI integration testing.
 *
 * WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * Molecules:
 *   OsO4   — osmium tetroxide  (Z: Os=76, O=8)
 *             Td symmetry, d(Os-O) = 1.711 Å
 *             90 molecules × 5 atoms = 450 atoms
 *
 *   CH3HgI — methylmercury iodide (Z: C=6, H=1×3, Hg=80, I=53)
 *             Linear C–Hg–I backbone + methyl group
 *             75 molecules × 6 atoms = 450 atoms
 *
 * Files written to ./demo_xyz/ :
 *   osmium_tetroxide.xyz      .xyza     .xyzc     .xyzf
 *   methylmercury_iodide.xyz  .xyza     .xyzc     .xyzf
 *
 * Physical parameters used (spec §8 sanity ranges):
 *   Charges : O ≈ -0.83 e,  Os ≈ +3.32 e (formal +8, partial ~+3.3)
 *             Hg ≈ +0.50 e, I  ≈ -0.35 e, C ≈ -0.28 e, H ≈ +0.15 e
 *   Forces  : 1–40 kcal/(mol·Å)
 *   Velocity: ~0.02 Å/fs (300 K Maxwell-Boltzmann)
 *   Energy  : −500 to −1500 kcal/mol depending on molecule count
 */

#include "io/xyz_writer.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace vsepr::io;

// ============================================================================
// LCG-based deterministic RNG (no dependency on any vsepr random module)
// ============================================================================

struct DRng {
	uint64_t state;
	explicit DRng(uint64_t seed = 20240101) : state(seed) {}
	uint64_t next() {
		state ^= state << 13; state ^= state >> 7; state ^= state << 17;
		return state;
	}
	double uniform()    { return (next() & 0xFFFFFF) / double(0x1000000); }
	double gaussian() {
		// Box-Muller
		double u = uniform() + 1e-12, v = uniform();
		return std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * M_PI * v);
	}
	double range(double lo, double hi) { return lo + (hi - lo) * uniform(); }
};

// ============================================================================
// Molecule geometry builders
// ============================================================================

// OsO4 — Td (tetrahedral), Os at origin, 4 O at d=1.711 Å
// Tetrahedral O positions (normalised):
//   (+1, +1, -1)/√3
//   (+1, -1, +1)/√3
//   (-1, +1, +1)/√3
//   (-1, -1, -1)/√3

static std::vector<AtomRecord> build_OsO4(double cx, double cy, double cz, DRng& rng) {
	const double d = 1.711; // Os-O bond Å
	const double s = d / std::sqrt(3.0);
	const double Os_offsets[4][3] = {
		{+s, +s, -s}, {+s, -s, +s}, {-s, +s, +s}, {-s, -s, -s}
	};

	auto vel = [&](double mass_factor) -> XYZVec3 {
		// v_rms ~ sqrt(kB T / m), scale so typical |v| ~ 0.02 Å/fs at 300 K
		double sigma = 0.020 / std::sqrt(mass_factor);
		return {rng.gaussian() * sigma,
				rng.gaussian() * sigma,
				rng.gaussian() * sigma};
	};
	auto force = [&](double mag) -> XYZVec3 {
		double fx = rng.gaussian(), fy = rng.gaussian(), fz = rng.gaussian();
		double norm = std::sqrt(fx*fx + fy*fy + fz*fz) + 1e-12;
		return {fx/norm*mag, fy/norm*mag, fz/norm*mag};
	};

	std::vector<AtomRecord> atoms;

	// Os
	AtomRecord os;
	os.Z = 76; os.symbol = Z_to_symbol(76);
	os.x = cx; os.y = cy; os.z = cz;
	os.q  = +3.32;
	os.v  = vel(190.23);         // Os atomic mass
	os.f  = force(rng.range(2.0, 20.0));
	os.e  = -250.0 + rng.range(-5.0, 5.0);
	atoms.push_back(os);

	// 4 × O
	for (int k = 0; k < 4; ++k) {
		AtomRecord o;
		o.Z = 8; o.symbol = Z_to_symbol(8);
		o.x = cx + Os_offsets[k][0];
		o.y = cy + Os_offsets[k][1];
		o.z = cz + Os_offsets[k][2];
		o.q  = -0.83;
		o.v  = vel(16.0);
		o.f  = force(rng.range(5.0, 35.0));
		o.e  = -60.0 + rng.range(-2.0, 2.0);
		atoms.push_back(o);
	}
	return atoms;
}

// CH3HgI — C-Hg-I linear backbone + methyl H's
// Bond lengths: C-Hg 2.07 Å, Hg-I 2.73 Å, C-H 1.09 Å
// Place backbone along X; rotate H's to methyl geometry

static std::vector<AtomRecord> build_CH3HgI(double cx, double cy, double cz, DRng& rng) {
	auto vel = [&](double mf) -> XYZVec3 {
		double sigma = 0.020 / std::sqrt(mf);
		return {rng.gaussian()*sigma, rng.gaussian()*sigma, rng.gaussian()*sigma};
	};
	auto force = [&](double mag) -> XYZVec3 {
		double fx = rng.gaussian(), fy = rng.gaussian(), fz = rng.gaussian();
		double norm = std::sqrt(fx*fx + fy*fy + fz*fz) + 1e-12;
		return {fx/norm*mag, fy/norm*mag, fz/norm*mag};
	};
	auto make_atom = [&](int Z, double x, double y, double z,
						 double q, double mf) -> AtomRecord {
		AtomRecord a;
		a.Z = Z; a.symbol = Z_to_symbol(Z);
		a.x = x; a.y = y; a.z = z;
		a.q = q;
		a.v = vel(mf);
		a.f = force(rng.range(2.0, 30.0));
		a.e = -40.0 * Z * 0.01 + rng.range(-1.0, 1.0);
		return a;
	};

	// Backbone: C at -2.07, Hg at 0, I at +2.73 (all along X from cx)
	const double cHg = 2.07, HgI = 2.73, cH  = 1.09;
	double xC = cx - cHg, xHg = cx, xI = cx + HgI;

	std::vector<AtomRecord> atoms;
	atoms.push_back(make_atom(6,  xC,  cy, cz, -0.28, 12.011));   // C
	atoms.push_back(make_atom(80, xHg, cy, cz, +0.50, 200.59));   // Hg
	atoms.push_back(make_atom(53, xI,  cy, cz, -0.35, 126.90));   // I

	// 3 × H in methyl (tetrahedral around C, away from Hg)
	// H positions: cone with half-angle 109.5°/2 = 54.75°, distributed 120° apart
	const double theta = 54.75 * M_PI / 180.0;
	for (int k = 0; k < 3; ++k) {
		double phi = k * 2.0 * M_PI / 3.0;
		double hx = xC - cH * std::cos(theta);
		double hy = cy + cH * std::sin(theta) * std::cos(phi);
		double hz = cz + cH * std::sin(theta) * std::sin(phi);
		atoms.push_back(make_atom(1, hx, hy, hz, +0.15, 1.008));
	}
	return atoms;
}

// ============================================================================
// Pack N molecules on a cubic grid
// ============================================================================

struct MolPack {
	std::vector<AtomRecord> atoms;
	XYZBox                  box;
	double                  energy = 0.0;
	double                  temperature = 300.0;
};

template<typename Builder>
MolPack pack_molecules(int N_mol, double spacing, Builder builder, DRng& rng) {
	int side = static_cast<int>(std::ceil(std::cbrt(N_mol)));
	MolPack mp;
	mp.box.ax = side * spacing;
	mp.box.ay = side * spacing;
	mp.box.az = side * spacing;
	mp.box.pbc[0] = true; mp.box.pbc[1] = true; mp.box.pbc[2] = true;

	int count = 0;
	for (int iz = 0; iz < side && count < N_mol; ++iz)
	for (int iy = 0; iy < side && count < N_mol; ++iy)
	for (int ix = 0; ix < side && count < N_mol; ++ix, ++count) {
		double cx = (ix + 0.5) * spacing;
		double cy = (iy + 0.5) * spacing;
		double cz = (iz + 0.5) * spacing;
		auto mol = builder(cx, cy, cz, rng);
		for (auto& a : mol) mp.atoms.push_back(a);
	}

	// Crude energy: sum per-atom energies
	for (const auto& a : mp.atoms)
		if (a.e) mp.energy += *a.e;

	return mp;
}

// ============================================================================
// Frame builder helpers
// ============================================================================

static XYZFrame make_frame(const MolPack& mp,
							const std::string& name,
							int frame_idx = 0,
							bool xyza_fields = true)
{
	XYZFrame f;
	f.N           = static_cast<int>(mp.atoms.size());
	f.frame_index = frame_idx;
	f.energy      = mp.energy;
	f.temperature = mp.temperature;
	f.box         = mp.box;
	f.has_charge   = xyza_fields;
	f.has_velocity = xyza_fields;
	f.has_force    = xyza_fields;
	f.has_energy_col = xyza_fields;

	// Build comment line per spec: <name> | E = <val> kcal/mol | T = <T> K
	char buf[160];
	std::snprintf(buf, sizeof(buf),
		"%s | E = %.4f kcal/mol | T = %.1f K | step %d",
		name.c_str(), mp.energy, mp.temperature, frame_idx);
	f.comment = buf;

	// Copy atoms (strip optional fields for .xyz)
	f.atoms.reserve(mp.atoms.size());
	for (auto a : mp.atoms) {
		if (!xyza_fields) {
			a.q = std::nullopt;
			a.v = std::nullopt;
			a.f = std::nullopt;
			a.e = std::nullopt;
		}
		f.atoms.push_back(a);
	}
	return f;
}

// Displace atoms slightly for trajectory frames
static MolPack jitter_frame(const MolPack& base, DRng& rng, double sigma = 0.02) {
	MolPack mp = base;
	for (auto& a : mp.atoms) {
		a.x += rng.gaussian() * sigma;
		a.y += rng.gaussian() * sigma;
		a.z += rng.gaussian() * sigma;
		// Update forces to reflect random perturbation
		if (a.f) {
			a.f->x += rng.gaussian() * 0.5;
			a.f->y += rng.gaussian() * 0.5;
			a.f->z += rng.gaussian() * 0.5;
		}
	}
	mp.energy += rng.range(-2.0, 2.0);
	mp.temperature = 300.0 + rng.gaussian() * 5.0;
	return mp;
}

// ============================================================================
// Write all 8 files
// ============================================================================

static void write_all(const std::string& outdir,
					   const std::string& stem,
					   const std::string& mol_name,
					   const MolPack& base_pack,
					   DRng& rng)
{
	fs::create_directories(outdir);

	XYZWriterConfig cfg;
	cfg.prop_precision = 6;

	// ----- .xyz (static, coords only) -----
	{
		auto frame = make_frame(base_pack, mol_name, 0, false);
		write_xyz(outdir + "/" + stem + ".xyz", frame, cfg);
		std::cout << "  wrote " << stem << ".xyz  (" << frame.N << " atoms)\n";
	}

	// ----- .xyza (one frame, extended properties) -----
	{
		auto frame = make_frame(base_pack, mol_name, 0, true);
		write_xyza(outdir + "/" + stem + ".xyza", frame, cfg);
		std::cout << "  wrote " << stem << ".xyza (" << frame.N << " atoms)\n";
	}

	// ----- .xyzc (checkpoint) -----
	{
		XYZData data;
		data.frames.push_back(make_frame(base_pack, mol_name, 0, true));

		CheckpointState ck;
		ck.step      = 0;
		ck.time      = 0.0;
		ck.dt        = 0.001;          // 1 fs
		ck.T_target  = 300.0;
		ck.seed      = 20240101;
		ck.box       = base_pack.box;
		data.checkpoint = ck;

		write_xyzc(outdir + "/" + stem + ".xyzc", data, cfg);
		std::cout << "  wrote " << stem << ".xyzc (checkpoint, " << data.frames[0].N << " atoms)\n";
	}

	// ----- .xyzf (5-frame trajectory) -----
	{
		XYZData traj;
		MolPack current = base_pack;
		for (int fi = 0; fi < 5; ++fi) {
			auto frame = make_frame(current, mol_name, fi, true);
			traj.frames.push_back(frame);
			if (fi < 4) current = jitter_frame(current, rng);
		}
		write_xyzf(outdir + "/" + stem + ".xyzf", traj.frames, false, cfg);
		std::cout << "  wrote " << stem << ".xyzf (5 frames, "
				  << traj.frames[0].N << " atoms/frame)\n";
	}
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
	std::string outdir = "demo_xyz";
	if (argc >= 2) outdir = argv[1];

	DRng rng(20240314);

	std::cout << "\n=== gen_xyz_demo  (DEV54-XYZ-SPEC-INTEGRATION) ===\n\n";

	// ---- OsO4 : 90 molecules × 5 atoms = 450 ----
	std::cout << "OsO4 (osmium tetroxide)  — 90 mol × 5 atoms = 450\n";
	{
		// spacing 8 Å gives comfortable room around each OsO4 (≈4 Å diameter)
		auto pack = pack_molecules(90, 8.0,
			[](double cx, double cy, double cz, DRng& r) {
				return build_OsO4(cx, cy, cz, r);
			}, rng);
		std::cout << "  total atoms: " << pack.atoms.size() << "\n";
		write_all(outdir, "osmium_tetroxide", "OsO4", pack, rng);
	}

	std::cout << "\n";

	// ---- CH3HgI : 75 molecules × 6 atoms = 450 ----
	std::cout << "CH3HgI (methylmercury iodide)  — 75 mol × 6 atoms = 450\n";
	{
		// spacing 9 Å (molecule end-to-end ≈ 6.9 Å)
		auto pack = pack_molecules(75, 9.0,
			[](double cx, double cy, double cz, DRng& r) {
				return build_CH3HgI(cx, cy, cz, r);
			}, rng);
		std::cout << "  total atoms: " << pack.atoms.size() << "\n";
		write_all(outdir, "methylmercury_iodide", "CH3HgI", pack, rng);
	}

	std::cout << "\n=== Done. Files written to: " << outdir << "/ ===\n\n";
	return 0;
}
