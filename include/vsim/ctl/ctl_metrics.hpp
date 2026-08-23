#pragma once
/**
 * ctl_metrics.hpp  —  VSIM-CTL metrics store
 * ============================================
 * WO-VSIM-CTL-06 (partial)  |  v5.1.x
 *
 * MetricsStore holds observable quantities produced by a runtime run.
 * All fields are unreadable until capture() or run_case() has been called.
 *
 * Access guards:
 *   - read any metric before capture() → throws CtlMetricsError
 *   - assert() before capture()        → throws CtlMetricsError
 *   - capture() without a completed run → stores NaN/sentinel values
 *
 * Design: the metrics object is a plain aggregate with a "captured" flag.
 * The runtime wrapper populates it; the script accesses it read-only.
 */

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <vector>

namespace vsim::ctl {

// ============================================================================
// CtlMetricsError
// ============================================================================

struct CtlMetricsError : std::runtime_error {
	explicit CtlMetricsError(const std::string& msg)
		: std::runtime_error("metrics: " + msg) {}
};

// ============================================================================
// MetricsSnapshot — a named observable (scalar)
// ============================================================================

struct MetricsValue {
	double value  = std::numeric_limits<double>::quiet_NaN();
	bool   valid  = false;

	bool   has_nan() const { return std::isnan(value); }
};

// ============================================================================
// MetricsStore
// ============================================================================

class MetricsStore {
public:
	bool captured = false;

	// -----------------------------------------------------------------------
	// Well-known energy sub-store
	struct EnergyMetrics {
		double total_eV    = std::numeric_limits<double>::quiet_NaN();
		double kinetic_eV  = std::numeric_limits<double>::quiet_NaN();
		double potential_eV= std::numeric_limits<double>::quiet_NaN();
		double drift       = std::numeric_limits<double>::quiet_NaN();
		bool   has_nan()   const { return std::isnan(total_eV); }
	};

	// Well-known force sub-store
	struct ForceMetrics {
		double channel_sum_error        = std::numeric_limits<double>::quiet_NaN();
		bool   disabled_channels_zero   = false;
		bool   has_nan()                const { return std::isnan(channel_sum_error); }
	};

	EnergyMetrics energy;
	ForceMetrics  force;

	// Arbitrary named scalars (from metrics.capture overrides)
	std::unordered_map<std::string, MetricsValue> scalars;

	// -----------------------------------------------------------------------
	// Mark as captured (called by runtime wrapper after run_case completes)
	void capture() {
		captured = true;
	}

	// Reset — called before a new run
	void reset() {
		captured = false;
		energy   = {};
		force    = {};
		scalars.clear();
	}

	// -----------------------------------------------------------------------
	// Populate from runtime (typed setters)
	void set_energy_total(double v)     { energy.total_eV     = v; }
	void set_energy_kinetic(double v)   { energy.kinetic_eV   = v; }
	void set_energy_potential(double v) { energy.potential_eV = v; }
	void set_energy_drift(double v)     { energy.drift        = v; }
	void set_force_channel_sum_error(double v) { force.channel_sum_error = v; }
	void set_force_disabled_zero(bool v)       { force.disabled_channels_zero = v; }
	void set_scalar(const std::string& name, double v) {
		scalars[name] = { v, true };
	}

	// -----------------------------------------------------------------------
	// Guarded read — throws if not captured
	const EnergyMetrics& read_energy() const {
		require_captured("energy");
		return energy;
	}

	const ForceMetrics& read_force() const {
		require_captured("force");
		return force;
	}

	double read_scalar(const std::string& name) const {
		require_captured(name);
		auto it = scalars.find(name);
		if (it == scalars.end())
			throw CtlMetricsError("unknown scalar '" + name + "'");
		return it->second.value;
	}

	// -----------------------------------------------------------------------
	// assert(expression) — simple named-field evaluator
	// expression form: "field.subfield op value"
	//   e.g. "energy.drift < 1e-6"
	//        "force.disabled_channels_zero == true"
	//        "energy.has_nan == false"
	struct AssertResult {
		bool        passed = false;
		std::string expression;
		std::string reason;
	};

	AssertResult assert_expr(const std::string& expr) const {
		AssertResult r;
		r.expression = expr;
		if (!captured) {
			r.reason = "metrics not captured";
			return r;
		}

		// Tokenize: field op value
		// Split on whitespace
		std::vector<std::string> tokens;
		{
			std::string tok;
			for (char c : expr) {
				if (c == ' ' || c == '\t') {
					if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
				} else {
					tok += c;
				}
			}
			if (!tok.empty()) tokens.push_back(tok);
		}

		if (tokens.size() < 3) {
			r.reason = "malformed expression";
			return r;
		}

		const std::string& field = tokens[0];
		const std::string& op    = tokens[1];
		const std::string& rhs   = tokens[2];

		// Resolve field to double
		double lhs_d = std::numeric_limits<double>::quiet_NaN();
		bool   lhs_b = false;
		bool   is_bool_field = false;

		if (field == "energy.drift")         { lhs_d = energy.drift; }
		else if (field == "energy.total")    { lhs_d = energy.total_eV; }
		else if (field == "energy.kinetic")  { lhs_d = energy.kinetic_eV; }
		else if (field == "energy.has_nan")  { lhs_b = energy.has_nan(); is_bool_field = true; }
		else if (field == "force.channel_sum_error") { lhs_d = force.channel_sum_error; }
		else if (field == "force.disabled_channels_zero") { lhs_b = force.disabled_channels_zero; is_bool_field = true; }
		else if (field == "force.has_nan")   { lhs_b = force.has_nan(); is_bool_field = true; }
		else {
			auto it = scalars.find(field);
			if (it != scalars.end()) lhs_d = it->second.value;
			else { r.reason = "unknown field '" + field + "'"; return r; }
		}

		// Evaluate
		if (is_bool_field) {
			bool rhs_b = (rhs == "true");
			if (op == "==")       r.passed = (lhs_b == rhs_b);
			else if (op == "!=")  r.passed = (lhs_b != rhs_b);
			else { r.reason = "unsupported bool operator '" + op + "'"; return r; }
		} else {
			double rhs_d = std::stod(rhs);
			if (op == "<")        r.passed = (lhs_d < rhs_d);
			else if (op == "<=")  r.passed = (lhs_d <= rhs_d);
			else if (op == ">")   r.passed = (lhs_d > rhs_d);
			else if (op == ">=")  r.passed = (lhs_d >= rhs_d);
			else if (op == "==")  r.passed = (lhs_d == rhs_d);
			else if (op == "!=")  r.passed = (lhs_d != rhs_d);
			else { r.reason = "unsupported numeric operator '" + op + "'"; return r; }
			if (!r.passed)
				r.reason = "got " + std::to_string(lhs_d);
		}
		return r;
	}

private:
	void require_captured(const std::string& field) const {
		if (!captured)
			throw CtlMetricsError(
				"'" + field + "' accessed before metrics.capture() or runtime.run_case()");
	}
};

} // namespace vsim::ctl
