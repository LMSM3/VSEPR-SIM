#pragma once
/**
 * xsim_xport_writer.hpp
 * =====================
 * IWriter         — pluggable export format writer interface.
 * WriterRegistry  — maps format keys to IWriter instances.
 * XYZWriter       — built-in .xyz format writer (reads frames.xyz from run_dir).
 *
 * Adding a new format:
 *   1. Derive from IWriter, implement format_key() and write().
 *   2. Register via WriterRegistry::add() in ExportPackage constructor.
 *
 * namespace xsim::xport
 */

#include <memory>
#include <string>
#include <unordered_map>
#include "xsim_xport_job.hpp"
#include "xsim_xport_result.hpp"

namespace xsim::xport {

// ============================================================================
// IWriter
// ============================================================================

class IWriter {
public:
	virtual ~IWriter() = default;
	[[nodiscard]] virtual std::string format_key() const = 0;
	virtual ExportResult write(const ExportJob& job) = 0;
};

// ============================================================================
// WriterRegistry
// ============================================================================

class WriterRegistry {
public:
	void     add (std::unique_ptr<IWriter> writer);
	IWriter* find(const std::string& key);

private:
	std::unordered_map<std::string, std::unique_ptr<IWriter>> writers_;
};

// ============================================================================
// XYZWriter
// ============================================================================

class XYZWriter final : public IWriter {
public:
	[[nodiscard]] std::string format_key() const override;
	ExportResult write(const ExportJob& job) override;
};

} // namespace xsim::xport
