// =============================================================================
// test_basis_archive.cpp  —  WO-XSUITE-02B  basis archive round-trip tests
// =============================================================================

#include "vsim/eigenmine.hpp"
#include "vsim/basis_archive.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void check(bool cond, const char* msg) {
	if (!cond) { std::cerr << "[FAIL] " << msg << "\n"; std::exit(1); }
	std::cout << "[OK]   " << msg << "\n";
}

// ---------------------------------------------------------------------------
// Build a small set of ModeRecords for testing
// ---------------------------------------------------------------------------

static std::vector<vsepr::presolve::ModeRecord> make_modes(int n, int dims) {
	std::vector<vsepr::presolve::ModeRecord> modes;
	for (int i = 0; i < n; ++i) {
		vsepr::presolve::ModeRecord m;
		m.seed        = i * 10;
		m.batch       = i / 5;
		m.mode_index  = i % 3;
		m.eigenvalue  = 1.0 + i * 0.1;
		m.recurrence  = 0.8 - i * 0.01;
		m.stability   = 0.75;
		m.coupling    = 0.5;
		m.recon_error = 0.05 + i * 0.001;
		m.utility     = vsepr::presolve::eigenmine_score_mode(m);
		m.eigenvector.resize(static_cast<size_t>(dims));
		for (int d = 0; d < dims; ++d)
			m.eigenvector[static_cast<size_t>(d)] = static_cast<double>(i * dims + d) * 0.01;
		m.basis_hash = vsepr::presolve::basis_archive_hash_vec(m.eigenvector);
		modes.push_back(std::move(m));
	}
	return modes;
}

// ---------------------------------------------------------------------------
// BA-1: NDJSON round-trip
// ---------------------------------------------------------------------------

static void test_BA1_ndjson_roundtrip() {
	auto modes = make_modes(5, 4);
	fs::path path = fs::temp_directory_path() / "test_ba1_modes.ndjson";
	if (fs::exists(path)) fs::remove(path);

	for (const auto& m : modes)
		vsepr::presolve::basis_archive_append_mode(m, path.string());

	auto read_back = vsepr::presolve::basis_archive_read_modes(path.string());
	check(read_back.size() == modes.size(), "BA-1: read-back count matches");
	for (size_t i = 0; i < modes.size(); ++i) {
		check(read_back[i].seed == modes[i].seed, "BA-1: seed matches");
		check(read_back[i].basis_hash == modes[i].basis_hash, "BA-1: basis_hash matches");
	}
}

// ---------------------------------------------------------------------------
// BA-2: Binary basis round-trip
// ---------------------------------------------------------------------------

static void test_BA2_bin_roundtrip() {
	auto modes = make_modes(4, 6);
	fs::path path = fs::temp_directory_path() / "test_ba2_basis.bin";
	if (fs::exists(path)) fs::remove(path);

	bool wrote = vsepr::presolve::basis_archive_write_bin(modes, path.string());
	check(wrote, "BA-2: write_bin returns true");
	check(fs::exists(path), "BA-2: bin file exists");

	vsepr::presolve::BasisFileHeader hdr;
	std::vector<std::vector<float>> vecs;
	bool read_ok = vsepr::presolve::basis_archive_read_bin(path.string(), hdr, vecs);
	check(read_ok, "BA-2: read_bin returns true");
	check(hdr.n_modes == modes.size(), "BA-2: n_modes matches");
	check(hdr.n_dims  == 6, "BA-2: n_dims = 6");
	check(vecs.size()  == modes.size(), "BA-2: vec count matches");
}

// ---------------------------------------------------------------------------
// BA-3: TSV summary written correctly
// ---------------------------------------------------------------------------

static void test_BA3_tsv() {
	auto modes = make_modes(3, 4);
	fs::path path = fs::temp_directory_path() / "test_ba3_summary.tsv";
	if (fs::exists(path)) fs::remove(path);

	vsepr::presolve::basis_archive_write_tsv(modes, path.string());
	check(fs::exists(path), "BA-3: TSV file exists");

	std::ifstream f(path);
	std::string header;
	std::getline(f, header);
	check(header.find("seed") != std::string::npos, "BA-3: TSV has seed column");
	check(header.find("utility") != std::string::npos, "BA-3: TSV has utility column");
	int rows = 0;
	std::string line;
	while (std::getline(f, line)) ++rows;
	check(rows == 3, "BA-3: TSV has correct data row count");
}

// ---------------------------------------------------------------------------
// BA-4: uniform dimension check
// ---------------------------------------------------------------------------

static void test_BA4_uniform_dims() {
	auto modes = make_modes(4, 5);
	check(vsepr::presolve::basis_archive_check_uniform_dims(modes),
		  "BA-4: uniform dims = true for equal-length vectors");

	modes[2].eigenvector.push_back(0.0); // break uniformity
	check(!vsepr::presolve::basis_archive_check_uniform_dims(modes),
		  "BA-4: uniform dims = false after length mismatch");
}

// ---------------------------------------------------------------------------
// BA-5: hash is deterministic and non-empty
// ---------------------------------------------------------------------------

static void test_BA5_hash() {
	std::vector<double> v = {1.0, 2.0, 3.0, 4.0};
	auto h1 = vsepr::presolve::basis_archive_hash_vec(v);
	auto h2 = vsepr::presolve::basis_archive_hash_vec(v);
	check(!h1.empty(), "BA-5: hash non-empty");
	check(h1 == h2,    "BA-5: hash deterministic");

	v[0] = 9.9;
	auto h3 = vsepr::presolve::basis_archive_hash_vec(v);
	check(h3 != h1, "BA-5: different input -> different hash");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== Basis Archive tests ===\n";
	test_BA1_ndjson_roundtrip();
	test_BA2_bin_roundtrip();
	test_BA3_tsv();
	test_BA4_uniform_dims();
	test_BA5_hash();
	std::cout << "All basis archive tests passed.\n";
	return 0;
}
