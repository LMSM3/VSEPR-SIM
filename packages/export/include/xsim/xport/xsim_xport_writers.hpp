#pragma once
/**
 * xsim_xport_writers.hpp
 * ======================
 * Declarations for the eight built-in export writers.
 *
 * Copy writers (read from run_dir, write to output_dir):
 *   AnalysisJsonWriter       → analysis.json
 *   MetricsTsvWriter         → metrics.tsv
 *   EventsJsonWriter         → events.json
 *   SymbolicTraceJsonWriter  → symbolic_trace.json
 *
 * Generate writers (produce content from job fields / filesystem state):
 *   ReportMdWriter           → report.md
 *   VerifyReportWriter       → verify_report.md   (runs after data writers)
 *   VerifyTsvWriter          → verify.tsv          (runs after data writers)
 *   ManifestJsonWriter       → manifest.json       (runs last — scans output_dir)
 *
 * All derive from IWriter and are registered in ExportPackage on construction.
 * Add to ExportPackage::ExportPackage() in xsim_export.cpp.
 *
 * namespace xsim::xport
 */

#include "xsim_xport_writer.hpp"  // IWriter, ExportJob, ExportResult

namespace xsim::xport {

// ============================================================================
// Copy writers — transfer files from run_dir to output_dir
// ============================================================================

class AnalysisJsonWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

class MetricsTsvWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

class EventsJsonWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

class SymbolicTraceJsonWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

// ============================================================================
// Generate writers — produce structured content
// ============================================================================

class ReportMdWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

// Runs after all data writers. Checks expected files against config flags.
class VerifyReportWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

// Runs after all data writers. TSV form of the verification summary.
class VerifyTsvWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

// Runs last. Scans output_dir filesystem to produce the artifact inventory.
class ManifestJsonWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

} // namespace xsim::xport
