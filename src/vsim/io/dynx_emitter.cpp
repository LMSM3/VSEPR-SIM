/**
 * src/vsim/io/dynx_emitter.cpp
 * ==============================
 * WO-72B  -  .dynx Pipeline Emitter implementation
 *
 * WO-72B | V5.1.4
 */

#include "include/vsim/io/dynx_emitter.hpp"

namespace vsim {
namespace io {

// ============================================================================
// DynxLiveCache
// ============================================================================

void DynxLiveCache::push(DynxRichFrame frame) {
	std::lock_guard<std::mutex> lk(mutex_);
	slot_ = std::move(frame);
	++push_count_;
}

std::optional<DynxRichFrame> DynxLiveCache::poll() {
	std::lock_guard<std::mutex> lk(mutex_);
	auto result = std::move(slot_);
	slot_.reset();
	return result;
}

void DynxLiveCache::clear() {
	std::lock_guard<std::mutex> lk(mutex_);
	slot_.reset();
}

bool DynxLiveCache::has_frame() const {
	std::lock_guard<std::mutex> lk(mutex_);
	return slot_.has_value();
}

int DynxLiveCache::push_count() const {
	std::lock_guard<std::mutex> lk(mutex_);
	return push_count_;
}

void DynxLiveCache::reset_count() {
	std::lock_guard<std::mutex> lk(mutex_);
	push_count_ = 0;
}

// ============================================================================
// DynxEmitter — internal helpers
// ============================================================================

DynxRichFrame DynxEmitter::build_rich_frame(const DynxEmitContext& ctx) {
	DynxRichFrame f;
	f.index       = ctx.frame_index;
	f.time_fs     = ctx.time_fs;
	f.particles   = ctx.particles;
	f.forces      = ctx.forces;
	f.bond_forces = ctx.bond_forces;
	f.field_vectors = ctx.field_vectors;
	f.events      = ctx.events;   // start with caller-supplied events
	f.render      = ctx.render;
	f.cameras     = ctx.cameras;

	// Harvest KernelEventLog for events at this frame index
	auto& log = vsepr::kernel::KernelEventLog::instance();
	const uint64_t fid = static_cast<uint64_t>(ctx.frame_index);
	auto log_events = log.filter_by_frame(fid, fid);
	for (const auto& ev : log_events) {
		DynxEventPacket pkt;
		pkt.kind     = vsepr::kernel::kind_name(ev.kind);
		pkt.event_id = ev.event_id;
		pkt.source   = ev.source_formula;
		pkt.value    = ev.result_value;
		f.events.push_back(std::move(pkt));
	}

	return f;
}

// ============================================================================
// DynxEmitter — public API
// ============================================================================

bool DynxEmitter::open(const std::string& path, DynxHeader hdr) {
	path_  = path;
	error_.clear();
	if (!writer_.open(path, hdr)) {
		error_ = writer_.error();
		return false;
	}
	archive_open_  = true;
	frames_written_ = 0;
	return true;
}

bool DynxEmitter::emit_step(const DynxEmitContext& ctx) {
	// Build rich frame (harvests KernelEventLog)
	DynxRichFrame frame = build_rich_frame(ctx);

	// Push to live cache first (always — even if archive write fails)
	live_cache_.push(frame);

	// Write to archive
	if (archive_open_) {
		if (!writer_.write_rich_frame(frame)) {
			error_ = writer_.error();
			return false;
		}
		++frames_written_;
	}

	return true;
}

bool DynxEmitter::close() {
	if (!archive_open_) return true;
	const bool ok = writer_.close();
	if (!ok) error_ = writer_.error();
	archive_open_ = false;
	return ok;
}

} // namespace io
} // namespace vsim
