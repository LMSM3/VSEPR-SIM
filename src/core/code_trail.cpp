/**
 * code_trail.cpp  —  Code Trail Wind v0.1  (implementation)
 * ==========================================================
 * VSEPR-SIM 3.0.1
 *
 * CSV serialization, formula notation, stats aggregation,
 * and TrailWriter streaming implementation.
 */

#include "core/code_trail.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>

namespace vsepr {
namespace trail {

// ============================================================================
// Internal helpers
// ============================================================================

// Escape a field for CSV: wrap in quotes and escape internal quotes
std::string CodeTrail::escape_csv(const std::string& s) {
    // If the field contains a comma, newline, or quote, wrap in quotes
    bool needs_quote = (s.find(',')  != std::string::npos ||
                        s.find('"')  != std::string::npos ||
                        s.find('\n') != std::string::npos);
    if (!needs_quote) return s;

    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"') out += '"';   // double-up internal quotes
        out += c;
    }
    out += '"';
    return out;
}

// Format a double: NaN → "", otherwise full precision
std::string CodeTrail::format_double(double v) {
    if (std::isnan(v)) return "";
    std::ostringstream oss;
    oss << std::setprecision(17) << v;
    return oss.str();
}

// ============================================================================
// CSV header
// ============================================================================

std::string CodeTrail::csv_header() {
    return "step,op_kind,op_label,lhs,rhs,result,accumulator,"
           "formula_notation,unit_label,source_tag\n";
}

// ============================================================================
// Entry → CSV row
// ============================================================================

std::string CodeTrail::entry_to_csv_row(const TrailEntry& e) {
    std::string row;
    row.reserve(256);

    row += std::to_string(e.step);
    row += ',';
    row += op_kind_str(e.kind);
    row += ',';
    row += escape_csv(e.op_label);
    row += ',';
    row += format_double(e.lhs);
    row += ',';
    row += format_double(e.rhs);
    row += ',';
    row += format_double(e.result);
    row += ',';
    row += format_double(e.accumulator);
    row += ',';
    row += escape_csv(e.formula_notation);
    row += ',';
    row += escape_csv(e.unit_label);
    row += ',';
    row += escape_csv(e.source_tag);
    row += '\n';

    return row;
}

// ============================================================================
// CodeTrail::stats()
// ============================================================================

TrailStats CodeTrail::stats() const {
    TrailStats s;
    s.trail_name = name_;
    s.total_steps = static_cast<uint64_t>(entries_.size());
    for (const auto& e : entries_) {
        switch (e.kind) {
            case OpKind::BINARY:  s.binary_ops++;  break;
            case OpKind::UNARY:   s.unary_ops++;   break;
            case OpKind::ASSIGN:  s.assign_ops++;  break;
            case OpKind::COMPARE: s.compare_ops++; break;
            case OpKind::CUSTOM:  s.custom_ops++;  break;
        }
    }
    if (!entries_.empty()) {
        s.final_accumulator = entries_.back().accumulator;
    }
    return s;
}

// ============================================================================
// CodeTrail::to_csv_string()
// ============================================================================

std::string CodeTrail::to_csv_string() const {
    std::string out;
    out.reserve(entries_.size() * 200 + 512);

    // File-level preamble comment block (# lines — ignored by spreadsheets)
    out += "# Code Trail Wind v";
    out += CODE_TRAIL_VERSION;
    out += "\n";
    out += "# trail_name: ";
    out += name_;
    out += "\n";
    out += "# total_steps: ";
    out += std::to_string(entries_.size());
    out += "\n";

    TrailStats st = stats();
    out += "# ops_binary: ";  out += std::to_string(st.binary_ops);  out += "\n";
    out += "# ops_unary: ";   out += std::to_string(st.unary_ops);   out += "\n";
    out += "# ops_assign: ";  out += std::to_string(st.assign_ops);  out += "\n";
    out += "# ops_compare: "; out += std::to_string(st.compare_ops); out += "\n";
    out += "# ops_custom: ";  out += std::to_string(st.custom_ops);  out += "\n";
    out += "# final_accumulator: ";
    out += CodeTrail::format_double(st.final_accumulator);
    out += "\n";
    out += "#\n";

    // Column headers
    out += csv_header();

    // Data rows
    for (const auto& e : entries_) {
        out += entry_to_csv_row(e);
    }

    return out;
}

// ============================================================================
// CodeTrail::flush_csv()
// ============================================================================

bool CodeTrail::flush_csv(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    std::string content = to_csv_string();
    f << content;
    return f.good();
}

// ============================================================================
// TrailWriter  —  streaming (low-memory) CSV writer
// ============================================================================

TrailWriter::TrailWriter(const std::string& filepath,
                         const std::string& trail_name)
{
    out_.open(filepath);
    if (!out_.is_open()) return;

    // Write preamble header
    out_ << "# Code Trail Wind v" << CODE_TRAIL_VERSION << "\n";
    out_ << "# trail_name: " << trail_name << "\n";
    out_ << "# mode: streaming\n";
    out_ << "#\n";
    out_ << CodeTrail::csv_header();
}

TrailWriter::~TrailWriter() {
    if (out_.is_open()) {
        out_ << "# total_steps_written: " << step_counter_ << "\n";
        out_.close();
    }
}

void TrailWriter::write_row(const TrailEntry& e) {
    if (!out_.is_open()) return;
    out_ << CodeTrail::entry_to_csv_row(e);
}

void TrailWriter::record_binary(
    const std::string& op_label,
    double lhs, double rhs, double result, double accumulator,
    const std::string& formula_notation,
    const std::string& unit_label,
    const std::string& source_tag)
{
    TrailEntry e;
    e.step             = step_counter_++;
    e.op_label         = op_label;
    e.kind             = OpKind::BINARY;
    e.lhs              = lhs;
    e.rhs              = rhs;
    e.result           = result;
    e.accumulator      = accumulator;
    e.formula_notation = formula_notation;
    e.unit_label       = unit_label;
    e.source_tag       = source_tag;
    write_row(e);
}

void TrailWriter::record_unary(
    const std::string& op_label,
    double input, double result, double accumulator,
    const std::string& formula_notation,
    const std::string& unit_label,
    const std::string& source_tag)
{
    TrailEntry e;
    e.step             = step_counter_++;
    e.op_label         = op_label;
    e.kind             = OpKind::UNARY;
    e.lhs              = input;
    e.rhs              = std::numeric_limits<double>::quiet_NaN();
    e.result           = result;
    e.accumulator      = accumulator;
    e.formula_notation = formula_notation;
    e.unit_label       = unit_label;
    e.source_tag       = source_tag;
    write_row(e);
}

void TrailWriter::record_assign(
    const std::string& var_name,
    double value, double accumulator,
    const std::string& formula_notation,
    const std::string& unit_label,
    const std::string& source_tag)
{
    TrailEntry e;
    e.step             = step_counter_++;
    e.op_label         = "assign:" + var_name;
    e.kind             = OpKind::ASSIGN;
    e.lhs              = value;
    e.rhs              = std::numeric_limits<double>::quiet_NaN();
    e.result           = value;
    e.accumulator      = accumulator;
    e.formula_notation = formula_notation.empty()
                         ? (var_name + " = " + std::to_string(value))
                         : formula_notation;
    e.unit_label       = unit_label;
    e.source_tag       = source_tag;
    write_row(e);
}

} // namespace trail
} // namespace vsepr
