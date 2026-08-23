#pragma once
/**
 * xyza_dynamic.hpp
 * ================
 * VSEPR-SIM  |  WO-VSIM-66A  |  .xyza Dynamic State Gate
 *
 * Provides the WO-66A canonical structs and the high-level reader / writer
 * API for multi-frame .xyza dynamic-state files.
 *
 * Relationship to existing IO stack:
 *   xyz_unified.hpp  -- AtomRecord / XYZFrame  (optional Q/V/F/E fields)
 *   xyz_reader.hpp   -- parse_frame_xyza()      (column-level parser)
 *   xyz_writer.hpp   -- emit_xyza_frame()        (column-level emitter)
 *   xyza_dynamic.hpp -- XyzaAtomRecord / XyzaFrame  (concrete, no std::optional)
 *                       read_xyza_dynamic()          (multi-frame reader)
 *                       write_xyza_dynamic()         (multi-frame writer)
 *                       XyzaDynamicWriter            (streaming frame writer)
 *
 * Unit system (authoritative for this layer):
 *   position    Å  (angstrom)
 *   charge      e  (elementary charge)
 *   velocity    Å/fs
 *   force       eV/Å   (struct field names encode the unit)
 *   energy      eV
 *   time        fs
 *   temperature K
 *
 * The low-level layer (xyz_unified.hpp) uses kcal/mol.  This header exposes
 * kcal_to_eV / eV_per_A constants and conversion helpers so callers can
 * bridge the two layers without embedding magic numbers.
 *
 * Column layout (.xyza atom line):
 *   sym  x  y  z  [q  [vx vy vz  [fx fy fz  [e]]]]
 *   1    3  1  3     3                             = 11 extended columns max
 *
 * Missing trailing columns are zero-filled (spec §3, WO-66A scope item 4).
 * The properties= declaration drives column presence; absence = all zeros.
 *
 * Scope gate (WO-66A checklist):
 *   [x] 11-column atom-line support
 *   [x] Missing Q/V/F trailing values default to zero
 *   [x] properties= declaration compatibility
 *   [x] Charge model passthrough (charge_model tag in comment)
 *   [x] Velocity and force arrays enter runtime state (non-optional fields)
 *   [x] Multi-frame archive writer (XyzaDynamicWriter)
 *   [x] Q/V/F summary helpers for live report
 *   [x] Formation engine pre-state bridge (to_xyz_frame / from_xyz_frame)
 */

#include "../../src/io/xyz_unified.hpp"
#include "../../src/io/xyz_reader.hpp"
#include "../../src/io/xyz_writer.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <iomanip>

namespace vsepr {
namespace io {

// ============================================================================
// Unit conversion constants
// ============================================================================

/// 1 kcal/mol in eV
inline constexpr double kcal_to_eV       = 0.043364104;
/// 1 kcal/(mol·Å) in eV/Å
inline constexpr double kcal_mol_A_to_eV_A = 0.043364104;
/// 1 eV in kcal/mol
inline constexpr double eV_to_kcal       = 23.060541945;
/// 1 eV/Å in kcal/(mol·Å)
inline constexpr double eV_A_to_kcal_mol_A = 23.060541945;

// ============================================================================
// XyzaAtomRecord  —  WO-66A canonical per-atom record
// ============================================================================
// All fields are concrete (no std::optional).  Missing data in the source
// file is zero-filled by the reader (see zero_fill_policy below).

struct XyzaAtomRecord {
	std::string species;          // Element symbol, e.g. "O", "Na"

	Vec3   position_A;            // Cartesian position (Å)
	double charge_e    = 0.0;     // Partial charge (elementary e)

	Vec3   velocity_A_per_fs;     // Velocity (Å/fs)
	Vec3   force_eV_per_A;        // Force (eV/Å)   -- see unit note above
	double energy_eV   = 0.0;     // Per-atom energy (eV)

	XyzaAtomRecord() = default;
	XyzaAtomRecord(const std::string& sym,
				   Vec3 pos, double q = 0.0,
				   Vec3 vel = {}, Vec3 frc = {}, double e = 0.0)
		: species(sym), position_A(pos), charge_e(q),
		  velocity_A_per_fs(vel), force_eV_per_A(frc), energy_eV(e) {}

