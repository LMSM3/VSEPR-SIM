#pragma once
/**
 * include/vsim/io/demo_frame_sampler.hpp
 * =========================================
 * WO-OUTPUT-P2-B  |  Output System Expansion Phase 2  |  v5.1.x
 *
 * DemoFrameSampler  —  selects a representative subset of frames from a
 * .dynx archive and writes them to a condensed .demo.dynx file.
 *
 * Strategies (ExportDemoSection::strategy):
 *   first        - frames 0..N-1
 *   last         - last N frames
 *   uniform      - evenly spaced indices across the full archive (default)
 *   event_gated  - frames containing at least one EVENT packet
 *   keyframe     - frames ranked by event-packet density; falls back to uniform
 *
 * Usage:
 *   DemoSampleResult r = DemoFrameSampler::sample(
 *       "run.dynx", "run.demo.dynx", config);
 *   if (!r.ok) { // r.error }
 *
 * Group 73  —  DemoFrameSampler all strategies
 */

#include "vsim/vsim_document.hpp"  // ExportDemoSection
#include "vsim/io/dynx_writer.hpp"
#include <string>
#include <vector>

namespace vsim {
namespace io {

// ---------------------------------------------------------------------------
// DemoSampleResult
// ---------------------------------------------------------------------------
struct DemoSampleResult {
	bool                 ok          = false;
	std::string          error;
	int                  frames_in   = 0;   // total frames in source
	int                  frames_out  = 0;   // frames written to demo file
	std::vector<int>     frame_indices;      // which source indices were selected
	std::string          output_path;
	std::string          source_hash;        // propagated from source archive header
};

// ---------------------------------------------------------------------------
// DemoFrameSampler
// ---------------------------------------------------------------------------
class DemoFrameSampler {
public:
	// Sample from source_dynx, write condensed output to out_path.
	// cfg controls strategy and demo_frames count.
	static DemoSampleResult sample(
		const std::string&      source_dynx,
		const std::string&      out_path,
		const ExportDemoSection& cfg);

	// Compute which frame indices will be selected (no I/O).
	// keyframe_event_counts[i] = number of EVENT packets in frame i (used by
	// "keyframe" strategy; may be empty for other strategies).
	static std::vector<int> compute_indices(
		int total_frames,
		const ExportDemoSection& cfg,
		const std::vector<int>& event_frame_indices = {},
		const std::vector<int>& keyframe_event_counts = {});

private:
	static std::vector<int> strategy_first       (int total, int n);
	static std::vector<int> strategy_last        (int total, int n);
	static std::vector<int> strategy_uniform     (int total, int n);
	static std::vector<int> strategy_event_gated (const std::vector<int>& event_frames, int n);
	static std::vector<int> strategy_keyframe    (const std::vector<int>& event_counts, int n);
};

} // namespace io
} // namespace vsim
