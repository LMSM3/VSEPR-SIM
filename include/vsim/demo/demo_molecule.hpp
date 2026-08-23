#pragma once
/**
 * demo_molecule.hpp
 * =================
 * VSEPR-SIM  |  vsepr --demo  |  Kernel-embedded rotating molecule viewer
 *
 * Entirely self-contained (stdlib only).  No external data files, no GUI.
 * Renders a slowly-rotating molecule in the terminal using ANSI escape codes
 * and ASCII depth-sorted projection.
 *
 * Entry point:
 *   int vsim::demo::run_demo(int argc, char** argv);
 *
 * Optional flags (parsed here, not in vsepr.cpp):
 *   --molecule <name>   Force a specific molecule (H2O, CH4, NH3, SF6, ...)
 *   --fps <n>           Frames per second (default 10)
 *   --frames <n>        Total frames then exit (0 = infinite, default 0)
 *   --seed <n>          RNG seed for molecule selection (default: time)
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <csignal>
#include <cstdlib>

namespace vsim {
namespace demo {

// ============================================================================
// Geometry types (private to this header)
// ============================================================================

struct DemoAtom {
	double      x, y, z;   // Angstrom, centred at origin
	std::string symbol;
	uint32_t    color_rgb;  // CPK-ish packed RGB for ANSI truecolor
};

struct DemoBond {
	int a, b;               // indices into DemoAtom array
};

struct DemoMolecule {
	std::string          name;
	std::string          formula;
	std::vector<DemoAtom> atoms;
	std::vector<DemoBond> bonds;
};

// ============================================================================
// Hardcoded molecule library (VSEPR geometries, Angstrom, centred)
// ============================================================================

inline std::vector<DemoMolecule> build_molecule_library() {
	std::vector<DemoMolecule> lib;

	// H2O  — bent, ~104.5 deg
	{
		DemoMolecule m;
		m.name = "H2O"; m.formula = "H2O";
		double r = 0.96, ang = 104.45 * M_PI / 180.0;
		m.atoms = {
			{ 0.0,  0.0,  0.0,   "O", 0xFF2200 },
			{ r * std::sin(ang/2),  -r * std::cos(ang/2), 0.0, "H", 0xFFFFFF },
			{-r * std::sin(ang/2),  -r * std::cos(ang/2), 0.0, "H", 0xFFFFFF },
		};
		m.bonds = { {0,1},{0,2} };
		lib.push_back(std::move(m));
	}

	// NH3  — trigonal pyramidal
	{
		DemoMolecule m;
		m.name = "NH3"; m.formula = "NH3";
		double rNH = 1.012, ang = 107.8 * M_PI / 180.0;
		double h = rNH * std::cos((M_PI - ang) / 2.0);
		double rxy = rNH * std::sin((M_PI - ang) / 2.0);
		m.atoms = {
			{ 0.0, 0.3, 0.0, "N", 0x2244FF },
			{ rxy,         -h, 0.0,                  "H", 0xFFFFFF },
			{ rxy*std::cos(2*M_PI/3), -h, rxy*std::sin(2*M_PI/3), "H", 0xFFFFFF },
			{ rxy*std::cos(4*M_PI/3), -h, rxy*std::sin(4*M_PI/3), "H", 0xFFFFFF },
		};
		m.bonds = { {0,1},{0,2},{0,3} };
		lib.push_back(std::move(m));
	}

	// CH4  — tetrahedral
	{
		DemoMolecule m;
		m.name = "CH4"; m.formula = "CH4";
		double r = 1.09;
		double s = r / std::sqrt(3.0);
		m.atoms = {
			{  0.0,  0.0,  0.0, "C", 0x606060 },
			{  s,    s,    s,   "H", 0xFFFFFF },
			{ -s,   -s,    s,   "H", 0xFFFFFF },
			{  s,   -s,   -s,   "H", 0xFFFFFF },
			{ -s,    s,   -s,   "H", 0xFFFFFF },
		};
		m.bonds = { {0,1},{0,2},{0,3},{0,4} };
		lib.push_back(std::move(m));
	}

	// CO2  — linear
	{
		DemoMolecule m;
		m.name = "CO2"; m.formula = "CO2";
		m.atoms = {
			{ -1.16, 0.0, 0.0, "O", 0xFF2200 },
			{  0.0,  0.0, 0.0, "C", 0x606060 },
			{  1.16, 0.0, 0.0, "O", 0xFF2200 },
		};
		m.bonds = { {0,1},{1,2} };
		lib.push_back(std::move(m));
	}

	// BF3  — trigonal planar
	{
		DemoMolecule m;
		m.name = "BF3"; m.formula = "BF3";
		double r = 1.30;
		for (int i = 0; i < 3; ++i) {
			double ang = i * 2.0 * M_PI / 3.0;
			m.atoms.push_back({ r*std::cos(ang), r*std::sin(ang), 0.0, "F", 0x44DD44 });
		}
		m.atoms.push_back({ 0.0, 0.0, 0.0, "B", 0xFFB5B5 });
		m.bonds = { {3,0},{3,1},{3,2} };
		lib.push_back(std::move(m));
	}

	// SF6  — octahedral
	{
		DemoMolecule m;
		m.name = "SF6"; m.formula = "SF6";
		double r = 1.56;
		m.atoms = {
			{  0.0,  0.0,  0.0, "S", 0xFFFF30 },
			{  r,    0.0,  0.0, "F", 0x44DD44 },
			{ -r,    0.0,  0.0, "F", 0x44DD44 },
			{  0.0,  r,    0.0, "F", 0x44DD44 },
			{  0.0, -r,    0.0, "F", 0x44DD44 },
			{  0.0,  0.0,  r,   "F", 0x44DD44 },
			{  0.0,  0.0, -r,   "F", 0x44DD44 },
		};
		m.bonds = { {0,1},{0,2},{0,3},{0,4},{0,5},{0,6} };
		lib.push_back(std::move(m));
	}

	// PCl5  — trigonal bipyramidal
	{
		DemoMolecule m;
		m.name = "PCl5"; m.formula = "PCl5";
		double req = 2.02, rax = 2.14;
		m.atoms.push_back({ 0.0, 0.0,  0.0, "P",  0xFF8C00 });
		for (int i = 0; i < 3; ++i) {
			double ang = i * 2.0 * M_PI / 3.0;
			m.atoms.push_back({ req*std::cos(ang), 0.0, req*std::sin(ang), "Cl", 0x1FF01F });
		}
		m.atoms.push_back({ 0.0,  rax, 0.0, "Cl", 0x1FF01F });
		m.atoms.push_back({ 0.0, -rax, 0.0, "Cl", 0x1FF01F });
		for (int i = 1; i <= 5; ++i) m.bonds.push_back({0, i});
		lib.push_back(std::move(m));
	}

	// XeF4  — square planar
	{
		DemoMolecule m;
		m.name = "XeF4"; m.formula = "XeF4";
		double r = 1.95;
		m.atoms = {
			{  0.0, 0.0, 0.0, "Xe", 0x429EB0 },
			{  r,   0.0, 0.0, "F",  0x44DD44 },
			{ -r,   0.0, 0.0, "F",  0x44DD44 },
			{  0.0, 0.0,  r,  "F",  0x44DD44 },
			{  0.0, 0.0, -r,  "F",  0x44DD44 },
		};
		m.bonds = { {0,1},{0,2},{0,3},{0,4} };
		lib.push_back(std::move(m));
	}

	// C6H6  — benzene ring (flat, 6-fold)
	{
		DemoMolecule m;
		m.name = "C6H6"; m.formula = "C6H6";
		double rCC = 1.40, rCH = 1.09;
		for (int i = 0; i < 6; ++i) {
			double ang = i * M_PI / 3.0;
			m.atoms.push_back({ rCC*std::cos(ang), rCC*std::sin(ang), 0.0, "C", 0x606060 });
		}
		for (int i = 0; i < 6; ++i) {
			double ang = i * M_PI / 3.0;
			m.atoms.push_back({ (rCC+rCH)*std::cos(ang), (rCC+rCH)*std::sin(ang), 0.0, "H", 0xFFFFFF });
		}
		for (int i = 0; i < 6; ++i) {
			m.bonds.push_back({ i, (i+1)%6 });
			m.bonds.push_back({ i, i+6 });
		}
		lib.push_back(std::move(m));
	}

	// H2O2  — hydrogen peroxide, non-planar dihedral ~111 deg
	{
		DemoMolecule m;
		m.name = "H2O2"; m.formula = "H2O2";
		double rOO = 1.45, rOH = 0.96;
		double dih = 111.5 * M_PI / 180.0;
		double angHOO = 100.0 * M_PI / 180.0;
		m.atoms = {
			{ -rOO/2, 0.0, 0.0, "O", 0xFF2200 },
			{  rOO/2, 0.0, 0.0, "O", 0xFF2200 },
			// H on O1: in xz plane
			{ -rOO/2 - rOH*std::cos(M_PI - angHOO), rOH*std::sin(M_PI - angHOO), 0.0, "H", 0xFFFFFF },
			// H on O2: rotated by dihedral
			{  rOO/2 + rOH*std::cos(M_PI - angHOO),
			   rOH*std::sin(M_PI - angHOO)*std::cos(dih),
			   rOH*std::sin(M_PI - angHOO)*std::sin(dih), "H", 0xFFFFFF },
		};
		m.bonds = { {0,1},{0,2},{1,3} };
		lib.push_back(std::move(m));
	}

	// SO3  — trigonal planar, D3h
	{
		DemoMolecule m;
		m.name = "SO3"; m.formula = "SO3";
		double r = 1.42;
		for (int i = 0; i < 3; ++i) {
			double ang = i * 2.0 * M_PI / 3.0 + M_PI / 6.0;
			m.atoms.push_back({ r*std::cos(ang), r*std::sin(ang), 0.0, "O", 0xFF2200 });
		}
		m.atoms.push_back({ 0.0, 0.0, 0.0, "S", 0xFFFF30 });
		m.bonds = { {3,0},{3,1},{3,2} };
		lib.push_back(std::move(m));
	}

	// --demo12  |  Rnd3  --  randomized 3-component molecule (WO-72D extra credit)
	// Three distinct elements chosen from a curated palette, assembled into
	// one of three VSEPR geometries selected by the runtime seed.
	// Unique every run when no --seed is supplied.
	{
		struct ElemSpec { const char* sym; uint32_t rgb; double r; };
		static const ElemSpec palette[] = {
			{"C",  0x606060, 0.77}, {"N",  0x2244FF, 0.71}, {"O",  0xFF2200, 0.66},
			{"S",  0xFFFF30, 1.05}, {"P",  0xFF8800, 1.07}, {"F",  0x44FF44, 0.57},
			{"Cl", 0x20F020, 1.02}, {"B",  0xFF6622, 0.84}, {"Si", 0xF0C89C, 1.11},
			{"Br", 0xA62020, 1.20}, {"I",  0x940094, 1.39}, {"Se", 0xFFAA00, 1.20},
		};
		static constexpr int PSIZ = (int)(sizeof(palette)/sizeof(palette[0]));

		auto ts = static_cast<unsigned long>(
			std::chrono::system_clock::now().time_since_epoch().count() & 0xFFFFFFFFUL);
		auto pick = [&](int n) -> int { ts = ts * 1664525u + 1013904223u; return (int)((ts >> 16) % (unsigned)n); };

		int i0 = pick(PSIZ);
		int i1 = (i0 + 1 + pick(PSIZ - 1)) % PSIZ;
		int i2 = (i1 + 1 + pick(PSIZ - 2)) % PSIZ;
		if (i2 == i0) i2 = (i2 + 1) % PSIZ;

		const ElemSpec& A = palette[i0];
		const ElemSpec& B = palette[i1];
		const ElemSpec& C = palette[i2];

		int geom = pick(3);

		DemoMolecule m;
		m.name    = std::string("Rnd3[") + A.sym + B.sym + C.sym + "]";
		m.formula = std::string(A.sym) + B.sym + C.sym;

		double rAB = (A.r + B.r) * 0.95;
		double rAC = (A.r + C.r) * 0.95;

		if (geom == 0) {
			m.atoms = {
				{ 0.0,  0.0, 0.0, A.sym, A.rgb },
				{ rAB,  0.0, 0.0, B.sym, B.rgb },
				{-rAC,  0.0, 0.0, C.sym, C.rgb },
			};
		} else if (geom == 1) {
			double ang = 120.0 * M_PI / 180.0;
			m.atoms = {
				{  0.0,                    0.0, 0.0, A.sym, A.rgb },
				{  rAB*std::sin(ang/2), -rAB*std::cos(ang/2), 0.0, B.sym, B.rgb },
				{ -rAC*std::sin(ang/2), -rAC*std::cos(ang/2), 0.0, C.sym, C.rgb },
			};
		} else {
			double ang = 105.0 * M_PI / 180.0;
			m.atoms = {
				{  0.0,                    0.0, 0.0, A.sym, A.rgb },
				{  rAB*std::sin(ang/2), -rAB*std::cos(ang/2), 0.0, B.sym, B.rgb },
				{ -rAC*std::sin(ang/2), -rAC*std::cos(ang/2), 0.0, C.sym, C.rgb },
			};
		}
		m.bonds = { {0,1}, {0,2} };
		lib.push_back(std::move(m));
	}

	// NO2  — bent radical, ~134 deg
	{
		DemoMolecule m;
		m.name = "NO2"; m.formula = "NO2";
		double r = 1.20, ang = 134.1 * M_PI / 180.0;
		m.atoms = {
			{  0.0,  0.0,  0.0,   "N", 0x2244FF },
			{  r * std::sin(ang/2), -r * std::cos(ang/2), 0.0, "O", 0xFF2200 },
			{ -r * std::sin(ang/2), -r * std::cos(ang/2), 0.0, "O", 0xFF2200 },
		};
		m.bonds = { {0,1},{0,2} };
		lib.push_back(std::move(m));
	}

	// PF3  — trigonal pyramidal
	{
		DemoMolecule m;
		m.name = "PF3"; m.formula = "PF3";
		double rPF = 1.57, ang = 97.8 * M_PI / 180.0;
		double h  = rPF * std::cos((M_PI - ang) / 2.0);
		double rxy = rPF * std::sin((M_PI - ang) / 2.0);
		m.atoms = {
			{ 0.0, 0.3, 0.0, "P", 0xFF8C00 },
			{ rxy,                                              -h, 0.0,                   "F", 0x44DD44 },
			{ rxy*std::cos(2*M_PI/3), -h, rxy*std::sin(2*M_PI/3),  "F", 0x44DD44 },
			{ rxy*std::cos(4*M_PI/3), -h, rxy*std::sin(4*M_PI/3),  "F", 0x44DD44 },
		};
		m.bonds = { {0,1},{0,2},{0,3} };
		lib.push_back(std::move(m));
	}

	// IF5  — square pyramidal (one axial + 4 equatorial, lone pair below)
	{
		DemoMolecule m;
		m.name = "IF5"; m.formula = "IF5";
		double rax = 1.87, req = 1.87;
		m.atoms = {
			{  0.0,  0.0,  0.0, "I",  0x940094 },
			{  0.0,  rax,  0.0, "F",  0x44DD44 },   // axial
			{  req,  0.15, 0.0, "F",  0x44DD44 },
			{ -req,  0.15, 0.0, "F",  0x44DD44 },
			{  0.0,  0.15, req, "F",  0x44DD44 },
			{  0.0,  0.15,-req, "F",  0x44DD44 },
		};
		for (int i = 1; i <= 5; ++i) m.bonds.push_back({0, i});
		lib.push_back(std::move(m));
	}

	// ClF3  — T-shaped (2 lone pairs equatorial)
	{
		DemoMolecule m;
		m.name = "ClF3"; m.formula = "ClF3";
		double rax = 1.70, req = 1.60;
		m.atoms = {
			{  0.0,  0.0,  0.0, "Cl", 0x1FF01F },
			{  0.0,  rax,  0.0, "F",  0x44DD44 },   // axial up
			{  0.0, -rax,  0.0, "F",  0x44DD44 },   // axial down
			{  req,  0.0,  0.0, "F",  0x44DD44 },   // equatorial
		};
		m.bonds = { {0,1},{0,2},{0,3} };
		lib.push_back(std::move(m));
	}

	// N2O  — linear (N-N-O)
	{
		DemoMolecule m;
		m.name = "N2O"; m.formula = "N2O";
		m.atoms = {
			{ -1.13, 0.0, 0.0, "N", 0x2244FF },
			{  0.0,  0.0, 0.0, "N", 0x2244FF },
			{  1.19, 0.0, 0.0, "O", 0xFF2200 },
		};
		m.bonds = { {0,1},{1,2} };
		lib.push_back(std::move(m));
	}

	// C2H2  — acetylene, linear
	{
		DemoMolecule m;
		m.name = "C2H2"; m.formula = "C2H2";
		m.atoms = {
			{ -1.21, 0.0, 0.0, "C", 0x606060 },
			{  0.0,  0.0, 0.0, "C", 0x606060 },
			{  1.21, 0.0, 0.0, "C", 0x606060 },   // placeholder — actually H
			{ -2.27, 0.0, 0.0, "H", 0xFFFFFF },
			{  2.27, 0.0, 0.0, "H", 0xFFFFFF },
		};
		// fix: middle atom should be second C, then H on each end
		m.atoms[0] = { -0.605, 0.0, 0.0, "C", 0x606060 };
		m.atoms[1] = {  0.605, 0.0, 0.0, "C", 0x606060 };
		m.atoms[2] = { -1.665, 0.0, 0.0, "H", 0xFFFFFF };
		m.atoms[3] = {  1.665, 0.0, 0.0, "H", 0xFFFFFF };
		m.atoms.resize(4);
		m.bonds = { {0,1},{0,2},{1,3} };
		lib.push_back(std::move(m));
	}

	// C2H4  — ethylene, planar
	{
		DemoMolecule m;
		m.name = "C2H4"; m.formula = "C2H4";
		double rCC = 1.34, rCH = 1.08;
		double angHCC = 121.3 * M_PI / 180.0;
		double hx = rCH * std::cos(M_PI - angHCC);
		double hy = rCH * std::sin(M_PI - angHCC);
		m.atoms = {
			{ -rCC/2,  0.0,  0.0, "C", 0x606060 },
			{  rCC/2,  0.0,  0.0, "C", 0x606060 },
			{ -rCC/2 + hx,  hy, 0.0, "H", 0xFFFFFF },
			{ -rCC/2 + hx, -hy, 0.0, "H", 0xFFFFFF },
			{  rCC/2 - hx,  hy, 0.0, "H", 0xFFFFFF },
			{  rCC/2 - hx, -hy, 0.0, "H", 0xFFFFFF },
		};
		m.bonds = { {0,1},{0,2},{0,3},{1,4},{1,5} };
		lib.push_back(std::move(m));
	}

	// ICl3  — T-shaped (heavier halogen version of ClF3)
	{
		DemoMolecule m;
		m.name = "ICl3"; m.formula = "ICl3";
		double rax = 2.38, req = 2.34;
		m.atoms = {
			{  0.0,  0.0,  0.0, "I",  0x940094 },
			{  0.0,  rax,  0.0, "Cl", 0x1FF01F },
			{  0.0, -rax,  0.0, "Cl", 0x1FF01F },
			{  req,  0.0,  0.0, "Cl", 0x1FF01F },
		};
		m.bonds = { {0,1},{0,2},{0,3} };
		lib.push_back(std::move(m));
	}

	return lib;
}

// ============================================================================
// Element tour library  (Z = 1 .. 102)
// Each entry is a single-atom DemoMolecule — no bonds, positioned at origin.
// CPK colours approximate the Jmol / VESTA convention.
// ============================================================================

struct ElemInfo {
	int         Z;
	const char* symbol;
	const char* name;
	uint32_t    cpk_rgb;     // Jmol-approximate CPK colour
	double      r_cov;       // covalent radius in Angstrom (for display scale)
};

// clang-format off
inline const ElemInfo& elem_info(int Z) {
	static const ElemInfo TABLE[102] = {
		{  1, "H",  "Hydrogen",      0xFFFFFF, 0.31},
		{  2, "He", "Helium",        0xD9FFFF, 0.28},
		{  3, "Li", "Lithium",       0xCC80FF, 1.28},
		{  4, "Be", "Beryllium",     0xC2FF00, 0.96},
		{  5, "B",  "Boron",         0xFFB5B5, 0.84},
		{  6, "C",  "Carbon",        0x909090, 0.77},
		{  7, "N",  "Nitrogen",      0x3050F8, 0.71},
		{  8, "O",  "Oxygen",        0xFF0D0D, 0.66},
		{  9, "F",  "Fluorine",      0x90E050, 0.57},
		{ 10, "Ne", "Neon",          0xB3E3F5, 0.58},
		{ 11, "Na", "Sodium",        0xAB5CF2, 1.66},
		{ 12, "Mg", "Magnesium",     0x8AFF00, 1.41},
		{ 13, "Al", "Aluminium",     0xBFA6A6, 1.21},
		{ 14, "Si", "Silicon",       0xF0C8A0, 1.11},
		{ 15, "P",  "Phosphorus",    0xFF8000, 1.07},
		{ 16, "S",  "Sulfur",        0xFFFF30, 1.05},
		{ 17, "Cl", "Chlorine",      0x1FF01F, 1.02},
		{ 18, "Ar", "Argon",         0x80D1E3, 1.06},
		{ 19, "K",  "Potassium",     0x8F40D4, 2.03},
		{ 20, "Ca", "Calcium",       0x3DFF00, 1.76},
		{ 21, "Sc", "Scandium",      0xE6E6E6, 1.70},
		{ 22, "Ti", "Titanium",      0xBFC2C7, 1.60},
		{ 23, "V",  "Vanadium",      0xA6A6AB, 1.53},
		{ 24, "Cr", "Chromium",      0x8A99C7, 1.39},
		{ 25, "Mn", "Manganese",     0x9C7AC7, 1.39},
		{ 26, "Fe", "Iron",          0xE06633, 1.32},
		{ 27, "Co", "Cobalt",        0xF090A0, 1.26},
		{ 28, "Ni", "Nickel",        0x50D050, 1.24},
		{ 29, "Cu", "Copper",        0xC88033, 1.32},
		{ 30, "Zn", "Zinc",          0x7D80B0, 1.22},
		{ 31, "Ga", "Gallium",       0xC28F8F, 1.22},
		{ 32, "Ge", "Germanium",     0x668F8F, 1.20},
		{ 33, "As", "Arsenic",       0xBD80E3, 1.19},
		{ 34, "Se", "Selenium",      0xFFA100, 1.20},
		{ 35, "Br", "Bromine",       0xA62929, 1.20},
		{ 36, "Kr", "Krypton",       0x5CB8D1, 1.16},
		{ 37, "Rb", "Rubidium",      0x702EB0, 2.20},
		{ 38, "Sr", "Strontium",     0x00FF00, 1.95},
		{ 39, "Y",  "Yttrium",       0x94FFFF, 1.90},
		{ 40, "Zr", "Zirconium",     0x94E0E0, 1.75},
		{ 41, "Nb", "Niobium",       0x73C2C9, 1.64},
		{ 42, "Mo", "Molybdenum",    0x54B5B5, 1.54},
		{ 43, "Tc", "Technetium",    0x3B9E9E, 1.47},
		{ 44, "Ru", "Ruthenium",     0x248F8F, 1.46},
		{ 45, "Rh", "Rhodium",       0x0A7D8C, 1.42},
		{ 46, "Pd", "Palladium",     0x006985, 1.39},
		{ 47, "Ag", "Silver",        0xC0C0C0, 1.45},
		{ 48, "Cd", "Cadmium",       0xFFD98F, 1.44},
		{ 49, "In", "Indium",        0xA67573, 1.42},
		{ 50, "Sn", "Tin",           0x668080, 1.39},
		{ 51, "Sb", "Antimony",      0x9E63B5, 1.39},
		{ 52, "Te", "Tellurium",     0xD47A00, 1.38},
		{ 53, "I",  "Iodine",        0x940094, 1.39},
		{ 54, "Xe", "Xenon",         0x429EB0, 1.40},
		{ 55, "Cs", "Caesium",       0x57178F, 2.44},
		{ 56, "Ba", "Barium",        0x00C900, 2.15},
		{ 57, "La", "Lanthanum",     0x70D4FF, 2.07},
		{ 58, "Ce", "Cerium",        0xFFFFC7, 2.04},
		{ 59, "Pr", "Praseodymium",  0xD9FFC7, 2.03},
		{ 60, "Nd", "Neodymium",     0xC7FFC7, 2.01},
		{ 61, "Pm", "Promethium",    0xA3FFC7, 1.99},
		{ 62, "Sm", "Samarium",      0x8FFFC7, 1.98},
		{ 63, "Eu", "Europium",      0x61FFC7, 1.98},
		{ 64, "Gd", "Gadolinium",    0x45FFC7, 1.96},
		{ 65, "Tb", "Terbium",       0x30FFC7, 1.94},
		{ 66, "Dy", "Dysprosium",    0x1FFFC7, 1.92},
		{ 67, "Ho", "Holmium",       0x00FF9C, 1.92},
		{ 68, "Er", "Erbium",        0x00E675, 1.89},
		{ 69, "Tm", "Thulium",       0x00D452, 1.90},
		{ 70, "Yb", "Ytterbium",     0x00BF38, 1.87},
		{ 71, "Lu", "Lutetium",      0x00AB24, 1.87},
		{ 72, "Hf", "Hafnium",       0x4DC2FF, 1.75},
		{ 73, "Ta", "Tantalum",      0x4DA6FF, 1.70},
		{ 74, "W",  "Tungsten",      0x2194D6, 1.62},
		{ 75, "Re", "Rhenium",       0x267DAB, 1.51},
		{ 76, "Os", "Osmium",        0x266696, 1.44},
		{ 77, "Ir", "Iridium",       0x175487, 1.41},
		{ 78, "Pt", "Platinum",      0xD0D0E0, 1.36},
		{ 79, "Au", "Gold",          0xFFD123, 1.36},
		{ 80, "Hg", "Mercury",       0xB8B8D0, 1.32},
		{ 81, "Tl", "Thallium",      0xA6544D, 1.45},
		{ 82, "Pb", "Lead",          0x575961, 1.46},
		{ 83, "Bi", "Bismuth",       0x9E4FB5, 1.48},
		{ 84, "Po", "Polonium",      0xAB5C00, 1.40},
		{ 85, "At", "Astatine",      0x754F45, 1.50},
		{ 86, "Rn", "Radon",         0x428296, 1.50},
		{ 87, "Fr", "Francium",      0x420066, 2.60},
		{ 88, "Ra", "Radium",        0x007D00, 2.21},
		{ 89, "Ac", "Actinium",      0x70ABFA, 2.15},
		{ 90, "Th", "Thorium",       0x00BAFF, 2.06},
		{ 91, "Pa", "Protactinium",  0x00A1FF, 2.00},
		{ 92, "U",  "Uranium",       0x008FFF, 1.96},
		{ 93, "Np", "Neptunium",     0x0080FF, 1.90},
		{ 94, "Pu", "Plutonium",     0x006BFF, 1.87},
		{ 95, "Am", "Americium",     0x545CF2, 1.80},
		{ 96, "Cm", "Curium",        0x785CE3, 1.69},
		{ 97, "Bk", "Berkelium",     0x8A4FE3, 1.68},
		{ 98, "Cf", "Californium",   0xA136D4, 1.68},
		{ 99, "Es", "Einsteinium",   0xB31FD4, 1.65},
		{100, "Fm", "Fermium",       0xB31FBA, 1.67},
		{101, "Md", "Mendelevium",   0xB30DA6, 1.73},
		{102, "No", "Nobelium",      0xBD0D87, 1.76},
	};
	if (Z < 1 || Z > 102) return TABLE[0];
	return TABLE[Z - 1];
}
// clang-format on

inline std::vector<DemoMolecule> build_element_library() {
	std::vector<DemoMolecule> lib;
	lib.reserve(102);
	for (int Z = 1; Z <= 102; ++Z) {
		const ElemInfo& e = elem_info(Z);
		DemoMolecule m;
		m.name    = std::string(e.symbol) + " - " + e.name;
		m.formula = e.symbol;
		// Single atom at origin; display radius scaled from covalent radius
		double disp = std::max(0.5, std::min(2.5, e.r_cov));
		m.atoms.push_back({ 0.0, 0.0, 0.0, e.symbol, e.cpk_rgb });
		// No bonds for single atoms
		lib.push_back(std::move(m));
		(void)disp;
	}
	return lib;
}

// ============================================================================
// Math helpers
// ============================================================================

struct Vec3 { double x, y, z; };

inline Vec3 rotate_y(Vec3 v, double theta) {
	double c = std::cos(theta), s = std::sin(theta);
	return { c*v.x + s*v.z, v.y, -s*v.x + c*v.z };
}

inline Vec3 rotate_x(Vec3 v, double theta) {
	double c = std::cos(theta), s = std::sin(theta);
	return { v.x, c*v.y - s*v.z, s*v.y + c*v.z };
}

// ============================================================================
// ASCII renderer
// ============================================================================

static constexpr int COLS = 80;
static constexpr int ROWS = 30;

struct Pixel {
	char        ch  = ' ';
	double      z   = -1e9;
	uint32_t    rgb = 0xAAAAAA;
};

// CPK-ish radius scale for visual size (screen units)
inline double atom_screen_radius(const std::string& sym) {
	if (sym == "H")  return 1.2;
	if (sym == "C")  return 1.7;
	if (sym == "N")  return 1.6;
	if (sym == "O")  return 1.5;
	if (sym == "F")  return 1.4;
	if (sym == "S")  return 1.9;
	if (sym == "P")  return 1.9;
	if (sym == "Cl") return 1.8;
	if (sym == "Xe") return 2.2;
	if (sym == "Xe") return 2.2;
	if (sym == "B")  return 1.7;
	if (sym == "Xe") return 2.2;
	return 1.8;
}

inline void emit_ansi_rgb(uint32_t rgb) {
	int r = (rgb >> 16) & 0xFF;
	int g = (rgb >>  8) & 0xFF;
	int b =  rgb        & 0xFF;
	std::printf("\x1b[38;2;%d;%d;%dm", r, g, b);
}

// shadow_type values mirror VisualSection::shadow_type:
//   0 = off (flat)  1 = ambient_soft  2 = depth_fade  3 = contact
// live_mode mirrors VisualSection::live_switch: when true, the window is
// never cleared; each call overwrites the previous frame in-place.
inline void render_frame(const DemoMolecule& mol, double theta_y, double theta_x,
						  bool first_frame,
						  int display_idx = -1, int total = -1,
						  int shadow_type = 0, bool live_mode = false)
{
	// Scale: 1 Angstrom -> ~8 cols / ~5 rows
	const double sx = 8.0, sy = 5.0;
	const double cx = COLS / 2.0, cy = ROWS / 2.0;

	std::vector<Pixel> buf(COLS * ROWS);

	auto set_pixel = [&](int col, int row, char ch, double z, uint32_t rgb) {
		if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return;
		auto& p = buf[row * COLS + col];
		if (z > p.z) { p.ch = ch; p.z = z; p.rgb = rgb; }
	};

	// Draw bonds (lines between projected atoms)
	for (auto& bond : mol.bonds) {
		const auto& a0 = mol.atoms[bond.a];
		const auto& a1 = mol.atoms[bond.b];
		Vec3 p0 = rotate_x(rotate_y({a0.x, a0.y, a0.z}, theta_y), theta_x);
		Vec3 p1 = rotate_x(rotate_y({a1.x, a1.y, a1.z}, theta_y), theta_x);
		// Bresenham
		int x0 = (int)(cx + p0.x * sx), y0 = (int)(cy - p0.y * sy);
		int x1 = (int)(cx + p1.x * sx), y1 = (int)(cy - p1.y * sy);
		int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
		int sx2 = x0 < x1 ? 1 : -1, sy2 = y0 < y1 ? 1 : -1;
		int err = dx - dy;
		int steps = std::max(dx, dy);
		for (int k = 0; k <= steps; ++k) {
			double t = steps > 0 ? (double)k / steps : 0.0;
			double z = p0.z * (1.0 - t) + p1.z * t;
			// slightly behind atoms
			set_pixel(x0, y0, '-', z - 0.5, 0x444444);
			int e2 = 2 * err;
			if (e2 > -dy) { err -= dy; x0 += sx2; }
			if (e2 <  dx) { err += dx; y0 += sy2; }
		}
	}

	// Draw atoms (simple filled circle approximation)
	for (auto& atom : mol.atoms) {
		Vec3 rp = rotate_x(rotate_y({atom.x, atom.y, atom.z}, theta_y), theta_x);
		int px = (int)(cx + rp.x * sx);
		int py = (int)(cy - rp.y * sy);
		double vr = atom_screen_radius(atom.symbol);
		// single cell — label char
		char lbl = atom.symbol[0];
		// shade: per VisualSection::shadow_type
		// 0 = flat, 1 = ambient_soft, 2 = depth_fade, 3 = contact
		double shade;
		if (shadow_type == 1) {
			// ambient_soft: gentle gradient, good for small molecules
			shade = 0.30 + 0.70 * std::max(0.0, std::min(1.0, (rp.z + 3.0) / 6.0));
		} else if (shadow_type == 2) {
			// depth_fade: strong perspective cue, good for crystals
			shade = 1.0  - 0.55 * std::max(0.0, std::min(1.0, (3.0 - rp.z) / 6.0));
		} else if (shadow_type == 3) {
			// contact: proximity darkening (simple z-spread heuristic)
			double spread = std::abs(rp.z) / 3.0;
			shade = std::max(0.15, 1.0 - spread * 0.4);
		} else {
			// 0 = off: legacy flat shading
			shade = 0.4 + 0.6 * std::max(0.0, std::min(1.0, (rp.z + 3.0) / 6.0));
		}
		auto shade_rgb = [shade](uint32_t rgb) -> uint32_t {
			int r = (int)(((rgb >> 16) & 0xFF) * shade);
			int g = (int)(((rgb >>  8) & 0xFF) * shade);
			int b = (int)(( rgb        & 0xFF) * shade);
			return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
		};
		// fill a small disc
		int ir = std::max(1, (int)(vr * 0.8));
		for (int dy2 = -ir; dy2 <= ir; ++dy2) {
			for (int dx2 = -ir; dx2 <= ir; ++dx2) {
				if (dx2*dx2 + dy2*dy2*2 <= ir*ir*2) {
					char ch2 = (dx2 == 0 && dy2 == 0) ? lbl : '.';
					set_pixel(px + dx2, py + dy2, ch2, rp.z, shade_rgb(atom.color_rgb));
				}
			}
		}
	}

	// Output: live-switch feed — refresh in-place or full clear on first frame.
	// live_mode = true  : always cursor-up overwrite (never tear down the window)
	// live_mode = false : first_frame triggers a full clear; subsequent frames
	//                     still overwrite in-place (legacy element-carousel behaviour)
	bool do_clear = !live_mode && first_frame;
	if (do_clear)
		std::printf("\x1b[2J\x1b[H");  // full clear only on true first frame
	else if (!first_frame)
		std::printf("\x1b[%dA", ROWS + 2); // move up and overwrite
	// Title bar
	if (display_idx > 0 && total > 0)
		std::printf("\x1b[38;2;100;200;255m  VSEPR-SIM  --demo%-2d/%d  |  %s (%s)  |  Ctrl+C to exit\x1b[0m\n",
				display_idx, total, mol.name.c_str(), mol.formula.c_str());
	else
		std::printf("\x1b[38;2;100;200;255m  VSEPR-SIM  --demo  |  %s (%s)  |  Ctrl+C to exit\x1b[0m\n",
				mol.name.c_str(), mol.formula.c_str());
	std::printf("\x1b[38;2;50;50;80m%s\x1b[0m\n", std::string(COLS, '-').c_str());
	for (int row = 0; row < ROWS; ++row) {
		for (int col = 0; col < COLS; ++col) {
			const Pixel& p = buf[row * COLS + col];
			if (p.ch != ' ') {
				emit_ansi_rgb(p.rgb);
				std::putchar(p.ch);
				std::printf("\x1b[0m");
			} else {
				std::putchar(' ');
			}
		}
		std::putchar('\n');
	}
	std::fflush(stdout);
}

// ============================================================================
// Signal handling
// ============================================================================

static volatile sig_atomic_t g_demo_stop = 0;
inline void demo_sig_handler(int) { g_demo_stop = 1; }

// ============================================================================
// Entry point
// ============================================================================

// forced_index: -1 = use seed/molecule-name selection, 0-based index otherwise
inline int run_demo(int argc, char** argv, int forced_index = -1) {
	// -- Parse flags --
	int fps     = 10;
	int frames  = 0;        // 0 = infinite
	long seed   = (long)std::chrono::system_clock::now().time_since_epoch().count();
	std::string force_mol;
	int  shadow_type = 0;   // 0=off 1=ambient_soft 2=depth_fade 3=contact
	bool live_mode   = false; // live-switch: refresh in-place

	for (int i = 0; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--fps"         && i+1 < argc) { fps        = std::atoi(argv[++i]); }
		if (a == "--frames"      && i+1 < argc) { frames     = std::atoi(argv[++i]); }
		if (a == "--seed"        && i+1 < argc) { seed       = std::atol(argv[++i]); }
		if (a == "--molecule"    && i+1 < argc) { force_mol  = argv[++i]; }
		if (a == "--shadow"      && i+1 < argc) { shadow_type = std::atoi(argv[++i]); }
		if (a == "--live-switch")               { live_mode  = true; }
		if (a == "--list") {
			auto lib = build_molecule_library();
			std::printf("VSEPR-SIM demo molecules:\n");
			for (size_t k = 0; k < lib.size(); ++k)
				std::printf("  --demo%-3zu  %s\n", k+1, lib[k].name.c_str());
			return 0;
		}
	}
	fps         = std::max(1, std::min(60, fps));
	shadow_type = std::max(0, std::min(3, shadow_type));

	// -- Pick molecule --
	auto library = build_molecule_library();
	const DemoMolecule* mol = nullptr;
	if (forced_index >= 0) {
		if (forced_index >= (int)library.size()) {
			std::fprintf(stderr, "Demo index %d out of range (1..%zu available).\n",
						forced_index + 1, library.size());
			return 2;
		}
		mol = &library[(size_t)forced_index];
	} else if (!force_mol.empty()) {
		for (auto& m : library)
			if (m.name == force_mol || m.formula == force_mol) { mol = &m; break; }
		if (!mol) {
			std::fprintf(stderr, "Unknown molecule '%s'. Available: ", force_mol.c_str());
			for (auto& m : library) std::fprintf(stderr, "%s ", m.name.c_str());
			std::fprintf(stderr, "\n");
			return 2;
		}
	} else {
		mol = &library[(size_t)seed % library.size()];
	}
	// Compute 1-based display index
	int display_idx = -1;
	for (size_t k = 0; k < library.size(); ++k)
		if (&library[k] == mol) { display_idx = (int)k + 1; break; }

	// -- Setup --
	std::signal(SIGINT,  demo_sig_handler);
	std::signal(SIGTERM, demo_sig_handler);

	// Hide cursor, clear screen
	std::printf("\x1b[?25l\x1b[2J\x1b[H");
	std::fflush(stdout);

	const double deg_per_frame = 3.0;   // Y-axis rotation per frame
	const double tilt_x        = 0.35;  // fixed slight X tilt so depth is visible
	const auto   frame_dur = std::chrono::milliseconds(1000 / fps);

	double theta_y = 0.0;
	int    frame_n = 0;
	bool   first   = true;

	while (!g_demo_stop && (frames == 0 || frame_n < frames)) {
		auto t0 = std::chrono::steady_clock::now();
		render_frame(*mol, theta_y, tilt_x, first, display_idx, (int)library.size(), shadow_type, live_mode);
		first   = false;
		theta_y += deg_per_frame * M_PI / 180.0;
		++frame_n;
		auto elapsed = std::chrono::steady_clock::now() - t0;
		if (elapsed < frame_dur)
			std::this_thread::sleep_for(frame_dur - elapsed);
	}

	// Restore cursor, move below rendered area
	std::printf("\x1b[%dB\x1b[?25h\n", ROWS + 2);
	std::fflush(stdout);
	std::printf("  Demo ended.  Run 'vsepr --demo --molecule <name>' to pick a specific molecule.\n");
	return 0;
}

// ============================================================================
// Element tour  (--demo 0)
// Cycles through Z=1..102 with semi-random dwell times (0.5 – 3.5 s each).
// ============================================================================

inline int run_demo_element_tour(int argc, char** argv) {
	// -- Parse extra flags --
	int   fps        = 12;
	long  seed       = (long)std::chrono::system_clock::now().time_since_epoch().count();
	int   start_z    = 1;    // start element (1-based Z)
	int   shadow_type = 0;   // 0=off 1=ambient_soft 2=depth_fade 3=contact
	bool  live_mode  = false; // live-switch feed: refresh in-place across element switches

	for (int i = 0; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--fps"         && i+1 < argc) fps         = std::atoi(argv[++i]);
		if (a == "--seed"        && i+1 < argc) seed        = std::atol(argv[++i]);
		if (a == "--start"       && i+1 < argc) start_z     = std::atoi(argv[++i]);
		if (a == "--shadow"      && i+1 < argc) shadow_type = std::atoi(argv[++i]);
		if (a == "--live-switch")               live_mode   = true;
		if (a == "--list") {
			std::printf("VSEPR-SIM element tour (Z=1..102):\n");
			for (int Z = 1; Z <= 102; ++Z) {
				const ElemInfo& e = elem_info(Z);
				std::printf("  Z=%-3d  %-3s  %s\n", e.Z, e.symbol, e.name);
			}
			return 0;
		}
	}
	fps         = std::max(1, std::min(60, fps));
	start_z     = std::max(1, std::min(102, start_z));
	shadow_type = std::max(0, std::min(3, shadow_type));

	// Simple LCG for dwell randomisation
	auto lcg = [](uint64_t& s) -> double {
		s = s * 6364136223846793005ULL + 1442695040888963407ULL;
		return (double)(s >> 33) / (double)(1ULL << 31); // [0, 1)
	};
	uint64_t rng = (uint64_t)(unsigned long)seed ^ 0xDEADBEEF12345678ULL;

	// -- Setup --
	std::signal(SIGINT,  demo_sig_handler);
	std::signal(SIGTERM, demo_sig_handler);

	// live_mode: skip the destructive 2J clear so the window is never torn down
	if (!live_mode)
		std::printf("\x1b[?25l\x1b[2J\x1b[H");
	else
		std::printf("\x1b[?25l");   // hide cursor; first render_frame handles positioning
	std::fflush(stdout);

	const auto frame_dur = std::chrono::milliseconds(1000 / fps);
	const double deg_per_frame = 2.5;
	const double tilt_x        = 0.40;

	auto library = build_element_library();

	bool first = true;  // first frame ever; render_frame uses this to set cursor home

	for (int Z = start_z; Z <= 102 && !g_demo_stop; ++Z) {
		// Semi-random dwell: 0.5 .. 3.5 s
		double dwell_s  = 0.5 + lcg(rng) * 3.0;
		int    n_frames = std::max(1, (int)(dwell_s * fps));

		const DemoMolecule& mol = library[(size_t)(Z - 1)];

		// Title overlay shows Z, symbol, name and progress bar
		// We build a fake "display_idx" trick: reuse render_frame's idx/total path
		double theta_y = 0.0;

		for (int f = 0; f < n_frames && !g_demo_stop; ++f) {
			auto t0 = std::chrono::steady_clock::now();

			// render_frame uses display_idx > 0 to show "Z/102" in title
			// We pass Z as display_idx and 102 as total; mol.name already has symbol+name
			render_frame(mol, theta_y, tilt_x, first, Z, 102, shadow_type, live_mode);
			first    = false;
			theta_y += deg_per_frame * M_PI / 180.0;

			auto elapsed = std::chrono::steady_clock::now() - t0;
			if (elapsed < frame_dur)
				std::this_thread::sleep_for(frame_dur - elapsed);
		}
	}

	std::printf("\x1b[%dB\x1b[?25h\n", ROWS + 2);
	std::fflush(stdout);
	std::printf("  Element tour complete (Z=1..102).  "
				"Run 'vsepr --demo0 --start <Z>' to begin from a specific element.\n");
	return 0;
}

} // namespace demo
} // namespace vsim
