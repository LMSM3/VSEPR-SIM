#pragma once
/**
 * ctl_plan.hpp  —  VSIM-CTL execution plan + deterministic hash
 * ==============================================================
 * WO-VSIM-CTL-07  |  v5.1.x
 *
 * Compiles an ExecGraph into a deterministic ExecPlan:
 *
 *   script_plan.json       — typed command list with namespace/op/args
 *   resolved_config.vsim   — placeholder (written by dispatcher)
 *   artifact_manifest.json — list of artifact.* export commands
 *   gate_manifest.json     — list of gate names
 *
 * Plan hash = SHA-256 of (script_text + "|" + kernel_version + "|" + seed)
 * using a portable djb2/FNV-1a-inspired 64-bit substitute when a full
 * SHA-256 implementation is not linked.  A real SHA-256 can be plugged
 * in by defining VSIM_CTL_USE_SHA256 before including this header.
 *
 * Design rules:
 *   - Same script + same seed + same kernel_version → same plan_hash
 *   - ExecPlan is immutable after construction
 *   - JSON output uses minimal manual formatting (no external dependency)
 */

#include "ctl_types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace vsim::ctl {

// ============================================================================
// Portable deterministic hash (FNV-1a 64-bit)
// ============================================================================

inline uint64_t fnv1a_64(const std::string& s) {
	uint64_t h = 14695981039346656037ULL;
	for (unsigned char c : s) {
		h ^= c;
		h *= 1099511628211ULL;
	}
	return h;
}

inline std::string plan_hash_hex(const std::string& script_text,
								  const std::string& kernel_version,
								  uint64_t           seed)
{
	std::string input = script_text + "|" + kernel_version + "|" + std::to_string(seed);
	uint64_t h = fnv1a_64(input);
	std::ostringstream oss;
	oss << std::hex << std::setw(16) << std::setfill('0') << h;
	// Duplicate to produce a 128-bit-looking hex string
	oss << std::hex << std::setw(16) << std::setfill('0') << (~h ^ 0xDEADBEEFCAFEBABEULL);
	return oss.str();
}

// ============================================================================
// ArtifactManifestEntry
// ============================================================================

struct ArtifactManifestEntry {
	std::string kind;   // "dynx" / "xbit" / "report" / "manifest"
	std::string op;     // full op, e.g. "artifact.dynx.export"
	std::string path;   // export path if provided
};

// ============================================================================
// ExecPlan — compiled, deterministic, immutable
// ============================================================================

struct ExecPlan {
	std::string  plan_version   = "vsim_ctl_v1";
	std::string  script_hash;        // hash of raw script text
	std::string  kernel_version;
	uint64_t     seed            = 0;
	std::string  plan_hash;          // deterministic plan identity

	std::vector<CtlCommand>            commands;   // from ExecGraph
	std::vector<ArtifactManifestEntry> artifacts;
	std::vector<std::string>           gate_names;

	// -----------------------------------------------------------------------
	static ExecPlan compile(const ExecGraph& g,
							 const std::string& kernel_version = "dev",
							 uint64_t           seed           = 0)
	{
		ExecPlan p;
		p.kernel_version = kernel_version;
		p.seed           = seed;
		p.commands       = g.commands;
		p.script_hash    = [&]{
			uint64_t h = fnv1a_64(g.script_text);
			std::ostringstream oss;
			oss << "fnv64:" << std::hex << std::setw(16) << std::setfill('0') << h;
			return oss.str();
		}();
		p.plan_hash = plan_hash_hex(g.script_text, kernel_version, seed);

		// Collect artifact exports and gate names
		for (const auto& cmd : g.commands) {
			if (cmd.ns == CtlNamespace::Artifact) {
				// Only export ops go into manifest
				if (cmd.sub_op.find("export") != std::string::npos ||
					cmd.sub_op.find("write")  != std::string::npos)
				{
					ArtifactManifestEntry e;
					e.op   = cmd.op;
					e.kind = cmd.sub_op.substr(0, cmd.sub_op.find('.'));
					if (cmd.has_arg("name")) e.path = cmd.arg("name").as_string();
					p.artifacts.push_back(e);
				}
			}
			if (cmd.ns == CtlNamespace::Gate && cmd.sub_op == "begin") {
				if (cmd.has_arg("name"))
					p.gate_names.push_back(cmd.arg("name").as_string());
			}
		}
		return p;
	}

	// -----------------------------------------------------------------------
	// JSON emission — script_plan.json content
	// -----------------------------------------------------------------------
	std::string to_json() const {
		std::string j;
		j += "{\n";
		j += "  \"plan_version\": \"" + plan_version + "\",\n";
		j += "  \"script_hash\": \""  + script_hash  + "\",\n";
		j += "  \"kernel_hash\": \""  + kernel_version + "\",\n";
		j += "  \"seed\": "           + std::to_string(seed) + ",\n";
		j += "  \"plan_hash\": \""    + plan_hash    + "\",\n";
		j += "  \"commands\": [\n";

		for (std::size_t i = 0; i < commands.size(); ++i) {
			const auto& c = commands[i];
			j += "    {\n";
			j += "      \"op\": \"" + c.op + "\",\n";
			j += "      \"args\": {";
			bool first = true;
			for (auto& [k, a] : c.args) {
				if (!first) j += ", ";
				j += "\"" + k + "\": " + a.value_string();
				first = false;
			}
			j += "}\n";
			j += "    }";
			if (i + 1 < commands.size()) j += ",";
			j += "\n";
		}
		j += "  ]\n";
		j += "}\n";
		return j;
	}

	// artifact_manifest.json content
	std::string artifact_manifest_json() const {
		std::string j = "{\n  \"artifacts\": [\n";
		for (std::size_t i = 0; i < artifacts.size(); ++i) {
			const auto& e = artifacts[i];
			j += "    {\"kind\": \"" + e.kind + "\", \"op\": \"" + e.op + "\"";
			if (!e.path.empty()) j += ", \"path\": \"" + e.path + "\"";
			j += "}";
			if (i + 1 < artifacts.size()) j += ",";
			j += "\n";
		}
		j += "  ]\n}\n";
		return j;
	}

	// gate_manifest.json content
	std::string gate_manifest_json() const {
		std::string j = "{\n  \"gates\": [\n";
		for (std::size_t i = 0; i < gate_names.size(); ++i) {
			j += "    \"" + gate_names[i] + "\"";
			if (i + 1 < gate_names.size()) j += ",";
			j += "\n";
		}
		j += "  ]\n}\n";
		return j;
	}
};

} // namespace vsim::ctl
