#pragma once
/**
 * xyz_unified.hpp -- Canonical XYZ family data model
 * ====================================================
 * VSEPR-SIM  |  WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * Implements the unified data model described in docs/xyz_file_formats.tex
 * (Edition 2).  This header is the single ground-truth type layer for all
 * four serialisation formats.  Existing legacy headers (include/io/xyz_format.hpp,
 * include/thermal/xyzc_format.hpp) are NOT modified; this layer sits above them.
 *
 * Format hierarchy (from spec §1):
 *   .xyz   ⊂  .xyza  ⊂  .xyzc    (progressive extension, backward compatible)
 *   .xyzf  =  multi-frame wrapper  (independently parseable per frame)
 *
 * Unit system (non-negotiable, spec §1.1):
 *   coords      Å (angstrom)
 *   energy      kcal/mol
 *   velocity    Å/fs
 *   force       kcal/(mol·Å)
 *   charge      elementary e
 *   time        fs
 *   temperature K
 */

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <algorithm>
#include "core/math_vec3.hpp"

namespace vsepr {
namespace io {

// ============================================================================
// Vec3 alias (Day #56 — unified via vsepr::Vec3)
// ============================================================================
// XYZVec3 kept as an alias so existing call sites in xyz_reader.hpp and
// elsewhere compile without modification.
using XYZVec3 = vsepr::Vec3;

// ============================================================================
// AtomRecord — per-atom data for one frame
// ============================================================================
//
// Always present:  Z, x, y, z
// Optional (.xyza+): q, v, f, e  (present iff the column was declared)
//
// Z is the canonical identity (atomic number).  The element symbol is kept
// alongside for round-trip write fidelity.

struct AtomRecord {
	int         Z       = 0;       // Atomic number (identity)
	std::string symbol;            // Element symbol (for writers)
	double      x{}, y{}, z{};    // Cartesian coordinates (Å)

	// Optional extended fields (spec §3 / .xyza)
	std::optional<double>  q;      // Partial charge (e)
	std::optional<XYZVec3> v;      // Velocity (Å/fs)
	std::optional<XYZVec3> f;      // Force (kcal/(mol·Å))
	std::optional<double>  e;      // Per-atom energy (kcal/mol)

	AtomRecord() = default;
	AtomRecord(int Z_, const std::string& sym, double x_, double y_, double z_)
		: Z(Z_), symbol(sym), x(x_), y(y_), z(z_) {}
};

// ============================================================================
// Box / PBC descriptor
// ============================================================================

struct XYZBox {
	double ax{}, ay{}, az{};       // Orthorhombic diagonal (Å)
	bool   pbc[3] = {true, true, true};

	// Full 3×3 lattice (row-major; diagonal = orthorhombic)
	// Lattice="ax 0 0  0 ay 0  0 0 az" from spec §5
	std::array<double, 9> lattice = {0,0,0, 0,0,0, 0,0,0};

	double volume() const { return ax * ay * az; }
};

// ============================================================================
// XYZFrame — one complete frame (any format)
// ============================================================================

struct XYZFrame {
	int                   N       = 0;
	std::string           comment;
	std::vector<AtomRecord> atoms;

	// Derived from comment-line parsing
	std::optional<double>  energy;       // kcal/mol  (from "E = <val>")
	std::optional<double>  temperature;  // K         (from "T = <val>")
	std::optional<XYZBox>  box;          // bounding box / PBC

	// Per-frame property columns present (populated by reader)
	bool has_charge   = false;
	bool has_velocity = false;
	bool has_force    = false;
	bool has_energy_col = false;

	// Frame index within .xyzf file (0-based)
	int frame_index = 0;
};

// ============================================================================
// CheckpointState — .xyzc header block (spec §4)
// ============================================================================

struct CheckpointState {
	int    step     = 0;
	double time     = 0.0;     // fs
	double dt       = 0.0;     // fs
	double T_target = 0.0;     // K  (0 = NVE)
	int    seed     = 0;
	std::optional<XYZBox> box;
};

// ============================================================================
// XYZData — unified container
// ============================================================================

struct XYZData {
	std::vector<XYZFrame>          frames;
	std::optional<CheckpointState> checkpoint;  // present for .xyzc only

