/**
 * vsim_output_filter.cpp  —  WO-73E / WO-74B
 * ============================================
 * (1) VsimOutputFilter: normalises raw VSIM output records into
 *     EmpiricalDB-compatible rows. Resolves banned field-name aliases
 *     to canonical names (WO-73A). Unchanged from WO-73E.
 *
 * (2) CsvOutputHandler: IOutputFormatHandler strategy "csv".
 *     Self-registers in ModuleRegistry<IOutputFormatHandler>.  Gates on
 *     ExportSection::write_summary_csv.
 *
 * (3) JsonOutputHandler: strategy "json".  Gates on write_analysis_json.
 *
 * (4) XyzOutputHandler: strategy "xyz".  Property-space XYZ (Lambda/E/UFF
 *     as x/y/z).  Gates on write_xyz.
 *
 * (5) XyzFullOutputHandler: strategy "xyzFull".  Extended property-space XYZ
 *     with all NormalisedRecord fields in each atom line.  Gates on
 *     write_xyzfull.
 *
 * Scale-layer gating: all four handlers return scale_layer()="" (all layers).
 * Per-handler scale filtering can be added by overriding scale_layer().
 */

#include "multiscale/vsim_output_filter.hpp"
#include "vsim/export/i_output_format_handler.hpp"
#include "vsim/vsim_document.hpp"
#include "vsim/module_registry.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace vsepr {
namespace multiscale {

// ============================================================================
// Alias resolution helpers
// ============================================================================

double VsimOutputFilter::resolve_double(const VsimRawRecord& raw,
										 const std::string& canonical,
										 const std::vector<std::string>& aliases,
										 double default_val)
{
	// Try canonical name first
	auto it = raw.fields.find(canonical);
	if (it != raw.fields.end()) return it->second;

	// Try each banned alias (WO-73A)
	for (const auto& alias : aliases) {
		it = raw.fields.find(alias);
		if (it != raw.fields.end()) return it->second;
	}
	return default_val;
}

std::string VsimOutputFilter::resolve_string(const VsimRawRecord& raw,
											  const std::string& canonical,
											  const std::vector<std::string>& aliases,
											  const std::string& default_val)
{
	auto it = raw.tags.find(canonical);
	if (it != raw.tags.end()) return it->second;
	for (const auto& alias : aliases) {
		it = raw.tags.find(alias);
		if (it != raw.tags.end()) return it->second;
	}
	return default_val;
}

// ============================================================================
// normalise() — single record
// ============================================================================

NormalisedRecord VsimOutputFilter::normalise(const VsimRawRecord& raw) const
{
	NormalisedRecord rec;
	rec.source_type = "simulation";
	rec.run_id      = raw.run_id;
	rec.object_id   = raw.object_id;
	rec.gap_label   = "pending";

	// Lambda_nm — resolve all WO-73A banned aliases
	rec.Lambda_nm = resolve_double(raw, "Lambda_nm",
		{ "scaleLength", "lengthscale", "lscale", "lambda", "lambda_nm", "ideal_len" });

	// Energy fields
	rec.E_value = resolve_double(raw, "E_value", { "energy", "E", "e_value", "evalue" });
	rec.UFF     = resolve_double(raw, "UFF",     { "uff", "uff_energy", "UFF_energy" });
	rec.CFF     = resolve_double(raw, "CFF",     { "cff", "cff_factor", "CFF_factor" });

	// String tags
	rec.family        = resolve_string(raw, "family",        { "interaction_family", "fam" });
	rec.state         = resolve_string(raw, "state",         { "substance_primary_state", "phase" });
	rec.casting       = resolve_string(raw, "casting",       { "cast", "interaction_type" });
	rec.primary_scale = resolve_string(raw, "primary_scale", { "scale", "scale_tier", "z_scale_1" });

	// Validate: require object_id, run_id, and non-zero Lambda
	rec.valid = (!rec.object_id.empty() &&
				 !rec.run_id.empty() &&
				 rec.Lambda_nm > 0.0);

	return rec;
}

// ============================================================================
// normalise_batch()
// ============================================================================

std::vector<NormalisedRecord> VsimOutputFilter::normalise_batch(
	const std::vector<VsimRawRecord>& records) const
{
	std::vector<NormalisedRecord> out;
	out.reserve(records.size());
	for (const auto& raw : records) {
		auto rec = normalise(raw);
		if (rec.valid) out.push_back(std::move(rec));
	}
	return out;
}

// ============================================================================
// to_csv()
// ============================================================================

std::string VsimOutputFilter::to_csv(const std::vector<NormalisedRecord>& records)
{
	std::ostringstream ss;
	ss << "source_type,object_id,family,state,casting,"
	   << "Lambda_nm,E_value,UFF,CFF,primary_scale,gap_label,run_id\n";

	for (const auto& r : records) {
		ss << r.source_type   << ","
		   << r.object_id     << ","
		   << r.family        << ","
		   << r.state         << ","
		   << r.casting       << ","
		   << std::fixed << std::setprecision(4)
		   << r.Lambda_nm     << ","
		   << r.E_value       << ","
		   << r.UFF           << ","
		   << r.CFF           << ","
		   << r.primary_scale << ","
		   << r.gap_label     << ","
		   << r.run_id        << "\n";
	}
	return ss.str();
}

} // namespace multiscale
} // namespace vsepr

