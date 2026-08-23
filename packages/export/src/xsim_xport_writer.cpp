/**
 * src/xsim_xport_writer.cpp
 * =========================
 * WriterRegistry — pluggable writer registration and lookup.
 * XYZWriter      — writes particle positions to .xyz format.
 *
 * XYZWriter behavior (Option A — run_dir is simulation output directory):
 *   1. Looks for frames.xyz in job.run_dir.
 *   2. If found, copies it to output_dir/trajectory.xyz.
 *   3. If not found, creates an empty trajectory.xyz so downstream tools
 *      detect the file exists without crashing.
 *
 * Real frame I/O (stride, split-file, binary cache) is wired
 * once the simulation output contract is formalized.
 */

#include "xsim/xport/xsim_xport_writer.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace xsim::xport {

// ============================================================================
// WriterRegistry
// ============================================================================

void WriterRegistry::add(std::unique_ptr<IWriter> writer) {
	const std::string key = writer->format_key();
	writers_[key] = std::move(writer);
}

IWriter* WriterRegistry::find(const std::string& key) {
	const auto it = writers_.find(key);
	return (it != writers_.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// XYZWriter
// ============================================================================

std::string XYZWriter::format_key() const { return "xyz"; }

ExportResult XYZWriter::write(const ExportJob& job) {
	ExportResult result;

	// Resolve output directory: job.output_dir > job.config.output_dir > "."
	const fs::path out_dir = !job.output_dir.empty()       ? job.output_dir
						   : !job.config.output_dir.empty() ? job.config.output_dir
						   : fs::path(".");
	const fs::path out_path = out_dir / "trajectory.xyz";

	std::error_code ec;
	fs::create_directories(out_dir, ec);
	if (ec) {
		result.errors.push_back("XYZWriter: cannot create output dir: " + ec.message());
		return result;
	}

	// Option A: read frames.xyz from run_dir if it exists.
	const fs::path src_xyz = job.run_dir / "frames.xyz";
	if (!job.run_dir.empty() && fs::exists(src_xyz)) {
		fs::copy_file(src_xyz, out_path, fs::copy_options::overwrite_existing, ec);
		if (ec) {
			result.errors.push_back("XYZWriter: copy failed: " + ec.message());
			return result;
		}
	} else {
		// No source file — create an empty placeholder.
		std::ofstream f(out_path, std::ios::out | std::ios::trunc);
		if (!f.is_open()) {
			result.errors.push_back("XYZWriter: cannot create: " + out_path.string());
			return result;
		}
	}

	result.written_files.push_back(out_path);
	return result;
}

} // namespace xsim::xport