	bool empty() const { return frames.empty(); }
	int  frame_count() const { return static_cast<int>(frames.size()); }
	const XYZFrame& first() const { return frames.front(); }
};

// ============================================================================
// Format detection (spec §3, step 1)
// ============================================================================

enum class XYZFormat {
	XYZ,    // static geometry
	XYZA,   // + charge/velocity/force/energy columns
	XYZC,   // + checkpoint header (single frame)
	XYZF,   // multi-frame trajectory
	XYZW,   // wind particle field (VSEPR extension)
	UNKNOWN
};

// Detect from file extension only — caller is responsible for content check
inline XYZFormat detect_format_by_extension(const std::string& path) {
	auto ext_start = path.rfind('.');
	if (ext_start == std::string::npos) return XYZFormat::UNKNOWN;
	std::string ext = path.substr(ext_start);
	// normalise to lower-case
	for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (ext == ".xyz")  return XYZFormat::XYZ;
	if (ext == ".xyza") return XYZFormat::XYZA;
	if (ext == ".xyzc") return XYZFormat::XYZC;
	if (ext == ".xyzf") return XYZFormat::XYZF;
	if (ext == ".xyzw") return XYZFormat::XYZW;
	return XYZFormat::UNKNOWN;
}

inline const char* format_name(XYZFormat f) {
	switch (f) {
		case XYZFormat::XYZ:  return ".xyz";
		case XYZFormat::XYZA: return ".xyza";
		case XYZFormat::XYZC: return ".xyzc";
		case XYZFormat::XYZF: return ".xyzf";
		case XYZFormat::XYZW: return ".xyzw";
		default:              return "unknown";
	}
}

// ============================================================================
// Parse warning (soft errors — spec §9)
// ============================================================================

enum class XYZWarnCode {
	COLUMN_ZERO_FILL,     // declared property but insufficient columns — zeroed
	UNKNOWN_PROPERTY,     // unrecognised key in properties= list
	ENERGY_PARSE_FAIL,    // comment "E = ..." could not be parsed
	TEMP_PARSE_FAIL,      // comment "T = ..." could not be parsed
	SANITY_FORCE,         // |F| > 1000 kcal/(mol·Å)
	SANITY_CHARGE,        // |q| > 5 e
	SANITY_VELOCITY,      // |v| > 1.0 Å/fs (10× typical 300K value)
	SANITY_COORD,         // coordinate > 1e6 Å
};

struct XYZParseWarning {
	XYZWarnCode code;
	int         frame_index = -1;
	int         atom_index  = -1;
	std::string message;
};

// ============================================================================
// Observable-range sanity validator (spec §8)
// ============================================================================
//
// Returns a list of warnings.  Empty = all clear.
// Thresholds from Table 6 of xyz_file_formats.tex.

inline std::vector<XYZParseWarning> validate_frame(const XYZFrame& frame, int fi = 0) {
	std::vector<XYZParseWarning> warns;

	for (int i = 0; i < static_cast<int>(frame.atoms.size()); ++i) {
		const auto& a = frame.atoms[i];

		// Coordinate sanity
		if (std::abs(a.x) > 1e6 || std::abs(a.y) > 1e6 || std::abs(a.z) > 1e6) {
			warns.push_back({XYZWarnCode::SANITY_COORD, fi, i,
				"coord > 1e6 Å on atom " + std::to_string(i)});
		}

		// Charge sanity
		if (a.q && std::abs(*a.q) > 5.0) {
			warns.push_back({XYZWarnCode::SANITY_CHARGE, fi, i,
				"charge " + std::to_string(*a.q) + " e exceeds |5| threshold"});
		}

		// Force sanity
		if (a.f) {
			double fmag = std::sqrt(a.f->x*a.f->x + a.f->y*a.f->y + a.f->z*a.f->z);
			if (fmag > 1000.0) {
				warns.push_back({XYZWarnCode::SANITY_FORCE, fi, i,
					"force " + std::to_string(fmag) + " kcal/(mol·Å) exceeds 1000 threshold"});
			}
		}

		// Velocity sanity
		if (a.v) {
			double vmag = std::sqrt(a.v->x*a.v->x + a.v->y*a.v->y + a.v->z*a.v->z);
			if (vmag > 1.0) {
				warns.push_back({XYZWarnCode::SANITY_VELOCITY, fi, i,
					"velocity " + std::to_string(vmag) + " Å/fs exceeds 1.0 threshold"});
			}
		}
	}
	return warns;
}

inline std::vector<XYZParseWarning> validate_data(const XYZData& data) {
	std::vector<XYZParseWarning> all;
	for (int i = 0; i < static_cast<int>(data.frames.size()); ++i) {
		auto w = validate_frame(data.frames[i], i);
		all.insert(all.end(), w.begin(), w.end());
	}
	return all;
}

// ============================================================================
// Comment-line helpers (spec §2.2)
// ============================================================================
//
// Format: "<name> | E = <val> kcal/mol | T = <val> K | <notes>"

inline std::string build_comment(const std::string& name,
								  std::optional<double> energy     = std::nullopt,
								  std::optional<double> temperature = std::nullopt,
								  const std::string& notes          = "")
{
	std::ostringstream out;
	out << name;
	if (energy)      out << " | E = " << *energy << " kcal/mol";
	if (temperature) out << " | T = " << *temperature << " K";
	if (!notes.empty()) out << " | " << notes;
	return out.str();
}

inline std::optional<double> parse_comment_energy(const std::string& comment) {
	auto pos = comment.find("E = ");
	if (pos == std::string::npos) return std::nullopt;
	try {
		size_t end;
		double v = std::stod(comment.substr(pos + 4), &end);
		return v;
	} catch (...) { return std::nullopt; }
}

inline std::optional<double> parse_comment_temperature(const std::string& comment) {
	auto pos = comment.find("T = ");
	if (pos == std::string::npos) return std::nullopt;
	try {
		size_t end;
		double v = std::stod(comment.substr(pos + 4), &end);
		return v;
	} catch (...) { return std::nullopt; }
}

// ============================================================================
// Element symbol → atomic number (essential subset)
// ============================================================================

inline int symbol_to_Z(const std::string& sym) {
	// Ordered by frequency in molecular simulations
	static const std::pair<const char*, int> table[] = {
		{"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},
		{"N",7},{"O",8},{"F",9},{"Ne",10},{"Na",11},{"Mg",12},
		{"Al",13},{"Si",14},{"P",15},{"S",16},{"Cl",17},{"Ar",18},
		{"K",19},{"Ca",20},{"Sc",21},{"Ti",22},{"V",23},{"Cr",24},
		{"Mn",25},{"Fe",26},{"Co",27},{"Ni",28},{"Cu",29},{"Zn",30},
		{"Ga",31},{"Ge",32},{"As",33},{"Se",34},{"Br",35},{"Kr",36},
		{"Rb",37},{"Sr",38},{"Y",39},{"Zr",40},{"Nb",41},{"Mo",42},
		{"Tc",43},{"Ru",44},{"Rh",45},{"Pd",46},{"Ag",47},{"Cd",48},
		{"In",49},{"Sn",50},{"Sb",51},{"Te",52},{"I",53},{"Xe",54},
		{"Cs",55},{"Ba",56},{"La",57},{"Ce",58},{"Pr",59},{"Nd",60},
		{"Pm",61},{"Sm",62},{"Eu",63},{"Gd",64},{"Tb",65},{"Dy",66},
		{"Ho",67},{"Er",68},{"Tm",69},{"Yb",70},{"Lu",71},
		{"Hf",72},{"Ta",73},{"W",74},{"Re",75},{"Os",76},{"Ir",77},
		{"Pt",78},{"Au",79},{"Hg",80},{"Tl",81},{"Pb",82},{"Bi",83},
		{"Po",84},{"At",85},{"Rn",86},
		{"Ra",88},{"Ac",89},{"Th",90},{"Pa",91},{"U",92},
		{"Np",93},{"Pu",94},{"Am",95},{"Cm",96},{"Bk",97},{"Cf",98},
		{"Es",99},{"Fm",100},{"Md",101},{"No",102},{"Lr",103},
	};
	for (auto& [s, z] : table) if (sym == s) return z;
	return 0;  // unknown
}

inline std::string Z_to_symbol(int Z) {
	static const char* table[] = {
		"?",
		"H","He","Li","Be","B","C","N","O","F","Ne",
		"Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca",
		"Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
		"Ga","Ge","As","Se","Br","Kr","Rb","Sr","Y","Zr",
		"Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn",
		"Sb","Te","I","Xe","Cs","Ba","La","Ce","Pr","Nd",
		"Pm","Sm","Eu","Gd","Tb","Dy","Ho","Er","Tm","Yb",
		"Lu","Hf","Ta","W","Re","Os","Ir","Pt","Au","Hg",
		"Tl","Pb","Bi","Po","At","Rn","Fr","Ra","Ac","Th",
		"Pa","U","Np","Pu","Am","Cm","Bk","Cf","Es","Fm",
		"Md","No","Lr"
	};
	if (Z >= 1 && Z <= 103) return table[Z];
	return "X" + std::to_string(Z);
}

} // namespace io
} // namespace vsepr