// ============================================================================
// CsvOutputHandler — IOutputFormatHandler strategy "csv"
// ============================================================================
// Wraps VsimOutputFilter::to_csv() for single-record and batch writes.
// Scale-layer gating: handles all layers (scale_layer() returns "").
// File naming: output_dir / "vsim_output.csv" (appends if exists).

class CsvOutputHandler final : public IOutputFormatHandler {
public:
    [[nodiscard]] std::string_view format_key()   const override { return "csv"; }
    [[nodiscard]] std::string_view scale_layer()  const override { return ""; }
    [[nodiscard]] std::string_view description()  const override {
        return "CSV export — EmpiricalDB-compatible rows (WO-73E canonical names)";
    }

    [[nodiscard]] bool handles(const vsim::ExportSection& exp) const override {
        return exp.write_summary_csv;
    }

    void write(const vsepr::multiscale::NormalisedRecord& record,
               const vsim::ExportSection& /*exp*/,
               const fs::path& output_dir) override
    {
        write_batch({ record }, {}, output_dir);
    }

    void write_batch(
        const std::vector<vsepr::multiscale::NormalisedRecord>& records,
        const vsim::ExportSection& /*exp*/,
        const fs::path& output_dir) override
    {
        fs::create_directories(output_dir);
        const fs::path out = output_dir / "vsim_output.csv";
        // Append mode — multiple runs accumulate in one file per session.
        std::ofstream f(out, std::ios::app);
        if (!f.is_open()) return;
        // Write header only when the file is new / empty.
        if (f.tellp() == 0) {
            f << "source_type,object_id,family,state,casting,"
              << "Lambda_nm,E_value,UFF,CFF,primary_scale,gap_label,run_id\n";
        }
        const std::string csv = vsepr::multiscale::VsimOutputFilter::to_csv(records);
        // Skip the header row produced by to_csv() (it starts with "source_type").
        const auto nl = csv.find('\n');
        if (nl != std::string::npos) f << csv.substr(nl + 1);
    }
};

// Self-register "csv" handler at static-init time.
static vsim::AutoRegister<CsvOutputHandler, IOutputFormatHandler> s_csv_reg("csv");

// ============================================================================
// JsonOutputHandler — IOutputFormatHandler strategy "json"  (WO-74B)
// ============================================================================
// Writes each NormalisedRecord as a JSON object inside a JSON array.
// Scale-layer: all layers (scale_layer() returns "").
// Gates on ExportSection::write_analysis_json.
// File: output_dir / "vsim_output.json"  (appended per session).

class JsonOutputHandler final : public IOutputFormatHandler {
public:
    [[nodiscard]] std::string_view format_key()  const override { return "json"; }
    [[nodiscard]] std::string_view scale_layer() const override { return ""; }
    [[nodiscard]] std::string_view description() const override {
        return "JSON export — NormalisedRecord array (WO-74B analysis layer)";
    }

    [[nodiscard]] bool handles(const vsim::ExportSection& exp) const override {
        return exp.write_analysis_json;
    }

    void write(const vsepr::multiscale::NormalisedRecord& record,
               const vsim::ExportSection& exp,
               const fs::path& output_dir) override
    {
        write_batch({ record }, exp, output_dir);
    }

    void write_batch(
        const std::vector<vsepr::multiscale::NormalisedRecord>& records,
        const vsim::ExportSection& /*exp*/,
        const fs::path& output_dir) override
    {
        fs::create_directories(output_dir);
        const fs::path out = output_dir / "vsim_output.json";
        std::ofstream f(out, std::ios::app);
        if (!f.is_open()) return;
        for (const auto& r : records) {
            f << "{"
              << "\"source_type\":\"" << r.source_type   << "\","
              << "\"object_id\":\""   << r.object_id     << "\","
              << "\"family\":\""      << r.family        << "\","
              << "\"state\":\""       << r.state         << "\","
              << "\"casting\":\""     << r.casting       << "\","
              << "\"Lambda_nm\":"     << std::fixed << std::setprecision(4) << r.Lambda_nm << ","
              << "\"E_value\":"       << r.E_value       << ","
              << "\"UFF\":"           << r.UFF            << ","
              << "\"CFF\":"           << r.CFF            << ","
              << "\"primary_scale\":\"" << r.primary_scale << "\","
              << "\"gap_label\":\""   << r.gap_label     << "\","
              << "\"run_id\":\""      << r.run_id        << "\""
              << "}\n";
        }
    }
};

