#pragma once
/**
 * include/vsim/io/dynx_reader.hpp
 * =================================
 * WO-72D  |  Phase 9  |  v5.1.x
 *
 * DynxReader — reads a .dynx v1 archive back into DynxRichFrame objects
 * without corrupting frame boundaries.
 *
 * Usage:
 *   DynxReader rd;
 *   if (!rd.open("run.dynx")) { // error }
 *   DynxRichFrame frame;
 *   while (rd.read_next_frame(frame)) {
 *       // process frame …
 *   }
 *   rd.close();
 *
 * Notes:
 *   - Unknown line prefixes inside a frame are silently skipped
 *     (forward-compatible with future extensions).
 *   - Particle records are identified by NOT matching any known rich-line
 *     prefix.  Each such record is parsed as:
 *       <symbol_or_idx> <x> <y> <z> [<vx> <vy> <vz>] [<energy>]
 *     A leading integer is treated as a positional index written by the
 *     streaming API; a leading alphabetic token is a symbol.
 *   - The header is available after open() via header().
 *
 * Group 72 — DynxReader / round-trip
 */

#include "vsim/io/dynx_writer.hpp"   // reuses DynxRichFrame and related structs

#include <cstdio>
#include <string>
#include <vector>

namespace vsim {
namespace io {

// ============================================================================
// DynxReader
// ============================================================================

class DynxReader {
public:
	DynxReader()  = default;
	~DynxReader() { close(); }

	DynxReader(const DynxReader&)            = delete;
	DynxReader& operator=(const DynxReader&) = delete;
	DynxReader(DynxReader&&)                 = default;
	DynxReader& operator=(DynxReader&&)      = default;

	// Open a .dynx file.  Parses the header block; does not load frames yet.
	// Returns false on I/O failure or if the file lacks a valid #dynx header.
	bool open(const std::string& path);

	// Read and populate the next frame.  Returns false when EOF is reached or
	// on a parse error (check error() for details).
	bool read_next_frame(DynxRichFrame& out);

	// Read ALL remaining frames into a vector.  Returns false if zero frames
	// could be read.
	bool read_all_frames(std::vector<DynxRichFrame>& out);

	// Close the underlying file handle.
	void close();

	bool        is_open()     const { return fp_ != nullptr; }
	int         frames_read() const { return frames_read_; }
	const std::string& path()  const { return path_; }
	const std::string& error() const { return error_; }
	const DynxHeader&  header() const { return hdr_; }

private:
	FILE*       fp_          = nullptr;
	int         frames_read_ = 0;
	std::string path_;
	std::string error_;
	DynxHeader  hdr_;

	// Parse one line that was found inside a FRAME block.
	// Returns true if the line was consumed as a known record type.
	bool parse_in_frame_line(const std::string& s, DynxRichFrame& frame);
};

} // namespace io
} // namespace vsim
