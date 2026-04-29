// src/v4/uff/uff_provenance_writer.cpp
// Formation Engine v4.1.0 -- UFF provenance writer implementation

#include "uff_provenance_writer.hpp"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <stdexcept>

namespace vsepr::uff {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

UFFProvenanceWriter::UFFProvenanceWriter(std::string output_dir)
	: output_dir_(std::move(output_dir))
{
	std::filesystem::create_directories(output_dir_);
}

UFFProvenanceWriter::~UFFProvenanceWriter() {
	flush();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void UFFProvenanceWriter::ensure_autocreate_open_() {
	if (autocreate_csv_.is_open()) return;
	const std::string path = output_dir_ + "/uff_autocreate_log.csv";
	autocreate_csv_.open(path, std::ios::app);
	if (!autocreate_csv_)
		throw std::runtime_error("UFFProvenanceWriter: cannot open " + path);
	if (!autocreate_header_written_) {
		autocreate_csv_ <<
			"timestamp,atom_type,element,coordination,geometry,"
			"r1,theta0,x1,D1,zeta,Z1,confidence,source_id,source_note\n";
		autocreate_header_written_ = true;
	}
}

void UFFProvenanceWriter::ensure_provenance_open_() {
	if (provenance_jsonl_.is_open()) return;
	const std::string path = output_dir_ + "/uff_provenance_log.jsonl";
	provenance_jsonl_.open(path, std::ios::app);
	if (!provenance_jsonl_)
		throw std::runtime_error("UFFProvenanceWriter: cannot open " + path);
}

void UFFProvenanceWriter::ensure_validation_open_() {
	if (validation_csv_.is_open()) return;
	const std::string path = output_dir_ + "/uff_validation_log.csv";
	validation_csv_.open(path, std::ios::app);
	if (!validation_csv_)
		throw std::runtime_error("UFFProvenanceWriter: cannot open " + path);
	if (!validation_header_written_) {
		validation_csv_ <<
			"timestamp,test_id,molecule,status,max_missing_params,notes\n";
		validation_header_written_ = true;
	}
}

std::string UFFProvenanceWriter::timestamp_utc_() {
	const auto now    = std::chrono::system_clock::now();
	const std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::ostringstream oss;
	// Use gmtime for a UTC timestamp; _s suffix on MSVC
#ifdef _WIN32
	std::tm tm_buf{};
	gmtime_s(&tm_buf, &t);
	oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
#else
	std::tm tm_buf{};
	gmtime_r(&t, &tm_buf);
	oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
#endif
	return oss.str();
}

std::string UFFProvenanceWriter::escape_json_string_(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 4);
	for (char c : s) {
		if      (c == '"')  out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else if (c == '\n') out += "\\n";
		else if (c == '\r') out += "\\r";
		else                out += c;
	}
	return out;
}

// ---------------------------------------------------------------------------
// write_generated_record  -> uff_autocreate_log.csv
// ---------------------------------------------------------------------------

void UFFProvenanceWriter::write_generated_record(const UFFEntry& e) {
	std::lock_guard<std::mutex> lk(mutex_);
	ensure_autocreate_open_();
	autocreate_csv_
		<< timestamp_utc_() << ','
		<< e.atom_type << ','
		<< e.element   << ','
		<< e.coordination_number << ','
		<< e.geometry_tag << ','
		<< e.r1     << ',' << e.theta0 << ','
		<< e.x1     << ',' << e.D1     << ','
		<< e.zeta   << ',' << e.Z1     << ','
		<< to_string(e.confidence) << ','
		<< e.source_id   << ','
		<< '"' << e.source_note << '"' << '\n';
}

// ---------------------------------------------------------------------------
// write_provenance_record  -> uff_provenance_log.jsonl
// ---------------------------------------------------------------------------

void UFFProvenanceWriter::write_provenance_record(const UFFEntry& e) {
	std::lock_guard<std::mutex> lk(mutex_);
	ensure_provenance_open_();
	provenance_jsonl_
		<< '{'
		<< "\"timestamp\":\""   << timestamp_utc_()                          << "\","
		<< "\"atom_type\":\""   << escape_json_string_(e.atom_type)          << "\","
		<< "\"element\":\""     << escape_json_string_(e.element)            << "\","
		<< "\"coordination\":"  << e.coordination_number                     << ','
		<< "\"geometry\":\""    << escape_json_string_(e.geometry_tag)       << "\","
		<< "\"r1\":"            << e.r1    << ','
		<< "\"theta0\":"        << e.theta0 << ','
		<< "\"x1\":"            << e.x1    << ','
		<< "\"D1\":"            << e.D1    << ','
		<< "\"zeta\":"          << e.zeta  << ','
		<< "\"Z1\":"            << e.Z1    << ','
		<< "\"Vi\":"            << e.Vi    << ','
		<< "\"Uj\":"            << e.Uj    << ','
		<< "\"Xi\":"            << e.Xi    << ','
		<< "\"Hard\":"          << e.Hard  << ','
		<< "\"Radius\":"        << e.Radius << ','
		<< "\"confidence\":\""  << to_string(e.confidence)                   << "\","
		<< "\"source_id\":\""   << escape_json_string_(e.source_id)          << "\","
		<< "\"source_note\":\"" << escape_json_string_(e.source_note)        << "\""
		<< "}\n";
}

// ---------------------------------------------------------------------------
// write_validation_record  -> uff_validation_log.csv
// ---------------------------------------------------------------------------

void UFFProvenanceWriter::write_validation_record(const SpotCheckResult& r) {
	std::lock_guard<std::mutex> lk(mutex_);
	ensure_validation_open_();
	validation_csv_
		<< timestamp_utc_()             << ','
		<< r.test_id                    << ','
		<< r.molecule                   << ','
		<< (r.passed ? "pass" : "warn") << ','
		<< r.missing_count              << ','
		<< '"' << r.notes << '"'        << '\n';
}

// ---------------------------------------------------------------------------
// flush
// ---------------------------------------------------------------------------

void UFFProvenanceWriter::flush() {
	std::lock_guard<std::mutex> lk(mutex_);
	if (autocreate_csv_.is_open())   autocreate_csv_.flush();
	if (provenance_jsonl_.is_open()) provenance_jsonl_.flush();
	if (validation_csv_.is_open())   validation_csv_.flush();
}

} // namespace vsepr::uff
