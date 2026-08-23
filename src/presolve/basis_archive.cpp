// =============================================================================
// basis_archive.cpp  —  Eigen Basis Serialisation  (WO-XSUITE-02B)
// =============================================================================

#include "vsim/basis_archive.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// Internal: FNV-1a hash over raw double bytes
// ---------------------------------------------------------------------------

static uint64_t fnv1a_doubles(const std::vector<double>& v) {
	uint64_t hash = 14695981039346656037ULL;
	for (double d : v) {
		unsigned char bytes[8];
		std::memcpy(bytes, &d, 8);
		for (int i = 0; i < 8; ++i) {
			hash ^= static_cast<uint64_t>(bytes[i]);
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

static std::string to_hex(uint64_t v) {
	static const char hex[] = "0123456789abcdef";
	std::string s(16, '0');
	for (int i = 15; i >= 0; --i, v >>= 4)
		s[static_cast<size_t>(i)] = hex[v & 0xF];
	return s;
}

// ---------------------------------------------------------------------------
// basis_archive_hash_vec
// ---------------------------------------------------------------------------

std::string basis_archive_hash_vec(const std::vector<double>& v) {
	return to_hex(fnv1a_doubles(v));
}

// ---------------------------------------------------------------------------
// NDJSON mode archive
// ---------------------------------------------------------------------------

// Minimal JSON escaping for string values (no control chars expected in paths)
static std::string json_escape(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 2);
	for (char c : s) {
		if (c == '"')  { out += "\\\""; }
		else if (c == '\\') { out += "\\\\"; }
		else out += c;
	}
	return out;
}

void basis_archive_append_mode(const ModeRecord& m, const std::string& path) {
	std::ofstream f(path, std::ios::app);
	if (!f) return;

	f << "{\"seed\":" << m.seed
	  << ",\"batch\":" << m.batch
	  << ",\"mode_index\":" << m.mode_index
	  << ",\"eigenvalue\":" << m.eigenvalue
	  << ",\"recurrence\":" << m.recurrence
	  << ",\"stability\":" << m.stability
	  << ",\"coupling\":" << m.coupling
	  << ",\"recon_error\":" << m.recon_error
	  << ",\"utility\":" << m.utility
	  << ",\"basis_hash\":\"" << json_escape(m.basis_hash) << "\"";

	if (!m.eigenvector.empty()) {
		f << ",\"eigenvector\":[";
		for (size_t i = 0; i < m.eigenvector.size(); ++i) {
			if (i) f << ",";
			f << m.eigenvector[i];
		}
		f << "]";
	}
	f << "}\n";
}

std::vector<ModeRecord> basis_archive_read_modes(const std::string& path) {
	std::vector<ModeRecord> modes;
	std::ifstream f(path);
	if (!f) return modes;

	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] != '{') continue;
		// Minimal field extraction — sufficient for the test suite
		ModeRecord m;
		auto read_int64 = [&](const std::string& key) -> int64_t {
			auto pos = line.find("\"" + key + "\":");
			if (pos == std::string::npos) return 0;
			pos += key.size() + 3;
			return std::stoll(line.substr(pos));
		};
		auto read_double = [&](const std::string& key) -> double {
			auto pos = line.find("\"" + key + "\":");
			if (pos == std::string::npos) return 0.0;
			pos += key.size() + 3;
			return std::stod(line.substr(pos));
		};
		auto read_str = [&](const std::string& key) -> std::string {
			auto pos = line.find("\"" + key + "\":\"");
			if (pos == std::string::npos) return {};
			pos += key.size() + 4;
			auto end = line.find('"', pos);
			if (end == std::string::npos) return {};
			return line.substr(pos, end - pos);
		};

		m.seed        = read_int64("seed");
		m.batch       = static_cast<int>(read_int64("batch"));
		m.mode_index  = static_cast<int>(read_int64("mode_index"));
		m.eigenvalue  = read_double("eigenvalue");
		m.recurrence  = read_double("recurrence");
		m.stability   = read_double("stability");
		m.coupling    = read_double("coupling");
		m.recon_error = read_double("recon_error");
		m.utility     = read_double("utility");
		m.basis_hash  = read_str("basis_hash");

		// Parse eigenvector array
		auto ev_pos = line.find("\"eigenvector\":[");
		if (ev_pos != std::string::npos) {
			ev_pos += 15;
			auto ev_end = line.find(']', ev_pos);
			if (ev_end != std::string::npos) {
				std::string ev_str = line.substr(ev_pos, ev_end - ev_pos);
				std::istringstream ss(ev_str);
				std::string tok;
				while (std::getline(ss, tok, ','))
					if (!tok.empty())
						m.eigenvector.push_back(std::stod(tok));
			}
		}

		modes.push_back(std::move(m));
	}
	return modes;
}

// ---------------------------------------------------------------------------
// Binary eigenvector basis
// ---------------------------------------------------------------------------

bool basis_archive_write_bin(
	const std::vector<ModeRecord>& modes,
	const std::string&             path)
{
	if (modes.empty()) return false;
	size_t n_dims = modes[0].eigenvector.size();
	for (const auto& m : modes)
		if (m.eigenvector.size() != n_dims) return false;

	std::ofstream f(path, std::ios::binary);
	if (!f) return false;

	BasisFileHeader hdr;
	hdr.n_modes = static_cast<uint64_t>(modes.size());
	hdr.n_dims  = static_cast<uint64_t>(n_dims);
	f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

	for (const auto& m : modes) {
		for (double d : m.eigenvector) {
			float fv = static_cast<float>(d);
			f.write(reinterpret_cast<const char*>(&fv), sizeof(float));
		}
	}
	return f.good();
}

bool basis_archive_read_bin(
	const std::string& path,
	BasisFileHeader&   header_out,
	std::vector<std::vector<float>>& vecs_out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;

	f.read(reinterpret_cast<char*>(&header_out), sizeof(BasisFileHeader));
	if (!f) return false;

	if (std::strncmp(header_out.magic, "EMBS", 4) != 0) return false;
	if (header_out.version != 1) return false;

	vecs_out.resize(static_cast<size_t>(header_out.n_modes));
	for (auto& vec : vecs_out) {
		vec.resize(static_cast<size_t>(header_out.n_dims));
		f.read(reinterpret_cast<char*>(vec.data()),
			   static_cast<std::streamsize>(header_out.n_dims * sizeof(float)));
		if (!f) return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// TSV summary
// ---------------------------------------------------------------------------

void basis_archive_write_tsv(
	const std::vector<ModeRecord>& modes,
	const std::string&             path)
{
	std::ofstream f(path);
	if (!f) return;

	f << "seed\tbatch\tmode_index\teigenvalue\trecurrence\t"
		 "stability\tcoupling\trecon_error\tutility\tbasis_hash\n";
	for (const auto& m : modes) {
		f << m.seed << "\t"
		  << m.batch << "\t"
		  << m.mode_index << "\t"
		  << m.eigenvalue << "\t"
		  << m.recurrence << "\t"
		  << m.stability << "\t"
		  << m.coupling << "\t"
		  << m.recon_error << "\t"
		  << m.utility << "\t"
		  << m.basis_hash << "\n";
	}
}

// ---------------------------------------------------------------------------
// Consistency helpers
// ---------------------------------------------------------------------------

bool basis_archive_check_uniform_dims(const std::vector<ModeRecord>& modes) {
	if (modes.empty()) return true;
	size_t n = modes[0].eigenvector.size();
	for (const auto& m : modes)
		if (m.eigenvector.size() != n) return false;
	return true;
}

} // namespace presolve
} // namespace vsepr
