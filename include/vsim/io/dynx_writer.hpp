#pragma once
/**
 * include/vsim/io/dynx_writer.hpp
 * ==================================
 * WO-VSIM-DYNX-V1-A / WO-VSIM-DYNX-V1-B  |  Phase 9  |  v5.1.x
 *
 * Dynx v1 — post-compiled dynamic session archive.
 *
 * Format (text-based, line-oriented):
 *
 *   #dynx v1
 *   #source <path>
 *   #source_hash <sha256-hex | "none">
 *   #kernel_version <string>
 *   #frame_count <N>
 *   #frame_interval <dt_fs>
 *   #particle_count <N>
 *   #timestamp <ISO-8601>
 *   FRAME <index> <time_fs>
 *   <N> <symbol> <x> <y> <z> [<vx> <vy> <vz>] [<energy>]
 *   FORCE      <idx> <fx> <fy> <fz>                      (optional)
 *   BOND_FORCE <i> <j> <bfx> <bfy> <bfz>                (optional)
 *   FIELD      <label> <fx> <fy> <fz>                    (optional)
 *   EVENT      <kind> <event_id> <source> <value>        (optional)
 *   RENDER     <idx> <r> <g> <b> <tag> <visible>         (optional)
 *   CAMERA     <label> <x> <y> <z> <pitch> <yaw> <zoom>  (optional)
 *   END_FRAME
 *   ...
 *   #END_DYNX
 *
 * Notes:
 *   - Velocities are optional; if omitted, 0 0 0 is used on read.
 *   - Energy is optional; per-particle scalar in kcal/mol.
 *   - Frame time is monotonically increasing (validated by DynxValidator).
 *   - All comment lines beginning with '#' are metadata or ignored.
 *   - FORCE/BOND_FORCE/FIELD/EVENT/RENDER/CAMERA lines are optional; unknown
 *     line prefixes are silently skipped by readers (forward-compatible).
 *
 * Roles:
 *   DynxWriter    — opens a file, writes frames, closes + finalizes header.
 *   DynxEmitter   — post-step hook: feeds rich frames to writer + live cache.
 *   DynxLiveCache — single-slot live frame cache for viewer polling.
 *   DynxInspector — reads metadata without full frame parse.
 *   DynxValidator — full structural validation (monotonic time, particle count).
 *
 * Group 57 — Dynx v1 session archive
 * Group 71 — DynxEmitter post-step pipeline (WO-72B)
 */

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vsim {
namespace io {

// ============================================================================
// DynxParticleState  —  one particle record inside a frame
// ============================================================================
struct DynxParticleState {
	std::string symbol;
	std::array<double, 3> pos = {0, 0, 0};       // Å
	std::array<double, 3> vel = {0, 0, 0};        // Å/fs
	double energy = 0.0;                           // kcal/mol (optional)
	bool   has_vel    = false;
	bool   has_energy = false;
};

// ============================================================================
// DynxFrame  —  one time step  (v1 baseline, used by DynxWriter)
// ============================================================================
struct DynxFrame {
	int    index   = 0;
	double time_fs = 0.0;
	std::vector<DynxParticleState> particles;
};

// ============================================================================
// WO-72B  —  Rich emitter data structs
// ============================================================================

// Per-particle force vector (kcal/mol/Å)
struct DynxForceState {
	int                  particle_idx = 0;
	std::array<double,3> force        = {0,0,0};
};

// Per-bond force vector
struct DynxBondForce {
	int                  i     = 0;
	int                  j     = 0;
	std::array<double,3> force = {0,0,0};
};

// Field vector (flux, stress component, etc.)
struct DynxFieldVector {
	std::string          label;            // e.g. "stress_xx", "flux_x"
	std::array<double,3> vec = {0,0,0};
};

// Event packet — sourced from KernelEventLog
struct DynxEventPacket {
	std::string kind;        // "reaction" | "checkpoint" | "phase_transition" | ...
	uint64_t    event_id = 0;
	std::string source;      // source_formula from KernelEvent
	double      value    = 0.0;
};

// Per-particle render metadata
struct DynxRenderMeta {
	int         particle_idx = 0;
	uint8_t     r = 255, g = 255, b = 255;  // color override (RGB)
	std::string tag;                         // bead tag label
	bool        visible = true;
};

// Camera state snapshot
struct DynxCameraState {
	std::string label;           // e.g. "default", "close_up"
	double      x     = 0.0;
	double      y     = 0.0;
	double      z     = 0.0;
	double      pitch = 0.0;     // degrees
	double      yaw   = 0.0;     // degrees
	double      zoom  = 1.0;
};

// DynxRichFrame  —  extended frame carrying all spec'd emitter data
struct DynxRichFrame {
	// v1 baseline
	int    index   = 0;
	double time_fs = 0.0;
	std::vector<DynxParticleState> particles;

	// WO-72B additions (all optional — empty = not emitted)
	std::vector<DynxForceState>   forces;
	std::vector<DynxBondForce>    bond_forces;
	std::vector<DynxFieldVector>  field_vectors;
	std::vector<DynxEventPacket>  events;
	std::vector<DynxRenderMeta>   render;
	std::vector<DynxCameraState>  cameras;

	// Convert to baseline DynxFrame (for DynxWriter)
	DynxFrame to_base_frame() const {
		DynxFrame f;
		f.index    = index;
		f.time_fs  = time_fs;
		f.particles = particles;
		return f;
	}
};

// DynxEmitContext  —  caller-populated input bag for one post-step emit
struct DynxEmitContext {
	// Required
	int    frame_index = 0;
	double time_fs     = 0.0;
	std::vector<DynxParticleState> particles;

	// Optional rich data
	std::vector<DynxForceState>   forces;
	std::vector<DynxBondForce>    bond_forces;
	std::vector<DynxFieldVector>  field_vectors;
	std::vector<DynxEventPacket>  events;     // caller may pre-populate;
											  // DynxEmitter also appends
											  // KernelEventLog entries
											  // for this frame_index
	std::vector<DynxRenderMeta>   render;
	std::vector<DynxCameraState>  cameras;
};

// ============================================================================
// DynxHeader  —  file-level metadata (written at open, finalized at close)
// ============================================================================
struct DynxHeader {
	std::string source_path;          // originating .vsim path
	std::string source_hash = "none"; // SHA-256 hex or "none"
	std::string kernel_version = "v5.1.x";
	double      frame_interval_fs = 1.0;
	int         particle_count = 0;   // particles per frame (0 = variable)
	std::string timestamp;            // ISO-8601 UTC, filled at open

	// Finalised at close:
	int  frame_count = 0;
};

// ============================================================================
// DynxWriter
// ============================================================================
//
// Usage:
//   DynxWriter w;
//   if (!w.open("run.dynx", hdr)) { /* error */ }
//   w.write_frame(frame);
//   w.close();
//
class DynxWriter {
public:
	DynxWriter()  = default;
	~DynxWriter() { if (is_open_) close(); }

	// Disable copy; allow move.
	DynxWriter(const DynxWriter&)            = delete;
	DynxWriter& operator=(const DynxWriter&) = delete;
	DynxWriter(DynxWriter&&)                 = default;
	DynxWriter& operator=(DynxWriter&&)      = default;

	// Open a file and write the header block.
	// Returns false on I/O failure.
	bool open(const std::string& path, DynxHeader hdr);

	// Write one frame (v1 baseline — positions/velocities/energy only).
	// Returns false if the writer is not open.
	bool write_frame(const DynxFrame& frame);

	// Write one rich frame (WO-72B — includes forces, events, render, camera).
	// Falls back gracefully to write_frame() if no rich data is present.
	// Returns false if the writer is not open.
	bool write_rich_frame(const DynxRichFrame& frame);

	// -------------------------------------------------------------------------
	// Streaming API (WO-72D)
	// Allows callers to emit a frame line-by-line without building intermediate
	// vectors.  Sequence: begin_frame → write_particle* → write_force* → ... →
	// end_frame.  begin_frame / end_frame must be paired; calling begin_frame
	// while a frame is already open finalises the current one first.
	// -------------------------------------------------------------------------

	// Open a new frame block.  Writes "FRAME <frame_id> <t>" to the stream.
	// dt is stored and written as a comment for diagnostic purposes.
	bool begin_frame(std::size_t frame_id, double t, double dt = 0.0);

	// Particle position (+ optional velocity / energy depending on flags).
	bool write_particle(std::size_t idx, double x, double y, double z);

	// Force on particle idx  (kcal/mol/Å)
	bool write_force(std::size_t idx, double fx, double fy, double fz);

	// Bond force between particles i and j
	bool write_bond_force(std::size_t i, std::size_t j,
						  double bfx, double bfy, double bfz);

	// Named field vector  (flux, stress component, …)
	bool write_field(const std::string& label, double fx, double fy, double fz);

	// Simulation event packet
	bool write_event(const std::string& kind, int event_id,
					 const std::string& source, double value);

	// Per-particle render metadata
	bool write_render(std::size_t idx, int r, int g, int b,
					  const std::string& tag, bool visible);

	// Camera state snapshot
	bool write_camera(const std::string& label,
					  double x, double y, double z,
					  double pitch, double yaw, double zoom);

	// Close the current frame block.  Writes "END_FRAME".
	// Calling end_frame when no frame is open is a no-op.
	bool end_frame();

	// Finalize the file: patch frame_count in the header comment, close stream.
	bool close();

	bool is_open()   const { return is_open_; }
	int  frame_count() const { return frames_written_; }

	const std::string& path()  const { return path_; }
	const std::string& error() const { return error_; }

private:
	bool        is_open_       = false;
	bool        frame_open_    = false;   // true between begin_frame / end_frame
	int         frames_written_ = 0;
	std::string path_;
	std::string error_;
	DynxHeader  hdr_;
	// Internal file handle (FILE*)
	void*       fp_ = nullptr;
};

// ============================================================================
// DynxInspectResult  —  output of inspect()
// ============================================================================
struct DynxInspectResult {
	bool        ok             = false;
	std::string source_path;
	std::string source_hash;
	std::string kernel_version;
	int         frame_count    = 0;
	double      frame_interval_fs = 0.0;
	int         particle_count = 0;
	std::string timestamp;
	std::string error;
};

// ============================================================================
// DynxValidateResult  —  output of validate()
// ============================================================================
struct DynxValidateResult {
	bool        ok              = false;
	int         frame_count     = 0;
	int         particle_count  = 0;
	bool        monotonic_time  = false;
	bool        hash_present    = false;
	std::string error;
	std::vector<std::string> warnings;
};

// ============================================================================
// Free-standing inspect / validate functions
// ============================================================================

// Read only the header comment block.  Fast — does not parse frames.
DynxInspectResult  dynx_inspect (const std::string& path);

// Full structural validation of a .dynx file.
DynxValidateResult dynx_validate(const std::string& path);

} // namespace io
} // namespace vsim
