#pragma once
/**
 * code_trail.hpp  —  Code Trail Wind v0.1
 * ========================================
 * VSEPR-SIM 3.0.1
 *
 * Records every numerical operation taken during a calculation as a
 * structured, deterministic audit trail.  Each step captures:
 *
 *   - step index        (monotonic, 0-based)
 *   - operation label   (e.g. "add", "multiply", "sqrt", "dot_product")
 *   - left operand      (or single input for unary ops)
 *   - right operand     (unused for unary ops — NaN-flagged)
 *   - result            (output of this operation)
 *   - accumulator       (running total / chain value, caller-managed)
 *   - formula_notation  (human-readable symbolic expression for this step)
 *   - unit_label        (optional physical unit string, e.g. "kcal/mol")
 *   - source_tag        (caller-supplied context, e.g. "LJ pair 3-7")
 *
 * Output:  deterministic CSV, one row per operation.
 * Future:  v0.2 will emit LaTeX formula sheets solvable on paper.
 *
 * Design philosophy
 * -----------------
 * Anti-black-box: every mapping decision must be inspectable and traceable.
 * The trail is append-only during a calculation, then flushed to CSV.
 * No hidden state.  The CSV is self-describing — column headers included.
 *
 * Usage (basic)
 * -------------
 *   CodeTrail trail("energy_calc");
 *   trail.record_binary("multiply", mass, accel, mass * accel,
 *                        mass * accel, "F = m * a", "kcal/mol/A", "Newton");
 *   trail.record_binary("add", F_bond, F_nonbond, F_bond + F_nonbond,
 *                        F_bond + F_nonbond, "F_total = F_bond + F_nonbond",
 *                        "kcal/mol/A", "sum forces");
 *   trail.flush_csv("output/energy_calc_trail.csv");
 *
 * Usage (scoped via TrailScope)
 * -----------------------------
 *   {
 *       TrailScope scope(trail, "LJ pair 3-7");
 *       // operations recorded inside this scope carry the tag "LJ pair 3-7"
 *   }
 *
 * Versioning
 * ----------
 *   v0.1  —  CSV output, operation trail, formula_notation as plain text
 *   v0.2  —  LaTeX formula block export (planned)
 *   v0.3  —  Paper-printable formula sheet with solution steps (planned)
 */

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace vsepr {
namespace trail {

// ============================================================================
// Version stamp
// ============================================================================

constexpr int CODE_TRAIL_MAJOR = 0;
constexpr int CODE_TRAIL_MINOR = 1;
constexpr const char* CODE_TRAIL_VERSION = "0.1";

// ============================================================================
// Operation classification
// ============================================================================

enum class OpKind : uint8_t {
    BINARY   = 0,   // two operands  -> result
    UNARY    = 1,   // one operand   -> result
    ASSIGN   = 2,   // named value assignment (no arithmetic)
    COMPARE  = 3,   // comparison producing a boolean (stored as 1.0/0.0)
    CUSTOM   = 4    // caller-defined — formula_notation carries full meaning
};

inline const char* op_kind_str(OpKind k) {
    switch (k) {
        case OpKind::BINARY:  return "binary";
        case OpKind::UNARY:   return "unary";
        case OpKind::ASSIGN:  return "assign";
        case OpKind::COMPARE: return "compare";
        case OpKind::CUSTOM:  return "custom";
    }
    return "unknown";
}

// ============================================================================
// TrailEntry  —  one recorded operation step
// ============================================================================

struct TrailEntry {
    uint64_t    step;               // monotonic index within this trail
    std::string op_label;           // short operation name ("add", "sqrt", ...)
    OpKind      kind;               // classification
    double      lhs;                // left operand  (or single input for unary)
    double      rhs;                // right operand (NaN if unary/assign)
    double      result;             // computed result of this step
    double      accumulator;        // caller-managed running value
    std::string formula_notation;   // human-readable symbolic expression
    std::string unit_label;         // physical unit (empty if dimensionless)
    std::string source_tag;         // caller context ("LJ pair 3-7", etc.)

    // Convenience: is this a unary/assign op?
    bool is_unary() const { return std::isnan(rhs); }
};

// ============================================================================
// TrailStats  —  aggregate summary emitted at end of CSV
// ============================================================================

struct TrailStats {
    uint64_t total_steps      = 0;
    uint64_t binary_ops       = 0;
    uint64_t unary_ops        = 0;
    uint64_t assign_ops       = 0;
    uint64_t compare_ops      = 0;
    uint64_t custom_ops       = 0;
    double   final_accumulator = 0.0;
    std::string trail_name;
};

// ============================================================================
// CodeTrail  —  append-only recorder for a single calculation
// ============================================================================

class CodeTrail {
public:
    // Construct a named trail.  name is embedded in the CSV header.
    explicit CodeTrail(const std::string& name = "unnamed_trail")
        : name_(name), step_counter_(0) {}

    // ── Recording API ──────────────────────────────────────────────────────

    // Record a binary operation:  result = lhs  <op>  rhs
    void record_binary(
        const std::string& op_label,
        double lhs,
        double rhs,
        double result,
        double accumulator,
        const std::string& formula_notation = "",
        const std::string& unit_label       = "",
        const std::string& source_tag       = "")
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
        e.source_tag       = active_tag_.empty() ? source_tag : active_tag_;
        entries_.push_back(std::move(e));
    }

    // Record a unary operation:  result = <op>(input)
    void record_unary(
        const std::string& op_label,
        double input,
        double result,
        double accumulator,
        const std::string& formula_notation = "",
        const std::string& unit_label       = "",
        const std::string& source_tag       = "")
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
        e.source_tag       = active_tag_.empty() ? source_tag : active_tag_;
        entries_.push_back(std::move(e));
    }

    // Record a named value assignment (no arithmetic, just bookkeeping)
    void record_assign(
        const std::string& var_name,
        double value,
        double accumulator,
        const std::string& formula_notation = "",
        const std::string& unit_label       = "",
        const std::string& source_tag       = "")
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
        e.source_tag       = active_tag_.empty() ? source_tag : active_tag_;
        entries_.push_back(std::move(e));
    }

    // Record a comparison (result stored as 1.0 = true, 0.0 = false)
    void record_compare(
        const std::string& op_label,
        double lhs,
        double rhs,
        bool   result,
        double accumulator,
        const std::string& formula_notation = "",
        const std::string& source_tag       = "")
    {
        TrailEntry e;
        e.step             = step_counter_++;
        e.op_label         = op_label;
        e.kind             = OpKind::COMPARE;
        e.lhs              = lhs;
        e.rhs              = rhs;
        e.result           = result ? 1.0 : 0.0;
        e.accumulator      = accumulator;
        e.formula_notation = formula_notation;
        e.unit_label       = "";
        e.source_tag       = active_tag_.empty() ? source_tag : active_tag_;
        entries_.push_back(std::move(e));
    }

    // Record a fully custom step — formula_notation carries all semantics
    void record_custom(
        const std::string& op_label,
        double result,
        double accumulator,
        const std::string& formula_notation,
        const std::string& unit_label = "",
        const std::string& source_tag = "")
    {
        TrailEntry e;
        e.step             = step_counter_++;
        e.op_label         = op_label;
        e.kind             = OpKind::CUSTOM;
        e.lhs              = std::numeric_limits<double>::quiet_NaN();
        e.rhs              = std::numeric_limits<double>::quiet_NaN();
        e.result           = result;
        e.accumulator      = accumulator;
        e.formula_notation = formula_notation;
        e.unit_label       = unit_label;
        e.source_tag       = active_tag_.empty() ? source_tag : active_tag_;
        entries_.push_back(std::move(e));
    }

    // ── Scope tagging (used by TrailScope) ─────────────────────────────────

    void push_tag(const std::string& tag) { active_tag_ = tag; }
    void pop_tag()                         { active_tag_.clear(); }

    // ── Read access ────────────────────────────────────────────────────────

    const std::vector<TrailEntry>& entries() const { return entries_; }
    const std::string& name()                const { return name_; }
    uint64_t           step_count()          const { return step_counter_; }

    // Compute aggregate stats from current entries
    TrailStats stats() const;

    // ── Output ─────────────────────────────────────────────────────────────

    // Flush all entries to a CSV file.
    // Returns true on success.
    bool flush_csv(const std::string& filepath) const;

    // Build the CSV as a string (useful for testing without file I/O)
    std::string to_csv_string() const;

    // Clear all entries and reset step counter (reuse trail object)
    void reset() {
        entries_.clear();
        step_counter_ = 0;
        active_tag_.clear();
    }

    // Public static formatting helpers (also used by TrailWriter)
    static std::string csv_header();
    static std::string entry_to_csv_row(const TrailEntry& e);
    static std::string escape_csv(const std::string& s);
    static std::string format_double(double v);

