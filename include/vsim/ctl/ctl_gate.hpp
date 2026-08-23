#pragma once
/**
 * ctl_gate.hpp  —  VSIM-CTL gate validation state
 * ================================================
 * WO-VSIM-CTL-06 (partial)  |  v5.1.x
 *
 * GateState tracks one validation gate: begin → assert* → end.
 *
 * Rules:
 *   - gate.begin("name")          : opens the gate, resets state
 *   - gate.assert(expression)     : evaluates a MetricsStore expression
 *   - gate.pass() / gate.fail()   : explicit override
 *   - gate.end()                  : closes and finalises the gate
 *
 * GateRegistry accumulates all gates for the session.
 */

#include "ctl_metrics.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace vsim::ctl {

// ============================================================================
// GateAssertRecord — one assertion within a gate
// ============================================================================

struct GateAssertRecord {
	std::string expression;
	bool        passed  = false;
	std::string reason;
};

// ============================================================================
// GateResult — final result of one gate
// ============================================================================

enum class GateStatus { Open, Passed, Failed };

struct GateResult {
	std::string                  name;
	GateStatus                   status   = GateStatus::Open;
	std::vector<GateAssertRecord> asserts;
	bool                         explicit_fail = false;

	bool passed() const { return status == GateStatus::Passed; }
	bool failed() const { return status == GateStatus::Failed; }
	bool open()   const { return status == GateStatus::Open;   }

	int pass_count() const {
		int n = 0;
		for (auto& a : asserts) if (a.passed) ++n;
		return n;
	}
	int fail_count() const {
		int n = 0;
		for (auto& a : asserts) if (!a.passed) ++n;
		return n;
	}
};

// ============================================================================
// GateState — mutable gate being executed
// ============================================================================

class GateState {
public:
	GateResult result;
	bool       is_open = false;

	// Open the gate
	void begin(const std::string& name) {
		result = {};
		result.name   = name;
		result.status = GateStatus::Open;
		is_open = true;
	}

	// Evaluate a metrics expression
	// Returns false if the assertion failed (caller may abort or continue)
	bool gate_assert(const std::string& expr, const MetricsStore& metrics) {
		if (!is_open) return false;
		auto ar = metrics.assert_expr(expr);
		GateAssertRecord rec;
		rec.expression = ar.expression;
		rec.passed     = ar.passed;
		rec.reason     = ar.reason;
		result.asserts.push_back(rec);
		return ar.passed;
	}

	// Explicit pass/fail override
	void explicit_pass() {
		if (is_open) result.status = GateStatus::Passed;
	}
	void explicit_fail(const std::string& reason = {}) {
		if (is_open) {
			result.explicit_fail = true;
			result.status        = GateStatus::Failed;
			if (!reason.empty()) {
				GateAssertRecord rec;
				rec.expression = "gate.fail(" + reason + ")";
				rec.passed     = false;
				rec.reason     = reason;
				result.asserts.push_back(rec);
			}
		}
	}

	// Close the gate — derives final status from assertion results
	GateResult end() {
		if (!is_open) return result;
		is_open = false;
		if (!result.explicit_fail) {
			// Derive from assertions
			bool any_fail = false;
			for (auto& a : result.asserts) if (!a.passed) { any_fail = true; break; }
			result.status = any_fail ? GateStatus::Failed : GateStatus::Passed;
		}
		return result;
	}
};

// ============================================================================
// GateRegistry — session-level accumulator
// ============================================================================

struct GateRegistry {
	std::vector<GateResult> results;

	void add(GateResult r) {
		results.push_back(std::move(r));
	}

	int total()   const { return static_cast<int>(results.size()); }
	int passed()  const {
		int n = 0;
		for (auto& r : results) if (r.passed()) ++n;
		return n;
	}
	int failed()  const {
		int n = 0;
		for (auto& r : results) if (r.failed()) ++n;
		return n;
	}
	bool all_passed() const { return failed() == 0 && total() > 0; }

	std::string summary() const {
		return std::to_string(passed()) + "/" + std::to_string(total()) + " gates passed";
	}
};

} // namespace vsim::ctl
