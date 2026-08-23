#pragma once
/**
 * ctl_validator.hpp  —  VSIM-CTL semantic validator
 * ==================================================
 * WO-VSIM-CTL-03  |  v5.1.x
 *
 * Validates an ExecGraph BEFORE any runtime dispatch.
 *
 * Checks:
 *   1. Unknown namespace           → error
 *   2. Unknown command in namespace → error
 *   3. Unknown kernel channel name  → error (kernel.channel.enable/disable)
 *   4. Dynx export before enable    → error
 *   5. XBIT export before create    → error
 *   6. metrics.* read before capture/run → error (structural check)
 *   7. gate.end without gate.begin  → error
 *   8. gate.begin without gate.end  → error
 *
 * ValidationResult::ok == false means the script must NOT be dispatched.
 */

#include "ctl_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace vsim::ctl {

// ============================================================================
// ValidationError — one semantic error
// ============================================================================

struct ValidationError {
	int         line  = 0;
	std::string op;
	std::string message;
};

// ============================================================================
// ValidationResult
// ============================================================================

struct ValidationResult {
	bool                         ok = true;
	std::vector<ValidationError> errors;

	void add(int line, const std::string& op, const std::string& msg) {
		ok = false;
		errors.push_back({ line, op, msg });
	}

	std::string first_error() const {
		if (errors.empty()) return {};
		const auto& e = errors.front();
		return "line " + std::to_string(e.line) + " [" + e.op + "]: " + e.message;
	}
};

// ============================================================================
// CtlValidator
// ============================================================================

class CtlValidator {
public:
	static ValidationResult validate(const ExecGraph& g) {
		CtlValidator v;
		v.run(g);
		return v.result_;
	}

private:
	ValidationResult result_;

	// State for flow checks
	bool dynx_enabled_   = false;
	bool xbit_created_   = false;
	bool run_executed_   = false;
	bool metrics_captured_ = false;
	int  gate_depth_     = 0;
	int  gate_begin_line_ = 0;

	void run(const ExecGraph& g) {
		for (const auto& cmd : g.commands) {
			check_cmd(cmd);
		}
		// Post-graph checks
		if (gate_depth_ > 0) {
			result_.add(gate_begin_line_, "gate.begin",
						"gate.begin has no matching gate.end");
		}
	}

	void check_cmd(const CtlCommand& cmd) {
		const int    line   = cmd.source_line;
		const auto&  op     = cmd.op;
		const auto&  sub_op = cmd.sub_op;
		const auto   ns     = cmd.ns;

		// 1. Unknown namespace
		if (ns == CtlNamespace::Unknown) {
			result_.add(line, op, "unknown namespace");
			return;
		}

		// 2. Unknown command
		if (!is_known_op(ns, sub_op)) {
			result_.add(line, op,
				"unknown command '" + sub_op + "' in namespace '" +
				namespace_name(ns) + "'");
			return;
		}

		// 3. Kernel channel validation
		if (ns == CtlNamespace::Kernel &&
			(sub_op == "channel.enable" || sub_op == "channel.disable"))
		{
			std::string ch_name;
			if (cmd.has_arg("name")) ch_name = cmd.arg("name").as_string();
			else if (cmd.has_arg("__pos0")) ch_name = cmd.arg("__pos0").as_string();

			if (!ch_name.empty() && !is_known_kernel_channel(ch_name)) {
				result_.add(line, op,
					"unknown kernel channel '" + ch_name + "'");
			}
		}

		// 4/5. Artifact prerequisite checks
		if (ns == CtlNamespace::Artifact) {
			if (sub_op == "dynx.enable")  { dynx_enabled_ = true; }
			if (sub_op == "xbit.create")  { xbit_created_ = true; }

			if (sub_op == "dynx.export" || sub_op == "dynx.validate" || sub_op == "dynx.include") {
				if (!dynx_enabled_)
					result_.add(line, op,
						"artifact.dynx." + sub_op.substr(5) +
						" requires artifact.dynx.enable before use");
			}
			if (sub_op == "xbit.export" || sub_op == "xbit.validate") {
				if (!xbit_created_)
					result_.add(line, op,
						"artifact.xbit." + sub_op.substr(5) +
						" requires artifact.xbit.create before use");
			}
		}

		// 6. metrics.* before capture/run
		if (ns == CtlNamespace::Metrics) {
			if (sub_op == "capture") {
				metrics_captured_ = true;
			} else if (sub_op == "assert" || sub_op == "export" || sub_op == "require") {
				if (!metrics_captured_ && !run_executed_)
					result_.add(line, op,
						"metrics." + sub_op +
						" used before metrics.capture() or runtime.run_case()");
			}
		}

		if (ns == CtlNamespace::Runtime && sub_op == "run_case") {
			run_executed_     = true;
			metrics_captured_ = false; // reset: must re-capture after next run
		}

		// 7/8. gate depth tracking
		if (ns == CtlNamespace::Gate) {
			if (sub_op == "begin") {
				++gate_depth_;
				gate_begin_line_ = line;
			} else if (sub_op == "end") {
				if (gate_depth_ == 0) {
					result_.add(line, op, "gate.end without matching gate.begin");
				} else {
					--gate_depth_;
				}
			}
		}
	}
};

} // namespace vsim::ctl
