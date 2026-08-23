#pragma once
/**
 * ctl_dispatcher.hpp  —  VSIM-CTL runtime wrapper dispatch
 * =========================================================
 * WO-VSIM-CTL-03  |  v5.1.x
 *
 * CtlDispatcher walks an ExecPlan and dispatches each CtlCommand to
 * approved C++ runtime wrapper stubs.
 *
 * Key constraints (the "doctrine"):
 *   - VSIM-CTL may schedule, configure, gate, and export.
 *   - Only the C++ kernel may evolve physical state.
 *   - The script may NOT directly mutate particle arrays, forces, or S-state.
 *
 * This file provides:
 *   - DispatchResult        — per-command outcome
 *   - CtlRuntimeHooks       — interface for real runtime wrappers
 *   - CtlDispatcher         — main dispatch loop
 *
 * The hooks default to no-op stubs; real implementations inject via
 * CtlRuntimeHooks subclasses.
 */

#include "ctl_plan.hpp"
#include "ctl_metrics.hpp"
#include "ctl_gate.hpp"
#include <string>
#include <vector>
#include <functional>

namespace vsim::ctl {

// ============================================================================
// DispatchResult — outcome of one command
// ============================================================================

struct DispatchResult {
	std::string op;
	bool        ok      = true;
	std::string error;
};

// ============================================================================
// CtlRuntimeHooks — interface for C++ runtime wrappers
// All methods are no-op stubs by default.
// ============================================================================

struct CtlRuntimeHooks {
	// runtime.*
	virtual bool run_case(const std::string& name)   { (void)name;  return true; }
	virtual bool step(int64_t n)                      { (void)n;     return true; }
	virtual bool relax()                              {               return true; }
	virtual bool reset(const std::string& scope)      { (void)scope; return true; }
	virtual bool checkpoint()                         {               return true; }

	// kernel.*
	virtual bool kernel_set(const std::string& key, const std::string& val)
		{ (void)key; (void)val; return true; }
	virtual bool channel_enable(const std::string& ch, double weight)
		{ (void)ch; (void)weight; return true; }
	virtual bool channel_disable(const std::string& ch)
		{ (void)ch; return true; }
	virtual bool channel_reset()
		{ return true; }
	virtual bool trace_enable(const std::string& profile)
		{ (void)profile; return true; }

	// artifact.*
	virtual bool dynx_enable(const std::string& profile)
		{ (void)profile; return true; }
	virtual bool dynx_include(const std::string& payload)
		{ (void)payload; return true; }
	virtual bool dynx_validate()  { return true; }
	virtual bool dynx_export(const std::string& path)
		{ (void)path; return true; }

	virtual bool xbit_create(const std::string& mode)
		{ (void)mode; return true; }
	virtual bool xbit_validate() { return true; }
	virtual bool xbit_export(const std::string& path)
		{ (void)path; return true; }

	virtual bool report_write(const std::string& path)
		{ (void)path; return true; }
	virtual bool manifest_write(const std::string& path)
		{ (void)path; return true; }

	// Called after run_case completes so dispatcher can capture metrics
	virtual void on_run_complete(MetricsStore& /*metrics*/) {}

	virtual ~CtlRuntimeHooks() = default;
};

// ============================================================================
// DispatchSession — full session state during dispatch
// ============================================================================

struct DispatchSession {
	std::vector<DispatchResult> results;
	MetricsStore                metrics;
	GateRegistry                gates;
	GateState                   current_gate;
	bool                        abort = false;

	bool all_ok() const {
		for (auto& r : results) if (!r.ok) return false;
		return true;
	}
};

// ============================================================================
// CtlDispatcher
// ============================================================================

class CtlDispatcher {
public:
	explicit CtlDispatcher(CtlRuntimeHooks* hooks = nullptr)
		: hooks_(hooks ? hooks : &default_hooks_) {}

	DispatchSession dispatch(const ExecPlan& plan) {
		DispatchSession session;
		for (const auto& cmd : plan.commands) {
			if (session.abort) break;
			auto dr = dispatch_one(cmd, session);
			session.results.push_back(dr);
		}
		return session;
	}

private:
	CtlRuntimeHooks  default_hooks_;
	CtlRuntimeHooks* hooks_;