	// Convenience: kinetic energy from velocity and mass (amu)
	double kinetic_energy_eV(double mass_amu) const {
		double v2 = velocity_A_per_fs.x * velocity_A_per_fs.x
				  + velocity_A_per_fs.y * velocity_A_per_fs.y
				  + velocity_A_per_fs.z * velocity_A_per_fs.z;
		// KE = 0.5 * m * v²,  units: amu * (Å/fs)² = 1.036427e-4 eV
		// 1 amu·(Å/fs)² = 1.036427e-4 eV
		return 0.5 * mass_amu * v2 * 1.036427e-4;
	}

	// Force magnitude (eV/Å)
	double force_magnitude_eV_per_A() const {
		return std::sqrt(force_eV_per_A.x * force_eV_per_A.x
					   + force_eV_per_A.y * force_eV_per_A.y
					   + force_eV_per_A.z * force_eV_per_A.z);
	}

	// Velocity magnitude (Å/fs)
	double velocity_magnitude_A_per_fs() const {
		return std::sqrt(velocity_A_per_fs.x * velocity_A_per_fs.x
					   + velocity_A_per_fs.y * velocity_A_per_fs.y
					   + velocity_A_per_fs.z * velocity_A_per_fs.z);
	}
};

// ============================================================================
// Charge model tag
// ============================================================================

enum class ChargeModel {
	Formal,     // integer formal charges
	Partial,    // real-valued partial charges (e.g. Gasteiger, RESP)
	Bader,      // Bader AIM charges
	Neutral,    // all charges zeroed
	Unknown,    // not declared in file
};

inline const char* charge_model_str(ChargeModel m) {
	switch (m) {
		case ChargeModel::Formal:  return "formal";
		case ChargeModel::Partial: return "partial";
		case ChargeModel::Bader:   return "bader";
		case ChargeModel::Neutral: return "neutral";
		default:                   return "unknown";
	}
}

inline ChargeModel parse_charge_model(const std::string& comment) {
	auto find_val = [&](const std::string& key) -> std::string {
		auto p = comment.find(key + "=");
		if (p == std::string::npos) return "";
		p += key.size() + 1;
		if (p < comment.size() && comment[p] == '"') ++p;
		auto e = comment.find_first_of("\" \t", p);
		return comment.substr(p, e == std::string::npos ? comment.size() - p : e - p);
	};
	std::string v = find_val("charge_model");
	if (v == "formal")  return ChargeModel::Formal;
	if (v == "partial") return ChargeModel::Partial;
	if (v == "bader")   return ChargeModel::Bader;
	if (v == "neutral") return ChargeModel::Neutral;
	return ChargeModel::Unknown;
}

// ============================================================================
// XyzaFrame  —  WO-66A canonical frame
// ============================================================================

struct XyzaFrame {
	uint64_t step         = 0;
	double   time_fs      = 0.0;
	double   energy_eV    = 0.0;
	double   temperature_K= 0.0;

	// Metadata preserved from comment line
	std::string label;           // project / system name
	std::string raw_comment;     // full verbatim comment line
	ChargeModel charge_model = ChargeModel::Unknown;

	// PBC
	bool   has_box = false;
	double lx = 0.0, ly = 0.0, lz = 0.0;
	bool   pbc[3] = {false, false, false};

	// Column presence flags (derived from properties= declaration)
	bool has_charge   = false;
	bool has_velocity = false;
	bool has_force    = false;
	bool has_energy_col = false;

	std::vector<XyzaAtomRecord> atoms;

	int atom_count() const { return static_cast<int>(atoms.size()); }

