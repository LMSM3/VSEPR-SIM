#pragma once
/**
 * include/vsim/io/demo_bundle_writer.hpp
 * ==========================================
 * WO-OUTPUT-P2-C  |  Output System Expansion Phase 2  |  v5.1.x
 *
 * DemoBundleWriter  —  assembles a .demo.X bundle from:
 *   - a .demo.dynx archive (produced by DemoFrameSampler)
 *   - the originating .vsim script (optional, cfg.include_source)
 *   - a demo_manifest.json (optional, cfg.include_manifest)
 *
 * Usage:
 *   DemoBundleResult r = DemoBundleWriter::write(
 *       "run.demo.dynx", "run.vsim", "run.demo.X",
 *       sample_result, cfg);
 *
 * Group 74  —  DemoBundleWriter round-trip
 */

#include "vsim/vsim_document.hpp"     // ExportDemoSection
#include "vsim/io/demo_frame_sampler.hpp"
#include <string>

namespace vsim {
namespace io {

// ---------------------------------------------------------------------------
// DemoBundleResult
// ---------------------------------------------------------------------------
struct DemoBundleResult {
	bool        ok          = false;
	std::string error;
	std::string bundle_path;
	int         member_count = 0;  // number of members embedded
};

// ---------------------------------------------------------------------------
// DemoBundleWriter
// ---------------------------------------------------------------------------
class DemoBundleWriter {
public:
	// Assemble a .demo.X bundle.
	//   demo_dynx_path  - path to the already-produced .demo.dynx file
	//   vsim_path       - path to the originating .vsim script (may be "")
	//   out_bundle_path - output .X file path
	//   sample_result   - metadata from DemoFrameSampler::sample()
	//   cfg             - ExportDemoSection for flags
	//   case_id         - logical case identifier (used in manifest)
	static DemoBundleResult write(
		const std::string&        demo_dynx_path,
		const std::string&        vsim_path,
		const std::string&        out_bundle_path,
		const DemoSampleResult&   sample_result,
		const ExportDemoSection&  cfg,
		const std::string&        case_id = "case");

private:
	static std::string build_manifest_json(
		const DemoSampleResult&  sample_result,
		const ExportDemoSection& cfg,
		const std::string&       case_id);
};

} // namespace io
} // namespace vsim