	DispatchResult dispatch_one(const CtlCommand& cmd, DispatchSession& session) {
		DispatchResult dr;
		dr.op = cmd.op;

		auto str_arg = [&](const std::string& key,
						   const std::string& fallback = {}) -> std::string {
			if (cmd.has_arg(key) && cmd.arg(key).is_string())
				return cmd.arg(key).as_string();
			return fallback;
		};
		auto dbl_arg = [&](const std::string& key, double fallback = 1.0) -> double {
			if (cmd.has_arg(key) && cmd.arg(key).is_double())
				return cmd.arg(key).as_double();
			if (cmd.has_arg(key) && cmd.arg(key).is_int())
				return static_cast<double>(cmd.arg(key).as_int());
			return fallback;
		};
		auto int_arg = [&](const std::string& key, int64_t fallback = 1) -> int64_t {
			if (cmd.has_arg(key) && cmd.arg(key).is_int())
				return cmd.arg(key).as_int();
			if (cmd.has_arg(key) && cmd.arg(key).is_double())
				return static_cast<int64_t>(cmd.arg(key).as_double());
			return fallback;
		};

		bool ok = true;

		switch (cmd.ns) {
		// ------------------------------------------------------------------
		case CtlNamespace::Control:
			// const/let were handled by parser — no runtime action needed
			break;

		// ------------------------------------------------------------------
		case CtlNamespace::Runtime: {
			const auto& sub = cmd.sub_op;
			if (sub == "run_case") {
				ok = hooks_->run_case(str_arg("name"));
				if (ok) {
					session.metrics.reset();
					hooks_->on_run_complete(session.metrics);
					session.metrics.capture();
				}
			} else if (sub == "step") {
				ok = hooks_->step(int_arg("name", 1));
			} else if (sub == "relax") {
				ok = hooks_->relax();
			} else if (sub == "reset") {
				ok = hooks_->reset(str_arg("name", "all"));
			} else if (sub == "checkpoint") {
				ok = hooks_->checkpoint();
			}
			break;
		}

		// ------------------------------------------------------------------
		case CtlNamespace::Kernel: {
			const auto& sub = cmd.sub_op;
			if (sub == "set") {
				ok = hooks_->kernel_set(str_arg("name"), str_arg("value"));
			} else if (sub == "channel.enable") {
				ok = hooks_->channel_enable(str_arg("name"), dbl_arg("weight", 1.0));
			} else if (sub == "channel.disable") {
				ok = hooks_->channel_disable(str_arg("name"));
			} else if (sub == "channel.reset") {
				ok = hooks_->channel_reset();
			} else if (sub == "trace.enable") {
				ok = hooks_->trace_enable(str_arg("name", str_arg("profile")));
			}
			break;
		}

		// ------------------------------------------------------------------
		case CtlNamespace::Artifact: {
			const auto& sub = cmd.sub_op;
			if      (sub == "dynx.enable")   ok = hooks_->dynx_enable(str_arg("name", str_arg("profile")));
			else if (sub == "dynx.include")  ok = hooks_->dynx_include(str_arg("name"));
			else if (sub == "dynx.validate") ok = hooks_->dynx_validate();
			else if (sub == "dynx.export")   ok = hooks_->dynx_export(str_arg("name"));
			else if (sub == "xbit.create")   ok = hooks_->xbit_create(str_arg("name", str_arg("mode")));
			else if (sub == "xbit.validate") ok = hooks_->xbit_validate();
			else if (sub == "xbit.export")   ok = hooks_->xbit_export(str_arg("name"));
			else if (sub == "report.write")  ok = hooks_->report_write(str_arg("name"));
			else if (sub == "manifest.write")ok = hooks_->manifest_write(str_arg("name"));
			break;
		}

		// ------------------------------------------------------------------
		case CtlNamespace::Metrics: {
			const auto& sub = cmd.sub_op;
			if (sub == "capture") {
				session.metrics.capture();
			} else if (sub == "assert") {
				auto expr = str_arg("name"); // positional arg
				if (expr.empty()) expr = str_arg("expression");
				auto ar = session.metrics.assert_expr(expr);
				if (!ar.passed) {
					ok = false;
					dr.error = "metrics.assert failed: " + expr +
							   (ar.reason.empty() ? "" : " (" + ar.reason + ")");
				}
			}
			break;
		}

		// ------------------------------------------------------------------
		case CtlNamespace::Gate: {
			const auto& sub = cmd.sub_op;
			if (sub == "begin") {
				session.current_gate.begin(str_arg("name"));
			} else if (sub == "assert") {
				auto expr = str_arg("name");
				bool passed = session.current_gate.gate_assert(expr, session.metrics);
				if (!passed) {
					// Gate assertion failure doesn't abort the session by default
					// (gates accumulate failures; gate.end() finalises)
				}
			} else if (sub == "pass") {
				session.current_gate.explicit_pass();
			} else if (sub == "fail") {
				session.current_gate.explicit_fail(str_arg("name"));
			} else if (sub == "end") {
				auto result = session.current_gate.end();
				session.gates.add(result);
				if (result.failed()) {
					dr.error = "gate '" + result.name + "' failed (" +
							   std::to_string(result.fail_count()) + " assertion(s) failed)";
					// Not a hard abort — keep running, accumulate all gate results
				}
			}
			break;
		}

		default:
			ok = false;
			dr.error = "unhandled namespace for op '" + cmd.op + "'";
			break;
		}

		dr.ok = ok;
		return dr;
	}
};

} // namespace vsim::ctl