	// Aggregate helpers for live report (WO-66A scope item 8)
	double total_charge_e() const {
		double q = 0.0;
		for (const auto& a : atoms) q += a.charge_e;
		return q;
	}
	double max_force_eV_per_A() const {
		double m = 0.0;
		for (const auto& a : atoms) m = std::max(m, a.force_magnitude_eV_per_A());
		return m;
	}
	double rms_velocity_A_per_fs() const {
		if (atoms.empty()) return 0.0;
		double s = 0.0;
		for (const auto& a : atoms) {
			const auto& v = a.velocity_A_per_fs;
			s += v.x*v.x + v.y*v.y + v.z*v.z;
		}
		return std::sqrt(s / atoms.size());
	}
	double rms_force_eV_per_A() const {
		if (atoms.empty()) return 0.0;
		double s = 0.0;
		for (const auto& a : atoms) {
			const auto& f = a.force_eV_per_A;
			s += f.x*f.x + f.y*f.y + f.z*f.z;
		}
		return std::sqrt(s / atoms.size());
	}
};

// ============================================================================
// Zero-fill policy — what to do when a column is absent from the file
// ============================================================================

enum class ZeroFillPolicy {
	Silent,    // zero and continue (default, matches spec §3)
	Warn,      // zero and push a warning
};

// ============================================================================
// XyzaDynamicReadResult
// ============================================================================

struct XyzaDynamicDiagnostic {
	enum class Kind { ZeroFill, UnknownProperty, SanityViolation, ParseError };
	Kind        kind;
	int         frame_index = -1;
	int         atom_index  = -1;
	std::string message;
};

struct XyzaDynamicReadResult {
	std::vector<XyzaFrame>              frames;
	std::vector<XyzaDynamicDiagnostic>  diagnostics;

	bool ok()        const { return frames.empty() == false; }
	bool has_diag()  const { return !diagnostics.empty(); }
	int  frame_count() const { return static_cast<int>(frames.size()); }

