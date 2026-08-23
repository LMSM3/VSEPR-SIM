#pragma once
/**
 * ctl_types.hpp  —  VSIM-CTL core type definitions
 * ==================================================
 * WO-VSIM-CTL-01  |  v5.1.x
 *
 * Defines the fundamental vocabulary of the VSIM Control Language:
 *   - CtlNamespace  : namespace enum (runtime / kernel / artifact / metrics / gate / control)
 *   - CtlArg        : typed argument (string, double, bool, int64)
 *   - CtlCommand    : single resolved command with namespace, op, and arg map
 *   - ExecGraph     : ordered, immutable sequence of CtlCommands — the compiled script
 *
 * Design rules:
 *   - ExecGraph is the ONLY object passed to the dispatcher.
 *   - Scripts may NOT mutate particle state directly — only through approved commands.
 *   - The kernel receives typed structs, never raw strings.
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <cstdint>

namespace vsim::ctl {

// ============================================================================
// Namespace enumeration
// ============================================================================

enum class CtlNamespace : uint8_t {
	Control,   // const / let / for / if / match / set / require / assert
	Runtime,   // runtime.*
	Kernel,    // kernel.*
	Artifact,  // artifact.*
	Metrics,   // metrics.*
	Gate,      // gate.*
	Unknown
};

inline CtlNamespace parse_namespace(const std::string& s) {
	if (s == "runtime")  return CtlNamespace::Runtime;
	if (s == "kernel")   return CtlNamespace::Kernel;
	if (s == "artifact") return CtlNamespace::Artifact;
	if (s == "metrics")  return CtlNamespace::Metrics;
	if (s == "gate")     return CtlNamespace::Gate;
	if (s == "const" || s == "let" || s == "for" ||
		s == "if"    || s == "match" || s == "set" ||
		s == "require" || s == "assert")
		return CtlNamespace::Control;
	return CtlNamespace::Unknown;
}

inline std::string namespace_name(CtlNamespace ns) {
	switch (ns) {
		case CtlNamespace::Control:  return "control";
		case CtlNamespace::Runtime:  return "runtime";
		case CtlNamespace::Kernel:   return "kernel";
		case CtlNamespace::Artifact: return "artifact";
		case CtlNamespace::Metrics:  return "metrics";
		case CtlNamespace::Gate:     return "gate";
		default:                     return "unknown";
	}
}

// ============================================================================
// CtlArg — typed argument value
// ============================================================================

using CtlArgValue = std::variant<std::string, double, bool, int64_t>;

struct CtlArg {
	std::string   key;
	CtlArgValue   value;

	// Convenience accessors
	bool is_string()  const { return std::holds_alternative<std::string>(value); }
	bool is_double()  const { return std::holds_alternative<double>(value); }
	bool is_bool()    const { return std::holds_alternative<bool>(value); }
	bool is_int()     const { return std::holds_alternative<int64_t>(value); }

	const std::string& as_string() const { return std::get<std::string>(value); }
	double             as_double() const { return std::get<double>(value); }
	bool               as_bool()   const { return std::get<bool>(value); }
	int64_t            as_int()    const { return std::get<int64_t>(value); }

	std::string value_string() const {
		if (is_string()) return '"' + as_string() + '"';
		if (is_double()) return std::to_string(as_double());
		if (is_bool())   return as_bool() ? "true" : "false";
		return std::to_string(as_int());
	}
};

using CtlArgMap = std::unordered_map<std::string, CtlArg>;

// ============================================================================
// CtlCommand — one resolved instruction
// ============================================================================

struct CtlCommand {
	CtlNamespace ns          = CtlNamespace::Unknown;
	std::string  op;          // full qualified op, e.g. "kernel.channel.enable"
	std::string  sub_op;      // sub-op portion after the namespace prefix, e.g. "channel.enable"
	CtlArgMap    args;
	int          source_line = 0;

	bool has_arg(const std::string& k) const {
		return args.count(k) != 0;
	}

	const CtlArg& arg(const std::string& k) const {
		auto it = args.find(k);
		if (it == args.end())
			throw std::out_of_range("CtlCommand::arg — key not found: " + k);
		return it->second;
	}

	// Positional first string arg (common pattern for single-arg commands)
	std::string first_string(const std::string& fallback = "") const {
		for (auto& [k, a] : args)
			if (a.is_string()) return a.as_string();
		return fallback;
	}
};

// ============================================================================
// ExecGraph — compiled, ordered, immutable execution plan
// ============================================================================

struct ExecGraph {
	std::string              script_text;   // original source
	std::string              script_hash;   // SHA-256 hex of script_text (set by planner)
	std::vector<CtlCommand>  commands;

	bool empty() const { return commands.empty(); }
	std::size_t size() const { return commands.size(); }

	const CtlCommand& operator[](std::size_t i) const { return commands[i]; }
};

// ============================================================================
// Known kernel channels (validated before runtime)
// ============================================================================

inline const std::vector<std::string>& known_kernel_channels() {
	static const std::vector<std::string> ch = {
		"repulsion", "dispersion", "coulomb",
		"bond", "field", "state",
		"lj", "ewald", "wall", "angle", "dihedral"
	};
	return ch;
}

inline bool is_known_kernel_channel(const std::string& name) {
	for (auto& c : known_kernel_channels())
		if (c == name) return true;
	return false;
}

// ============================================================================
// Known commands per namespace (used by validator)
// ============================================================================

inline const std::vector<std::string>& known_runtime_ops() {
	static const std::vector<std::string> ops = {
		"run_case", "step", "relax", "reset", "checkpoint"
	};
	return ops;
}

inline const std::vector<std::string>& known_kernel_ops() {
	static const std::vector<std::string> ops = {
		"set",
		"channel.enable", "channel.disable", "channel.reset",
		"trace.enable"
	};
	return ops;
}

inline const std::vector<std::string>& known_artifact_ops() {
	static const std::vector<std::string> ops = {
		"xbit.create", "xbit.validate", "xbit.export",
		"dynx.enable", "dynx.include", "dynx.validate", "dynx.export",
		"report.write", "manifest.write"
	};
	return ops;
}

inline const std::vector<std::string>& known_metrics_ops() {
	static const std::vector<std::string> ops = {
		"capture", "assert", "export", "require"
	};
	return ops;
}

inline const std::vector<std::string>& known_gate_ops() {
	static const std::vector<std::string> ops = {
		"begin", "pass", "fail", "assert", "end"
	};
	return ops;
}

inline const std::vector<std::string>& known_control_ops() {
	static const std::vector<std::string> ops = {
		"const", "let", "for", "if", "match", "set", "require", "assert"
	};
	return ops;
}

inline bool is_known_op(CtlNamespace ns, const std::string& sub_op) {
	auto check = [&](const std::vector<std::string>& ops) {
		for (auto& o : ops) if (o == sub_op) return true;
		return false;
	};
	switch (ns) {
		case CtlNamespace::Runtime:  return check(known_runtime_ops());
		case CtlNamespace::Kernel:   return check(known_kernel_ops());
		case CtlNamespace::Artifact: return check(known_artifact_ops());
		case CtlNamespace::Metrics:  return check(known_metrics_ops());
		case CtlNamespace::Gate:     return check(known_gate_ops());
		case CtlNamespace::Control:  return check(known_control_ops());
		default:                     return false;
	}
}

} // namespace vsim::ctl