private:
    std::string              name_;
    uint64_t                 step_counter_;
    std::vector<TrailEntry>  entries_;
    std::string              active_tag_;   // currently active scope tag
};

// ============================================================================
// TrailScope  —  RAII scope that pushes/pops a source_tag on a CodeTrail
// ============================================================================

class TrailScope {
public:
    TrailScope(CodeTrail& trail, const std::string& tag)
        : trail_(trail) {
        trail_.push_tag(tag);
    }
    ~TrailScope() {
        trail_.pop_tag();
    }
    TrailScope(const TrailScope&)            = delete;
    TrailScope& operator=(const TrailScope&) = delete;
private:
    CodeTrail& trail_;
};

// ============================================================================
// TrailWriter  —  convenience wrapper for streaming CSV writes
//                 without holding all entries in memory
// ============================================================================

class TrailWriter {
public:
    explicit TrailWriter(const std::string& filepath, const std::string& trail_name);
    ~TrailWriter();

    TrailWriter(const TrailWriter&)            = delete;
    TrailWriter& operator=(const TrailWriter&) = delete;

    // Same recording surface as CodeTrail but writes directly to file
    void record_binary(
        const std::string& op_label,
        double lhs, double rhs, double result, double accumulator,
        const std::string& formula_notation = "",
        const std::string& unit_label       = "",
        const std::string& source_tag       = "");

    void record_unary(
        const std::string& op_label,
        double input, double result, double accumulator,
        const std::string& formula_notation = "",
        const std::string& unit_label       = "",
        const std::string& source_tag       = "");

    void record_assign(
        const std::string& var_name,
        double value, double accumulator,
        const std::string& formula_notation = "",
        const std::string& unit_label       = "",
        const std::string& source_tag       = "");

    bool is_open() const { return out_.is_open(); }
    uint64_t step_count() const { return step_counter_; }

private:
    std::ofstream out_;
    uint64_t      step_counter_ = 0;
    std::string   active_tag_;

    void write_row(const TrailEntry& e);
};

} // namespace trail
} // namespace vsepr
