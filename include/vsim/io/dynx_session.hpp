#pragma once
/**
 * include/vsim/io/dynx_session.hpp
 * ==================================
 * WO-72D / WO-75J  |  Phase 9  |  v5.1.x
 *
 * Session types that bind a loaded .dynx archive to its playback and
 * render state.  Used by the viewer replay controller and (in the future)
 * the WO-75J multi-window drag-and-drop environment.
 *
 * DynxSession — one loaded .dynx file with its own playback cursor and
 *               render / camera state.
 *
 * PlaybackState — current playback position and speed.
 * RenderSettings — per-session visual overrides.
 *
 * Group 72 — DynxReader / replay session
 */

#include "vsim/io/dynx_writer.hpp"  // DynxRichFrame, DynxCameraState, DynxHeader
#include "vsim/io/dynx_reader.hpp"  // DynxReader — needed by load()

#include <string>
#include <vector>

namespace vsim {
namespace io {

// ============================================================================
// PlaybackState
// ============================================================================

enum class PlaybackMode {
	Paused,
	Playing,
	Stepping,   // advance exactly one frame per trigger
};

struct PlaybackState {
	PlaybackMode mode          = PlaybackMode::Paused;
	int          current_frame = 0;   // index into DynxSession::frames
	float        fps           = 10.0f;
	bool         loop          = false;

	void reset()    { current_frame = 0; mode = PlaybackMode::Paused; }
	bool at_end(int total_frames) const {
		return current_frame >= total_frames - 1;
	}
	// Advance by one frame; returns false when the end is reached (and loop
	// is false).
	bool advance(int total_frames) {
		if (at_end(total_frames)) {
			if (loop) { current_frame = 0; return true; }
			mode = PlaybackMode::Paused;
			return false;
		}
		++current_frame;
		return true;
	}
};

// ============================================================================
// RenderSettings
// ============================================================================

struct RenderSettings {
	bool show_bonds           = true;
	bool show_force_vectors   = false;
	bool show_velocity_arrows = false;
	bool color_by_type        = true;
	float atom_scale          = 1.0f;   // radius multiplier
	float bond_thickness      = 1.0f;
};

// ============================================================================
// DynxSession
// ============================================================================

struct DynxSession {
	std::string             path;
	DynxHeader              header;
	std::vector<DynxRichFrame> frames;
	PlaybackState           playback;
	DynxCameraState         camera;
	RenderSettings          render;

	bool        loaded  = false;
	std::string error;

	// Convenience helpers
	int  frame_count()      const { return static_cast<int>(frames.size()); }
	bool has_frames()       const { return !frames.empty(); }

	const DynxRichFrame* current_frame() const {
		if (!has_frames()) return nullptr;
		int idx = playback.current_frame;
		if (idx < 0 || idx >= frame_count()) return nullptr;
		return &frames[static_cast<std::size_t>(idx)];
	}

	// Factory: open a .dynx file, read all frames, return a populated session.
	// On failure, returns a session with loaded=false and error set.
	static DynxSession load(const std::string& path);
};

} // namespace io
} // namespace vsim
