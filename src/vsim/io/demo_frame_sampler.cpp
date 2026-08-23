/**
 * src/vsim/io/demo_frame_sampler.cpp
 * =====================================
 * WO-OUTPUT-P2-B  |  Output System Expansion Phase 2  |  v5.1.x
 */

#include "vsim/io/demo_frame_sampler.hpp"
#include "vsim/io/dynx_reader.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace vsim {
namespace io {

// ---------------------------------------------------------------------------
// Index helpers
// ---------------------------------------------------------------------------

std::vector<int> DemoFrameSampler::strategy_first(int total, int n) {
	std::vector<int> idx;
	int count = std::min(n, total);
	idx.reserve(count);
	for (int i = 0; i < count; ++i) idx.push_back(i);
	return idx;
}

std::vector<int> DemoFrameSampler::strategy_last(int total, int n) {
	std::vector<int> idx;
	int count = std::min(n, total);
	int start = total - count;
	idx.reserve(count);
	for (int i = start; i < total; ++i) idx.push_back(i);
	return idx;
}

std::vector<int> DemoFrameSampler::strategy_uniform(int total, int n) {
	if (total <= 0 || n <= 0) return {};
	std::vector<int> idx;
	int count = std::min(n, total);
	idx.reserve(count);
	if (count == 1) {
		idx.push_back(0);
		return idx;
	}
	for (int k = 0; k < count; ++k) {
		int i = static_cast<int>(std::round(
			static_cast<double>(k) * (total - 1) / (count - 1)));
		idx.push_back(i);
	}
	// Deduplicate (can happen when total < count)
	idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
	return idx;
}

std::vector<int> DemoFrameSampler::strategy_event_gated(
	const std::vector<int>& event_frames, int n)
{
	std::vector<int> idx = event_frames;
	std::sort(idx.begin(), idx.end());
	idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
	if (static_cast<int>(idx.size()) > n) idx.resize(n);
	return idx;
}

// Rank frames by event-packet count (descending) and take the top N.
// Falls back to uniform when event_counts is empty or all-zero.
std::vector<int> DemoFrameSampler::strategy_keyframe(
	const std::vector<int>& event_counts, int n)
{
	const int total = static_cast<int>(event_counts.size());
	if (total == 0) return {};

	// Build (count, index) pairs
	std::vector<std::pair<int,int>> ranked;
	ranked.reserve(total);
	for (int i = 0; i < total; ++i)
		ranked.emplace_back(event_counts[i], i);

	std::stable_sort(ranked.begin(), ranked.end(),
		[](const auto& a, const auto& b) { return a.first > b.first; });

	// If the top entry has 0 events, fall back to uniform
	if (ranked[0].first == 0)
		return strategy_uniform(total, n);

	const int count = std::min(n, total);
	std::vector<int> idx;
	idx.reserve(count);
	for (int k = 0; k < count; ++k)
		idx.push_back(ranked[k].second);
	std::sort(idx.begin(), idx.end());  // chronological order
	return idx;
}

std::vector<int> DemoFrameSampler::compute_indices(
	int total_frames,
	const ExportDemoSection& cfg,
	const std::vector<int>& event_frame_indices,
	const std::vector<int>& keyframe_event_counts)
{
	const int n = cfg.demo_frames > 0 ? cfg.demo_frames : 10;
	if (cfg.strategy == "first")       return strategy_first(total_frames, n);
	if (cfg.strategy == "last")        return strategy_last(total_frames, n);
	if (cfg.strategy == "event_gated") return strategy_event_gated(event_frame_indices, n);
	if (cfg.strategy == "keyframe")    return strategy_keyframe(keyframe_event_counts, n);
	return strategy_uniform(total_frames, n);  // default
}

// ---------------------------------------------------------------------------
// sample()
// ---------------------------------------------------------------------------

DemoSampleResult DemoFrameSampler::sample(
	const std::string&      source_dynx,
	const std::string&      out_path,
	const ExportDemoSection& cfg)
{
	DemoSampleResult result;
	result.output_path = out_path;

	// 1. Read all source frames
	DynxReader reader;
	if (!reader.open(source_dynx)) {
		result.error = "DemoFrameSampler: cannot open source: " + source_dynx;
		return result;
	}

	std::vector<DynxRichFrame> all_frames;
	if (!reader.read_all_frames(all_frames) && all_frames.empty()) {
		result.error = "DemoFrameSampler: no frames in source: " + source_dynx;
		return result;
	}
	reader.close();

	result.frames_in   = static_cast<int>(all_frames.size());
	result.source_hash = reader.header().source_hash;  // propagate provenance

	// 2. Build auxiliary per-frame data used by advanced strategies.
	//    Always built (cheap) so any strategy can be dispatched uniformly.
	std::vector<int> event_frame_idx;
	std::vector<int> keyframe_event_counts(result.frames_in, 0);
	for (int i = 0; i < result.frames_in; ++i) {
		const int ev_count = static_cast<int>(all_frames[i].events.size());
		keyframe_event_counts[i] = ev_count;
		if (ev_count > 0)
			event_frame_idx.push_back(i);
	}

	// 3. Select frame indices
	if (cfg.strategy == "event_gated" && event_frame_idx.empty()) {
		// Fallback: if no events found, use uniform
		ExportDemoSection fallback = cfg;
		fallback.strategy = "uniform";
		result.frame_indices = compute_indices(result.frames_in, fallback,
											   event_frame_idx, keyframe_event_counts);
	} else {
		result.frame_indices = compute_indices(result.frames_in, cfg,
											   event_frame_idx, keyframe_event_counts);
	}

	if (result.frame_indices.empty()) {
		result.error = "DemoFrameSampler: no frames selected";
		return result;
	}

	// 4. Write condensed .demo.dynx
	const DynxHeader& src_hdr = reader.header();  // still valid after close()
	DynxHeader demo_hdr = src_hdr;
	// Annotate provenance: mark as demo subset, preserve original source_hash
	demo_hdr.kernel_version = src_hdr.kernel_version + " [demo]";

	DynxWriter writer;
	if (!writer.open(out_path, demo_hdr)) {
		result.error = "DemoFrameSampler: cannot create output: " + out_path;
		return result;
	}

	std::set<int> selected(result.frame_indices.begin(), result.frame_indices.end());
	for (int i = 0; i < result.frames_in; ++i) {
		if (selected.count(i))
			writer.write_rich_frame(all_frames[i]);
	}
	writer.close();

	result.frames_out = static_cast<int>(result.frame_indices.size());
	result.ok         = true;
	return result;
}

} // namespace io
} // namespace vsim