// Self-register "json" handler at static-init time.
static vsim::AutoRegister<JsonOutputHandler, IOutputFormatHandler> s_json_reg("json");

// ============================================================================
// XyzOutputHandler — IOutputFormatHandler strategy "xyz"  (WO-74B)
// ============================================================================
// Writes each NormalisedRecord as one "atom" in an XYZ property-space file
// where x=Lambda_nm, y=E_value, z=UFF.  Each frame is one batch of records.
// Scale-layer: all layers (scale_layer() returns "").
// Gates on ExportSection::write_xyz.
// File: output_dir / "vsim_output.xyz"

class XyzOutputHandler final : public IOutputFormatHandler {
public:
    [[nodiscard]] std::string_view format_key()  const override { return "xyz"; }
    [[nodiscard]] std::string_view scale_layer() const override { return ""; }
    [[nodiscard]] std::string_view description() const override {
        return "XYZ property-space export — Lambda/E/UFF as x/y/z (WO-74B)";
    }

    [[nodiscard]] bool handles(const vsim::ExportSection& exp) const override {
        return exp.write_xyz;
    }

    void write(const vsepr::multiscale::NormalisedRecord& record,
               const vsim::ExportSection& exp,
               const fs::path& output_dir) override
    {
        write_batch({ record }, exp, output_dir);
    }

    void write_batch(
        const std::vector<vsepr::multiscale::NormalisedRecord>& records,
        const vsim::ExportSection& /*exp*/,
        const fs::path& output_dir) override
    {
        if (records.empty()) return;
        fs::create_directories(output_dir);
        const fs::path out = output_dir / "vsim_output.xyz";
        std::ofstream f(out, std::ios::app);
        if (!f.is_open()) return;
        f << records.size() << "\n";
        f << "VSIM property-space frame"
          << " | gap_label=" << records[0].gap_label
          << " | run_id="    << records[0].run_id << "\n";
        for (const auto& r : records) {
            f << r.object_id << "  "
              << std::fixed << std::setprecision(6)
              << r.Lambda_nm << "  "
              << r.E_value   << "  "
              << r.UFF       << "\n";
        }
    }
};

// Self-register "xyz" handler at static-init time.
static vsim::AutoRegister<XyzOutputHandler, IOutputFormatHandler> s_xyz_reg("xyz");

// ============================================================================
// XyzFullOutputHandler — IOutputFormatHandler strategy "xyzFull"  (WO-74B)
// ============================================================================
// Extended XYZ property-space: x=Lambda_nm, y=E_value, z=UFF.
// Comment line carries all NormalisedRecord fields (xyzFull doctrine).
// Scale-layer: all layers.  Gates on ExportSection::write_xyzfull.
// File: output_dir / "vsim_output.xyzFull"

class XyzFullOutputHandler final : public IOutputFormatHandler {
public:
    [[nodiscard]] std::string_view format_key()  const override { return "xyzFull"; }
    [[nodiscard]] std::string_view scale_layer() const override { return ""; }
    [[nodiscard]] std::string_view description() const override {
        return "xyzFull extended property-space export — all NormalisedRecord fields (WO-74B)";
    }

    [[nodiscard]] bool handles(const vsim::ExportSection& exp) const override {
        return exp.write_xyzfull;
    }

    void write(const vsepr::multiscale::NormalisedRecord& record,
               const vsim::ExportSection& exp,
               const fs::path& output_dir) override
    {
        write_batch({ record }, exp, output_dir);
    }

    void write_batch(
        const std::vector<vsepr::multiscale::NormalisedRecord>& records,
        const vsim::ExportSection& /*exp*/,
        const fs::path& output_dir) override
    {
        if (records.empty()) return;
        fs::create_directories(output_dir);
        const fs::path out = output_dir / "vsim_output.xyzFull";
        std::ofstream f(out, std::ios::app);
        if (!f.is_open()) return;
        f << records.size() << "\n";
        // Extended comment line with all fields (xyzFull doctrine)
        f << "properties=object_id:Lambda_nm:E_value:UFF:CFF:primary_scale:gap_label:run_id"
          << " source_type=" << records[0].source_type
          << " run_id="      << records[0].run_id << "\n";
        for (const auto& r : records) {
            f << r.object_id << "  "
              << std::fixed << std::setprecision(6)
              << r.Lambda_nm     << "  "
              << r.E_value       << "  "
              << r.UFF           << "  "
              << r.CFF           << "  "
              << r.primary_scale << "  "
              << r.gap_label     << "  "
              << r.family        << "  "
              << r.state         << "  "
              << r.casting       << "\n";
        }
    }
};

// Self-register "xyzFull" handler at static-init time.
static vsim::AutoRegister<XyzFullOutputHandler, IOutputFormatHandler> s_xyzfull_reg("xyzFull");