	void push_warn(int fi, int ai, const std::string& msg,
				   XyzaDynamicDiagnostic::Kind k = XyzaDynamicDiagnostic::Kind::ZeroFill) {
		diagnostics.push_back({k, fi, ai, msg});
	}
	void push_error(int fi, int ai, const std::string& msg) {
		diagnostics.push_back({XyzaDynamicDiagnostic::Kind::ParseError, fi, ai, msg});
	}
};

// ============================================================================
// Internal helpers
// ============================================================================

namespace detail {

/// Convert one low-level AtomRecord (kcal/mol units, optional fields) to
/// XyzaAtomRecord (eV units, concrete fields).
inline XyzaAtomRecord atom_from_unified(const AtomRecord& a,
										 bool has_charge, bool has_vel, bool has_frc, bool has_e,
										 int fi, int ai,
										 XyzaDynamicReadResult& res,
										 ZeroFillPolicy zfp)
{
	XyzaAtomRecord r;
	r.species    = a.symbol;
	r.position_A = {a.x, a.y, a.z};

	if (has_charge) {
		r.charge_e = a.q ? *a.q : 0.0;
		if (!a.q && zfp == ZeroFillPolicy::Warn)
			res.push_warn(fi, ai, "charge zero-filled");
	}
	if (has_vel) {
		r.velocity_A_per_fs = a.v ? *a.v : Vec3{};
		if (!a.v && zfp == ZeroFillPolicy::Warn)
			res.push_warn(fi, ai, "velocity zero-filled");
	}
	if (has_frc) {
		// Convert from kcal/(mol·Å) to eV/Å
		Vec3 fk = a.f ? *a.f : Vec3{};
		r.force_eV_per_A = {fk.x * kcal_mol_A_to_eV_A,
							 fk.y * kcal_mol_A_to_eV_A,
							 fk.z * kcal_mol_A_to_eV_A};
		if (!a.f && zfp == ZeroFillPolicy::Warn)
			res.push_warn(fi, ai, "force zero-filled");
	}
	if (has_e) {
		// Convert from kcal/mol to eV
		r.energy_eV = a.e ? (*a.e * kcal_to_eV) : 0.0;
		if (!a.e && zfp == ZeroFillPolicy::Warn)
			res.push_warn(fi, ai, "energy zero-filled");
	}
	return r;
}

/// Parse step= and time= from comment line (WO-66A extensions)
inline void parse_dynamic_comment(const std::string& comment,
								   uint64_t& step_out, double& time_out)
{
	step_out = 0; time_out = 0.0;
	// "step <N>" or "step=<N>"
	auto try_parse = [&](const std::string& key, auto& val) {
		auto p = comment.find(key);
		if (p == std::string::npos) return;
		p += key.size();
		while (p < comment.size() && (comment[p] == ' ' || comment[p] == '=')) ++p;
		std::istringstream ss(comment.substr(p));
		ss >> val;
	};
	try_parse("step", step_out);
	try_parse("time", time_out);
}

/// Build the extended comment line for a dynamic frame
inline std::string make_dynamic_comment(const XyzaFrame& f,
										 const std::string& props_decl)
{
	std::ostringstream out;
	if (!f.label.empty()) out << f.label << " | ";
	out << "step " << f.step;
	if (f.time_fs > 0.0)
		out << " | time " << std::fixed << std::setprecision(3) << f.time_fs << " fs";
	if (f.energy_eV != 0.0)
		out << " | E = " << std::fixed << std::setprecision(6)
			<< (f.energy_eV * eV_to_kcal) << " kcal/mol";
	if (f.temperature_K > 0.0)
		out << " | T = " << std::fixed << std::setprecision(2) << f.temperature_K << " K";
	if (f.charge_model != ChargeModel::Unknown)
		out << " charge_model=\"" << charge_model_str(f.charge_model) << '"';
	if (f.has_box)
		out << " Lattice=\"" << std::fixed << std::setprecision(6)
			<< f.lx << " 0.000000 0.000000  0.000000 "
			<< f.ly << " 0.000000  0.000000 0.000000 "
			<< f.lz << "\""
			<< " pbc=\"" << (f.pbc[0]?'T':'F') << ' '
						 << (f.pbc[1]?'T':'F') << ' '
						 << (f.pbc[2]?'T':'F') << '"';
	if (!props_decl.empty())
		out << " properties=\"" << props_decl << '"';
	return out.str();
}

/// Build properties= declaration string from frame flags
inline std::string build_props_decl(const XyzaFrame& f) {
	std::string p;
	auto add = [&](const char* s) { p += (p.empty() ? "" : ":"); p += s; };
	if (f.has_charge)    add("charge");
	if (f.has_velocity)  add("velocity");
	if (f.has_force)     add("force");
	if (f.has_energy_col)add("energy");
	return p;
}

/// Emit one XyzaAtomRecord as an .xyza atom line (kcal/mol on disk)
inline void emit_atom(std::ostream& out, const XyzaAtomRecord& a,
					   const XyzaFrame& f, int coord_prec, int prop_prec)
{
	out << std::left  << std::setw(4)  << a.species
		<< std::right << std::fixed << std::setprecision(coord_prec)
		<< std::setw(14) << a.position_A.x
		<< std::setw(14) << a.position_A.y
		<< std::setw(14) << a.position_A.z;

	if (f.has_charge)
		out << std::setw(12) << std::setprecision(prop_prec) << a.charge_e;

	if (f.has_velocity)
		out << std::setw(12) << std::setprecision(prop_prec) << a.velocity_A_per_fs.x
			<< std::setw(12) << std::setprecision(prop_prec) << a.velocity_A_per_fs.y
			<< std::setw(12) << std::setprecision(prop_prec) << a.velocity_A_per_fs.z;

	if (f.has_force) {
		// Convert eV/Å back to kcal/(mol·Å) for file storage
		out << std::setw(12) << std::setprecision(prop_prec) << (a.force_eV_per_A.x * eV_A_to_kcal_mol_A)
			<< std::setw(12) << std::setprecision(prop_prec) << (a.force_eV_per_A.y * eV_A_to_kcal_mol_A)
			<< std::setw(12) << std::setprecision(prop_prec) << (a.force_eV_per_A.z * eV_A_to_kcal_mol_A);
	}

	if (f.has_energy_col)
		// Convert eV back to kcal/mol for file storage
		out << std::setw(14) << std::setprecision(prop_prec) << (a.energy_eV * eV_to_kcal);

	out << '\n';
}

/// Emit one complete XyzaFrame to stream
inline void emit_frame(std::ostream& out, const XyzaFrame& frame,
						int coord_prec = 6, int prop_prec = 6)
{
	out << frame.atoms.size() << '\n';
	out << make_dynamic_comment(frame, build_props_decl(frame)) << '\n';
	for (const auto& a : frame.atoms)
		emit_atom(out, a, frame, coord_prec, prop_prec);
}

} // namespace detail

// ============================================================================
// Read API
// ============================================================================

/**
 * read_xyza_dynamic — read all frames from a .xyza file.
 *
 * Each frame is independently parsed (per-frame properties= declaration,
 * per WO-66A scope / spec §4.3).  Missing Q/V/F columns are zero-filled.
 * Charge model is extracted from the comment line if present.
 *
 * Returns XyzaDynamicReadResult with frames + diagnostics.
 */
inline XyzaDynamicReadResult read_xyza_dynamic(
	const std::string& path,
	ZeroFillPolicy zfp = ZeroFillPolicy::Warn)
{
	XyzaDynamicReadResult res;

	std::ifstream in(path);
	if (!in) {
		res.push_error(-1, -1, "Cannot open: " + path);
		return res;
	}

	int fi = 0;
	while (in.peek() != EOF) {
		// Skip blank lines between frames
		std::string line;
		while (std::getline(in, line)) {
			auto trimmed = line;
			trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
			if (!trimmed.empty()) { break; }
		}
		if (in.eof() && line.empty()) break;

		// Put the line back via stringstream bridge + re-read via unified parser
		std::string all_frame = line + "\n";
		while (std::getline(in, line)) {
			all_frame += line + "\n";
			// Count atom lines read: stop after N atoms + comment (2 header lines)
			// We parse N from the first line to know when to stop.
			break;  // get comment line
		}
		// Parse N from first chunk
		int N = 0;
		{
			std::istringstream ss(all_frame);
			std::string nline;
			std::getline(ss, nline);
			try { N = std::stoi(nline); } catch (...) { break; }
		}
		// Read N atom lines
		for (int i = 0; i < N; ++i) {
			if (!std::getline(in, line)) break;
			all_frame += line + "\n";
		}

		// Now parse the assembled frame text with the unified xyza parser
		ParseContext ctx;
		XYZFrame uf;
		std::istringstream frame_stream(all_frame);
		int lineno = 0;
		if (!detail::parse_frame_xyza(frame_stream, uf, ctx, fi, lineno)) {
			for (const auto& e : ctx.errors)
				res.push_error(fi, -1, e.message);
			break;
		}

		// Bridge to XyzaFrame
		XyzaFrame xf;
		xf.has_charge    = uf.has_charge;
		xf.has_velocity  = uf.has_velocity;
		xf.has_force     = uf.has_force;
		xf.has_energy_col= uf.has_energy_col;
		xf.raw_comment   = uf.comment;
		xf.charge_model  = parse_charge_model(uf.comment);

		// Frame-level energy and temperature (kcal/mol → eV)
		if (uf.energy)
			xf.energy_eV = *uf.energy * kcal_to_eV;
		if (uf.temperature)
			xf.temperature_K = *uf.temperature;

		// Box
		if (uf.box) {
			xf.has_box = true;
			xf.lx = uf.box->ax; xf.ly = uf.box->ay; xf.lz = uf.box->az;
			xf.pbc[0] = uf.box->pbc[0];
			xf.pbc[1] = uf.box->pbc[1];
			xf.pbc[2] = uf.box->pbc[2];
		}

		// step= / time= from comment
		detail::parse_dynamic_comment(uf.comment, xf.step, xf.time_fs);
		xf.step = static_cast<uint64_t>(fi);  // fallback if not declared

		// Per-atom conversion
		xf.atoms.reserve(static_cast<size_t>(uf.N));
		for (int ai = 0; ai < static_cast<int>(uf.atoms.size()); ++ai) {
			xf.atoms.push_back(
				detail::atom_from_unified(uf.atoms[ai],
					xf.has_charge, xf.has_velocity, xf.has_force, xf.has_energy_col,
					fi, ai, res, zfp));
		}

		// Propagate low-level warnings
		for (const auto& w : ctx.warnings)
			res.push_warn(fi, w.atom_index, w.message);

		res.frames.push_back(std::move(xf));
		++fi;
	}

	return res;
}

// ============================================================================
// Write API
// ============================================================================

/**
 * write_xyza_dynamic — write all frames to a .xyza file (overwrite).
 * Returns true on success.
 */
inline bool write_xyza_dynamic(const std::string& path,
								const std::vector<XyzaFrame>& frames,
								int coord_prec = 6, int prop_prec = 6)
{
	std::ofstream out(path);
	if (!out) return false;
	for (const auto& f : frames)
		detail::emit_frame(out, f, coord_prec, prop_prec);
	return out.good();
}

// ============================================================================
// XyzaDynamicWriter  —  streaming frame-by-frame archive writer (WO-66A §9)
// ============================================================================
//
// Usage:
//   XyzaDynamicWriter w("traj.xyza");
//   while (simulation_running) {
//       w.append(current_frame);
//   }
//   // file is flushed and closed on destruction

class XyzaDynamicWriter {
public:
	explicit XyzaDynamicWriter(const std::string& path,
								int coord_prec = 6, int prop_prec = 6)
		: path_(path), coord_prec_(coord_prec), prop_prec_(prop_prec)
		, out_(path, std::ios::out | std::ios::trunc)
	{
		if (!out_)
			throw std::runtime_error("XyzaDynamicWriter: cannot open " + path);
	}

