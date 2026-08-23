#pragma once
/**
 * include/vsim/io/dynx_emitter.hpp
 * ===================================
 * WO-72B  -  .dynx Pipeline Emitter
 *
 * Two complementary output paths per post-step call:
 *
 *   1. Archive path  (DynxWriter)
 *      Streams every rich frame to a .dynx file sequentially.
 *      Opened once at sim start, closed + finalized at sim end.
 *
 *   2. Live cache path  (DynxLiveCache)
 *      Single-slot mutex-guarded store of the latest DynxRichFrame.
 *      Viewers call poll() to get-and-clear the slot — they always
 *      see the freshest state without accumulating frame history.
 *      This is the "clear cache, keep live" window refresh model.
 *
 * Usage (post-step hook pattern):
 *
 *   DynxEmitter emitter;
 *   DynxHeader  hdr;
 *   hdr.source_path = "run.vsim";
 *   emitter.open("out/run.dynx", hdr);
 *
 *   // inside FIRE/MD step loop:
 *   DynxEmitContext ctx;
 *   ctx.frame_index = step;
 *   ctx.time_fs     = step * dt_fs;
 *   ctx.particles   = build_particle_states(...);
 *   ctx.forces      = build_force_states(...);
 *   // ... fill bond_forces, field_vectors, render, cameras as needed ...
 *   emitter.emit_step(ctx);   // writes to file + pushes to live cache
 *
 *   emitter.close();          // finalizes archive; live cache unaffected
 *
 *   // Viewer thread:
 *   auto frame = emitter.live_cache().poll();  // returns latest or nullopt
 *   if (frame) { render(*frame); }
 *
 * KernelEventLog integration:
 *   emit_step() automatically harvests KernelEvents whose frame_id matches
 *   ctx.frame_index from KernelEventLog::instance() and appends them as
 *   DynxEventPackets (in addition to any events pre-populated in ctx.events).
 *
 * WO-72B | V5.1.4
 */

#include "include/vsim/io/dynx_writer.hpp"
#include "include/kernel/kernel_event_log.hpp"

#include <mutex>
#include <optional>
#include <string>

namespace vsim {
namespace io {

// ============================================================================
// DynxLiveCache  —  single-slot live frame store for viewer polling
// ============================================================================

class DynxLiveCache {
public:
	// Push the latest rich frame.  Replaces any previously cached frame.
	void push(DynxRichFrame frame);

	// Get-and-clear.  Returns the stored frame (if any) and empties the slot.
	// The viewer calls this once per render tick; if nullopt the frame hasn't
	// changed since the last poll.
	std::optional<DynxRichFrame> poll();

	// Clear without returning the frame.
	void clear();

	// True if a new frame is waiting since the last poll.
	bool has_frame() const;

	// Total number of frames ever pushed (monotonically increasing).
	// Safe to call from any thread without consuming the cached frame.
	int push_count() const;

	// Reset the push counter (e.g. at session start).
	void reset_count();

private:
	mutable std::mutex           mutex_;
	std::optional<DynxRichFrame> slot_;
	int                          push_count_ = 0;
};

// ============================================================================
// DynxEmitter  —  post-step hook owning both output paths
// ============================================================================

class DynxEmitter {
public:
	DynxEmitter()  = default;
	~DynxEmitter() { if (archive_open_) close(); }

	// Disable copy; allow move.
	DynxEmitter(const DynxEmitter&)            = delete;
	DynxEmitter& operator=(const DynxEmitter&) = delete;
	DynxEmitter(DynxEmitter&&)                 = default;
	DynxEmitter& operator=(DynxEmitter&&)      = default;

	// Open the archive file.  Must be called before emit_step().
	// Returns false on I/O failure.  Live cache is always available.
	bool open(const std::string& path, DynxHeader hdr);

	// Post-step hook.  Call after every FIRE/MD step.
	//   - Harvests KernelEventLog events for ctx.frame_index.
	//   - Writes a rich frame to the archive (if open).
	//   - Pushes the rich frame to the live cache.
	// Returns false if the archive write fails (live cache is still updated).
	bool emit_step(const DynxEmitContext& ctx);

	// Finalize + close the archive.  Live cache is unaffected.
	bool close();

	bool        is_open()    const { return archive_open_; }
	int         frame_count() const { return frames_written_; }
	const std::string& path()  const { return path_; }
	const std::string& error() const { return error_; }

	// Access the live cache directly (e.g. from a viewer thread).
	DynxLiveCache&       live_cache()       { return live_cache_; }
	const DynxLiveCache& live_cache() const { return live_cache_; }

private:
	DynxWriter    writer_;
	DynxLiveCache live_cache_;
	bool          archive_open_  = false;
	int           frames_written_ = 0;
	std::string   path_;
	std::string   error_;

	// Build a DynxRichFrame from a DynxEmitContext, harvesting KernelEventLog.
	static DynxRichFrame build_rich_frame(const DynxEmitContext& ctx);

	// Write all rich-frame lines (FORCE, BOND_FORCE, FIELD, EVENT, RENDER, CAMERA)
	// to an open FILE* after the particle lines have been written.
	static bool write_rich_lines(std::FILE* fp, const DynxRichFrame& frame);
};

} // namespace io
} // namespace vsim
