/**
 * src/vsim/io/demo_bundle_writer.cpp
 * =====================================
 * WO-OUTPUT-P2-C  |  Output System Expansion Phase 2  |  v5.1.x
 */

#include "vsim/io/demo_bundle_writer.hpp"
#include "xbundle/xbundle_document.hpp"
#include "xbundle/xbundle_writer.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace io {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static std::string read_file_text(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open()) return "";
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// JSON-escape a string: escapes backslash, double-quote, and control chars.
static std::string json_escape(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 4);
	for (unsigned char c : s) {
		if (c == '\\') { out += "\\\\"; }
		else if (c == '"')  { out += "\\\""; }
		else if (c == '\n') { out += "\\n";  }
		else if (c == '\r') { out += "\\r";  }
		else if (c == '\t') { out += "\\t";  }
		else if (c < 0x20)  { out += ' ';    }  // replace other controls
		else                { out += static_cast<char>(c); }
	}
	return out;
}

// ISO-8601 UTC timestamp for the manifest.
static std::string iso8601_now() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t tt = std::chrono::system_clock::to_time_t(now);
	std::tm gmt{};
#ifdef _WIN32
	gmtime_s(&gmt, &tt);
#else
	gmtime_r(&tt, &gmt);
#endif
	std::ostringstream ss;
	ss << std::put_time(&gmt, "%Y-%m-%dT%H:%M:%SZ");
	return ss.str();
}

// ---------------------------------------------------------------------------
// manifest JSON
// ---------------------------------------------------------------------------

std::string DemoBundleWriter::build_manifest_json(
const DemoSampleResult&  sr,
const ExportDemoSection& cfg,
const std::string&       case_id)
{
std::ostringstream j;
j << "{\n";
j << "  \"schema\": \"demo_manifest_v1\",\n";
j << "  \"generated_utc\": \"" << iso8601_now() << "\",\n";
j << "  \"case_id\": \""       << json_escape(case_id)         << "\",\n";
j << "  \"strategy\": \""      << json_escape(cfg.strategy)    << "\",\n";
j << "  \"demo_frames_requested\": " << cfg.demo_frames << ",\n";
j << "  \"frames_in_source\": "  << sr.frames_in  << ",\n";
j << "  \"frames_in_demo\": "    << sr.frames_out << ",\n";
j << "  \"source_hash\": \""     << json_escape(sr.source_hash) << "\",\n";
j << "  \"source_dynx\": \""     << json_escape(sr.output_path) << "\",\n";
j << "  \"frame_indices\": [";
for (std::size_t i = 0; i < sr.frame_indices.size(); ++i) {
if (i) j << ", ";
j << sr.frame_indices[i];
}
j << "]\n";
j << "}\n";
return j.str();
}

// ---------------------------------------------------------------------------
// write()
// ---------------------------------------------------------------------------

DemoBundleResult DemoBundleWriter::write(
	const std::string&        demo_dynx_path,
	const std::string&        vsim_path,
	const std::string&        out_bundle_path,
	const DemoSampleResult&   sample_result,
	const ExportDemoSection&  cfg,
	const std::string&        case_id)
{
	DemoBundleResult result;
	result.bundle_path = out_bundle_path;

	// Read the .demo.dynx content
	std::string dynx_content = read_file_text(demo_dynx_path);
	if (dynx_content.empty()) {
		result.error = "DemoBundleWriter: cannot read demo dynx: " + demo_dynx_path;
		return result;
	}

	xbundle::XBundle bundle;
	bundle.manifest.name        = case_id + ".demo";
	bundle.manifest.description = "Lightweight molecular demonstration — " + case_id;
	bundle.manifest.entry_point  = "main.vsim";

	// Member 1
	{
		xbundle::XBundleEntry e;
		e.name    = "demo.dynx";
		e.kind    = xbundle::XBundleEntryKind::asset;
		e.content = dynx_content;
		bundle.entries.push_back(std::move(e));
		++result.member_count;
	}

	// Member 2: originating .vsim (optional)
	if (cfg.include_source && !vsim_path.empty()) {
		std::string vsim_content = read_file_text(vsim_path);
		if (!vsim_content.empty()) {
			xbundle::XBundleEntry e;
			e.name    = "main.vsim";
			e.kind    = xbundle::XBundleEntryKind::vsim;
			e.content = vsim_content;
			bundle.entries.push_back(std::move(e));
			++result.member_count;
		}
	}

	// Member 3: demo_manifest.json (optional)
	if (cfg.include_manifest) {
		xbundle::XBundleEntry e;
		e.name    = "demo_manifest.json";
		e.kind    = xbundle::XBundleEntryKind::asset;
		e.content = build_manifest_json(sample_result, cfg, case_id);
		bundle.entries.push_back(std::move(e));
		++result.member_count;
	}

	bundle.manifest.populated = true;

	try {
		xbundle::XBundleWriter::write_file(bundle, out_bundle_path);
	} catch (const std::exception& ex) {
		result.error = std::string("DemoBundleWriter: write failed: ") + ex.what();
		return result;
	}

	result.ok = true;
	return result;
}

} // namespace io
} // namespace vsim
