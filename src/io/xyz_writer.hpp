#pragma once
/**
 * xyz_writer.hpp -- Spec-compliant unified XYZ family writer
 * ===========================================================
 * VSEPR-SIM  |  WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * Implements the writer API described in docs/xyz_file_formats.tex §9:
 *
 *   write_xyz  (path, frame)          -- static geometry
 *   write_xyza (path, frame)          -- + extended property columns
 *   write_xyzc (path, data)           -- checkpoint header + .xyza frame
 *   write_xyzf (path, frames, append) -- multi-frame trajectory
 *
 * Comment-line convention (spec §2.2):
 *   "<name> | E = <val> kcal/mol | T = <val> K | <notes>"
 *
 * Coordinate precision: 6 decimal places (spec §1.2, ~0.1 pm resolution).
 */

#include "xyz_unified.hpp"
#include <ostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr {
namespace io {

// ============================================================================
// WriterConfig — shared output settings
// ============================================================================

struct XYZWriterConfig {
	int  coord_precision  = 6;     // decimal places for coordinates (spec §1.2)
	int  prop_precision   = 6;     // decimal places for extended properties
	bool write_charge     = true;
	bool write_velocity   = true;
	bool write_force      = true;
	bool write_energy_col = true;
};

// ============================================================================
// Internal helpers
// ============================================================================

namespace detail {

// Emit one .xyz atom line: "Sym  x  y  z"
inline void write_atom_xyz(std::ostream& out, const AtomRecord& a, int prec) {
	out << std::left << std::setw(4) << a.symbol
		<< std::right << std::fixed << std::setprecision(prec)
		<< std::setw(14) << a.x
		<< std::setw(14) << a.y
		<< std::setw(14) << a.z
		<< '\n';
}

// Emit one .xyza atom line, appending only the optional fields that are present
// and were requested by the config.
inline void write_atom_xyza(std::ostream& out, const AtomRecord& a,
							  const XYZWriterConfig& cfg)
{
	out << std::left << std::setw(4) << a.symbol
		<< std::right << std::fixed << std::setprecision(cfg.coord_precision)
		<< std::setw(14) << a.x
		<< std::setw(14) << a.y
		<< std::setw(14) << a.z;

	int pp = cfg.prop_precision;
	if (cfg.write_charge && a.q) {
		out << std::setw(12) << std::setprecision(pp) << *a.q;
	}
	if (cfg.write_velocity && a.v) {
		out << std::setw(12) << std::setprecision(pp) << a.v->x
			<< std::setw(12) << std::setprecision(pp) << a.v->y
			<< std::setw(12) << std::setprecision(pp) << a.v->z;
	}
	if (cfg.write_force && a.f) {
		out << std::setw(12) << std::setprecision(pp) << a.f->x
			<< std::setw(12) << std::setprecision(pp) << a.f->y
			<< std::setw(12) << std::setprecision(pp) << a.f->z;
	}
	if (cfg.write_energy_col && a.e) {
		out << std::setw(14) << std::setprecision(pp) << *a.e;
	}
	out << '\n';
}

// Build properties= declaration string from what the frame actually carries
inline std::string build_properties_decl(const XYZFrame& frame,
										   const XYZWriterConfig& cfg)
{
	std::string props;
	if (cfg.write_charge     && frame.has_charge)     props += (props.empty() ? "" : ":") + std::string("charge");
	if (cfg.write_velocity   && frame.has_velocity)   props += (props.empty() ? "" : ":") + std::string("velocity");
	if (cfg.write_force      && frame.has_force)      props += (props.empty() ? "" : ":") + std::string("force");
	if (cfg.write_energy_col && frame.has_energy_col) props += (props.empty() ? "" : ":") + std::string("energy");
	return props;
}

// Build the comment line for a frame (spec §2.2)
// Preserves original comment if it already has the pipe-delimited structure;
// otherwise constructs from scratch using energy/temperature fields.
inline std::string make_comment(const XYZFrame& frame, const std::string& props_decl) {
	std::ostringstream out;

	// If the original comment already has our convention, keep it and append
	// properties= only if needed.
	bool already_has_props = (frame.comment.find("properties=") != std::string::npos);

	if (!frame.comment.empty() && !already_has_props) {
		out << frame.comment;
	} else if (frame.comment.empty()) {
		out << "VSEPR-SIM frame " << frame.frame_index;
		if (frame.energy)      out << " | E = " << std::fixed << std::setprecision(4) << *frame.energy << " kcal/mol";
		if (frame.temperature) out << " | T = " << std::fixed << std::setprecision(2) << *frame.temperature << " K";
	} else {
		// already has properties= — keep verbatim
		out << frame.comment;
	}

	if (!props_decl.empty() && !already_has_props)
		out << " properties=\"" << props_decl << '"';

	return out.str();
}

// Emit box/lattice as comment-line key (spec §5)
inline std::string box_comment(const XYZBox& b) {
	std::ostringstream s;
	s << std::fixed << std::setprecision(6);
	s << "Lattice=\""
	  << b.lattice[0] << ' ' << b.lattice[1] << ' ' << b.lattice[2] << "  "
	  << b.lattice[3] << ' ' << b.lattice[4] << ' ' << b.lattice[5] << "  "
	  << b.lattice[6] << ' ' << b.lattice[7] << ' ' << b.lattice[8]
	  << "\" pbc=\""
	  << (b.pbc[0]?'T':'F') << ' '
	  << (b.pbc[1]?'T':'F') << ' '
	  << (b.pbc[2]?'T':'F') << '"';
	return s.str();
}

// Emit CHECKPOINT / END_CHECKPOINT block (spec §4)
inline void write_checkpoint_header(std::ostream& out, const CheckpointState& ck) {
	out << "CHECKPOINT\n"
		<< "step       " << ck.step     << '\n'
		<< "time       " << std::fixed << std::setprecision(6) << ck.time   << " fs\n"
		<< "dt         " << std::fixed << std::setprecision(6) << ck.dt     << " fs\n"
		<< "T_target   " << std::fixed << std::setprecision(4) << ck.T_target << " K\n"
		<< "seed       " << ck.seed     << '\n';
	if (ck.box) {
		out << "box        "
			<< std::fixed << std::setprecision(6)
			<< ck.box->ax << ' ' << ck.box->ay << ' ' << ck.box->az << '\n';
	}
	out << "END_CHECKPOINT\n";
}

// Core .xyz frame emitter (coordinates only)
inline void emit_xyz_frame(std::ostream& out, const XYZFrame& frame,
							 const XYZWriterConfig& cfg)
{
	out << frame.N << '\n';
	// Comment line: use original if present, or auto-build
	std::string comment = frame.comment.empty()
		? build_comment("VSEPR-SIM", frame.energy, frame.temperature)
		: frame.comment;
	if (frame.box && frame.comment.find("Lattice=") == std::string::npos) {
		comment += " " + box_comment(*frame.box);
	}
	out << comment << '\n';
	for (const auto& a : frame.atoms)
		write_atom_xyz(out, a, cfg.coord_precision);
}

// Core .xyza frame emitter (coordinates + extended properties)
inline void emit_xyza_frame(std::ostream& out, const XYZFrame& frame,
							  const XYZWriterConfig& cfg)
{
	out << frame.N << '\n';
	std::string props_decl = build_properties_decl(frame, cfg);
	std::string comment    = make_comment(frame, props_decl);
	if (frame.box && comment.find("Lattice=") == std::string::npos)
		comment += " " + box_comment(*frame.box);
	out << comment << '\n';
	for (const auto& a : frame.atoms)
		write_atom_xyza(out, a, cfg);
}

} // namespace detail

// ============================================================================
// Free-function writers (spec §9 API)
// ============================================================================

/**
 * Write .xyz file (coordinates only).
 */
inline bool write_xyz(const std::string& path, const XYZFrame& frame,
					   const XYZWriterConfig& cfg = {})
{
	std::ofstream out(path);
	if (!out) return false;
	detail::emit_xyz_frame(out, frame, cfg);
	return out.good();
}

inline bool write_xyz(std::ostream& out, const XYZFrame& frame,
					   const XYZWriterConfig& cfg = {})
{
	detail::emit_xyz_frame(out, frame, cfg);
	return out.good();
}

/**
 * Write .xyza file (coordinates + extended properties).
 */
inline bool write_xyza(const std::string& path, const XYZFrame& frame,
						const XYZWriterConfig& cfg = {})
{
	std::ofstream out(path);
	if (!out) return false;
	detail::emit_xyza_frame(out, frame, cfg);
	return out.good();
}

inline bool write_xyza(std::ostream& out, const XYZFrame& frame,
						const XYZWriterConfig& cfg = {})
{
	detail::emit_xyza_frame(out, frame, cfg);
	return out.good();
}

/**
 * Write .xyzc file (checkpoint header + single .xyza frame).
 * data must contain exactly one frame and a checkpoint state.
 */
inline bool write_xyzc(const std::string& path, const XYZData& data,
						const XYZWriterConfig& cfg = {})
{
	if (data.frames.empty()) return false;
	std::ofstream out(path);
	if (!out) return false;
	if (data.checkpoint)
		detail::write_checkpoint_header(out, *data.checkpoint);
	detail::emit_xyza_frame(out, data.frames.front(), cfg);
	return out.good();
}

inline bool write_xyzc(std::ostream& out, const XYZData& data,
						const XYZWriterConfig& cfg = {})
{
	if (data.frames.empty()) return false;
	if (data.checkpoint) detail::write_checkpoint_header(out, *data.checkpoint);
	detail::emit_xyza_frame(out, data.frames.front(), cfg);
	return out.good();
}

/**
 * Write .xyzf file (multi-frame trajectory).
 *
 * @param append  If true, open in append mode (streaming use).
 *                If false, overwrite.
 */
inline bool write_xyzf(const std::string& path,
						const std::vector<XYZFrame>& frames,
						bool append = false,
						const XYZWriterConfig& cfg = {})
{
	auto mode = append
		? (std::ios::out | std::ios::app)
		: (std::ios::out | std::ios::trunc);
	std::ofstream out(path, mode);
	if (!out) return false;
	for (const auto& frame : frames)
		detail::emit_xyza_frame(out, frame, cfg);
	return out.good();
}

inline bool write_xyzf(std::ostream& out,
						const std::vector<XYZFrame>& frames,
						const XYZWriterConfig& cfg = {})
{
	for (const auto& frame : frames)
		detail::emit_xyza_frame(out, frame, cfg);
	return out.good();
}

// ============================================================================
// XYZFWriter — streaming append writer for real-time trajectory output
// ============================================================================

class XYZFWriter {
public:
	explicit XYZFWriter(const std::string& path, bool append = false,
						 const XYZWriterConfig& cfg = {})
		: cfg_(cfg)
	{
		auto mode = append
			? (std::ios::out | std::ios::app)
			: (std::ios::out | std::ios::trunc);
		out_.open(path, mode);
	}

	bool write_frame(const XYZFrame& frame) {
		if (!out_) return false;
		detail::emit_xyza_frame(out_, frame, cfg_);
		return out_.good();
	}

	bool write_frame(const XYZFrame& frame, const std::string& comment_override) {
		if (!out_) return false;
		XYZFrame copy = frame;
		copy.comment = comment_override;
		detail::emit_xyza_frame(out_, copy, cfg_);
		return out_.good();
	}

	bool is_open()  const { return out_.is_open(); }
	bool good()     const { return out_.good(); }
	void flush()          { out_.flush(); }
	void close()          { out_.close(); }

	XYZWriterConfig& config() { return cfg_; }

private:
	std::ofstream   out_;
	XYZWriterConfig cfg_;
};

// ============================================================================
// Convenience: export XYZData trajectory as .xyzf
// ============================================================================

inline bool export_xyzf(const std::string& path, const XYZData& data,
						  bool append = false,
						  const XYZWriterConfig& cfg = {})
{
	return write_xyzf(path, data.frames, append, cfg);
}

} // namespace io
} // namespace vsepr