	~XyzaDynamicWriter() { flush(); }

	/// Append one frame to the archive.  Thread-unsafe — caller serialises.
	void append(const XyzaFrame& frame) {
		detail::emit_frame(out_, frame, coord_prec_, prop_prec_);
		++frames_written_;
	}

	/// Flush to disk without closing (useful for live monitoring).
	void flush() { out_.flush(); }

	int frames_written() const { return frames_written_; }
	const std::string& path() const { return path_; }

private:
	std::string   path_;
	int           coord_prec_;
	int           prop_prec_;
	std::ofstream out_;
	int           frames_written_ = 0;
};

// ============================================================================
// Bridge: XyzaFrame <-> XYZFrame (unified layer)  [WO-66A §7, §9]
// ============================================================================

/// Convert XyzaFrame to XYZFrame (eV → kcal/mol, concrete → optional).
/// Used by the formation engine pre-state consumer (WO-66B handoff).
inline XYZFrame to_xyz_frame(const XyzaFrame& f) {
	XYZFrame out;
	out.N             = f.atom_count();
	out.comment       = f.raw_comment;
	out.has_charge    = f.has_charge;
	out.has_velocity  = f.has_velocity;
	out.has_force     = f.has_force;
	out.has_energy_col= f.has_energy_col;
	out.frame_index   = static_cast<int>(f.step);
	if (f.energy_eV != 0.0) out.energy = f.energy_eV * eV_to_kcal;
	if (f.temperature_K > 0.0) out.temperature = f.temperature_K;
	if (f.has_box) {
		XYZBox box;
		box.ax = f.lx; box.ay = f.ly; box.az = f.lz;
		box.pbc[0] = f.pbc[0]; box.pbc[1] = f.pbc[1]; box.pbc[2] = f.pbc[2];
		box.lattice = {f.lx,0,0, 0,f.ly,0, 0,0,f.lz};
		out.box = box;
	}
	out.atoms.reserve(static_cast<size_t>(f.atom_count()));
	for (const auto& a : f.atoms) {
		AtomRecord r;
		r.symbol = a.species;
		r.Z      = 0;  // caller should fill Z from element table if needed
		r.x = a.position_A.x;
		r.y = a.position_A.y;
		r.z = a.position_A.z;
		if (f.has_charge)    r.q = a.charge_e;
		if (f.has_velocity)  r.v = a.velocity_A_per_fs;
		if (f.has_force)     r.f = Vec3{a.force_eV_per_A.x * eV_A_to_kcal_mol_A,
										 a.force_eV_per_A.y * eV_A_to_kcal_mol_A,
										 a.force_eV_per_A.z * eV_A_to_kcal_mol_A};
		if (f.has_energy_col)r.e = a.energy_eV * eV_to_kcal;
		out.atoms.push_back(std::move(r));
	}
	return out;
}

/// Convert XYZFrame to XyzaFrame (kcal/mol → eV, optional → concrete zero-fill).
inline XyzaFrame from_xyz_frame(const XYZFrame& f) {
	XyzaFrame out;
	out.step          = static_cast<uint64_t>(f.frame_index);
	out.has_charge    = f.has_charge;
	out.has_velocity  = f.has_velocity;
	out.has_force     = f.has_force;
	out.has_energy_col= f.has_energy_col;
	out.raw_comment   = f.comment;
	out.charge_model  = parse_charge_model(f.comment);
	if (f.energy)      out.energy_eV    = *f.energy * kcal_to_eV;
	if (f.temperature) out.temperature_K = *f.temperature;
	if (f.box) {
		out.has_box = true;
		out.lx = f.box->ax; out.ly = f.box->ay; out.lz = f.box->az;
		out.pbc[0] = f.box->pbc[0];
		out.pbc[1] = f.box->pbc[1];
		out.pbc[2] = f.box->pbc[2];
	}
	out.atoms.reserve(f.atoms.size());
	for (const auto& a : f.atoms) {
		XyzaAtomRecord r;
		r.species    = a.symbol;
		r.position_A = {a.x, a.y, a.z};
		r.charge_e          = a.q ? *a.q : 0.0;
		r.velocity_A_per_fs = a.v ? *a.v : Vec3{};
		Vec3 fk = a.f ? *a.f : Vec3{};
		r.force_eV_per_A    = {fk.x * kcal_mol_A_to_eV_A,
								fk.y * kcal_mol_A_to_eV_A,
								fk.z * kcal_mol_A_to_eV_A};
		r.energy_eV = a.e ? (*a.e * kcal_to_eV) : 0.0;
		out.atoms.push_back(std::move(r));
	}
	return out;
}

// ============================================================================
// Live report summary helpers  (WO-66A scope item 8)
// ============================================================================

struct XyzaFrameSummary {
	int      frame_index   = 0;
	uint64_t step          = 0;
	double   time_fs       = 0.0;
	double   energy_eV     = 0.0;
	double   temperature_K = 0.0;
	double   total_charge_e= 0.0;
	double   max_force_eV_per_A  = 0.0;
	double   rms_velocity_A_per_fs = 0.0;
	double   rms_force_eV_per_A    = 0.0;
	int      atom_count    = 0;
	ChargeModel charge_model = ChargeModel::Unknown;
};

inline XyzaFrameSummary summarise(const XyzaFrame& f, int idx = 0) {
	XyzaFrameSummary s;
	s.frame_index          = idx;
	s.step                 = f.step;
	s.time_fs              = f.time_fs;
	s.energy_eV            = f.energy_eV;
	s.temperature_K        = f.temperature_K;
	s.total_charge_e       = f.total_charge_e();
	s.max_force_eV_per_A   = f.max_force_eV_per_A();
	s.rms_velocity_A_per_fs= f.rms_velocity_A_per_fs();
	s.rms_force_eV_per_A   = f.rms_force_eV_per_A();
	s.atom_count           = f.atom_count();
	s.charge_model         = f.charge_model;
	return s;
}

/// Print a compact Q/V/F summary table for live reporting.
inline void print_qvf_summary(std::ostream& out,
							   const std::vector<XyzaFrameSummary>& sums)
{
	out << "  frame  step    E(eV)       T(K)     Q_tot(e)  |F|_max(eV/A)  v_rms(A/fs)\n";
	out << "  -----  ----  ----------  --------  ----------  ------------  -----------\n";
	for (const auto& s : sums) {
		out << "  " << std::setw(5) << s.frame_index
			<< "  " << std::setw(4) << s.step
			<< "  " << std::fixed << std::setprecision(4) << std::setw(10) << s.energy_eV
			<< "  " << std::setw(8) << std::setprecision(2) << s.temperature_K
			<< "  " << std::setw(10) << std::setprecision(4) << s.total_charge_e
			<< "  " << std::setw(12) << std::setprecision(4) << s.max_force_eV_per_A
			<< "  " << std::setw(11) << std::setprecision(4) << s.rms_velocity_A_per_fs
			<< '\n';
	}
}

} // namespace io
} // namespace vsepr
