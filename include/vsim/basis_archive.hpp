#pragma once
// =============================================================================
// basis_archive.hpp  —  Eigen Basis Serialisation Layer  (WO-XSUITE-02B)
// =============================================================================
//
// Handles read/write of:
//   eigenmine_modes.ndjson  — one JSON line per ModeRecord
//   eigenmine_basis.bin     — binary packed float32 eigenvectors
//   eigenmine_summary.tsv   — seed-indexed metrics table
//
// Design:
//   - NDJSON is forward-compatible: unknown fields are silently skipped
//   - Binary format: 4-byte magic "EMBS", 4-byte version=1, 8-byte n_modes,
//     8-byte n_dims, then n_modes × n_dims float32 values
//   - TSV: header row + one data row per retained mode
//
// =============================================================================

#include "eigenmine.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <istream>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// NDJSON mode archive
// ---------------------------------------------------------------------------

// Append one ModeRecord as a JSON line to the ndjson file at path.
// File is created if it does not exist.
void basis_archive_append_mode(
	const ModeRecord&  mode,
	const std::string& ndjson_path);

// Read all ModeRecords from an ndjson file.
std::vector<ModeRecord> basis_archive_read_modes(
	const std::string& ndjson_path);

// ---------------------------------------------------------------------------
// Binary eigenvector basis
// ---------------------------------------------------------------------------

struct BasisFileHeader {
	char     magic[4]  = {'E','M','B','S'};
	uint32_t version   = 1;
	uint64_t n_modes   = 0;
	uint64_t n_dims    = 0;
};

// Write all retained modes' eigenvectors to a binary file.
// Eigenvectors must all have the same dimensionality.
bool basis_archive_write_bin(
	const std::vector<ModeRecord>& modes,
	const std::string&             bin_path);

// Read eigenvectors from a binary file.
bool basis_archive_read_bin(
	const std::string&        bin_path,
	BasisFileHeader&          header_out,
	std::vector<std::vector<float>>& vecs_out);

// ---------------------------------------------------------------------------
// TSV summary
// ---------------------------------------------------------------------------

// Write a TSV summary of all retained modes.
// Columns: seed, batch, mode_index, eigenvalue, recurrence, stability,
//          coupling, recon_error, utility, basis_hash
void basis_archive_write_tsv(
	const std::vector<ModeRecord>& modes,
	const std::string&             tsv_path);

// ---------------------------------------------------------------------------
// Consistency helpers
// ---------------------------------------------------------------------------

// Returns true if all modes in the vector have identical eigenvector length.
bool basis_archive_check_uniform_dims(const std::vector<ModeRecord>& modes);

// Hash a float vector to a short hex string (FNV-1a over raw bytes).
std::string basis_archive_hash_vec(const std::vector<double>& v);

} // namespace presolve
} // namespace vsepr
