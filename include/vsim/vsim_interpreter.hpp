#pragma once
/**
 * include/vsim/vsim_interpreter.hpp
 * ===================================
 * Minimal VSIM expression interpreter for pbc.* and particle.* builtins.
 *
 * Scope:
 *   - Evaluates single-level namespace.function(args) call expressions.
 *   - Resolves variable names from a local scope map.
 *   - Dispatches to registered BuiltinFn handlers.
 *   - Exposes field access (.x, .y, .z) on XYZVec3 and Int3 return values.
 *   - Supports variable assignment via exec() for simple "name = expr" lines.
 *
 * This interpreter is intentionally minimal. It is not a general expression
 * evaluator. It handles exactly the expressions needed for pbc.* and
 * particle.* script blocks. Anything beyond that is 57E work.
 *
 * PBCInterpreterRuntime
 * ─────────────────────
 * Separate from the legacy VsimRuntime (vsim_runtime.hpp) which drives
 * the beta-10 simulation loop. PBCInterpreterRuntime is the context object
 * the interpreter bindings operate on. It holds particles, image counts,
 * and boundary/PBC configuration.
 *
 * Initialization order (required):
 *   1. Populate PBCInterpreterRuntime from parsed [cell]/[boundary]/[pbc] blocks
 *   2. Allocate particles and image_counts
 *   3. interp.set_runtime(rt)
 *   4. register_pbc_builtins(interp)
 *   5. register_particle_builtins(interp)
 *   6. Execute script blocks
 *
 * WO-VSEPR-SIM-57D  |  beta-8
 */

#include "vsim/vsim_value.hpp"
#include "vsim/vsim_document.hpp"
#include "box/pbc.hpp"

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace vsim {

// ── Particle: minimal state representation ────────────────────────────────────

struct ScriptParticle {
	XYZVec3 position;          // Wrapped coordinate (angstrom)
	std::string symbol;        // Element symbol ("Ar", "Na", ...)
	int Z = 0;                 // Atomic number
};

// ── PBCInterpreterRuntime ─────────────────────────────────────────────────────
// The interpreter context. Bindings receive a reference to this struct.
// All per-particle arrays are 0-indexed; VSIM scripts are 1-indexed.

struct PBCInterpreterRuntime {
	vsepr::PeriodicCell            cell;               // Active periodic cell
	vsepr::BoundaryConfig          boundary;           // Per-axis boundary modes
	PBCSection                     pbc_config;         // [pbc] options

	std::vector<ScriptParticle>    particles;          // Particle state (0-indexed)
	std::vector<vsepr::ImageCount> image_counts;       // Current step (0-indexed)
	std::vector<vsepr::ImageCount> image_counts_prev;  // Previous step (0-indexed)

	int particle_count() const {
		return static_cast<int>(particles.size());
	}

	void allocate_image_counts() {
		image_counts.assign(particles.size(), {0, 0, 0});
		image_counts_prev.assign(particles.size(), {0, 0, 0});
	}

	void snapshot_image_counts() {
		image_counts_prev = image_counts;
	}
};

// ── BuiltinFn signature ───────────────────────────────────────────────────────

using BuiltinFn = std::function<
	Value(const std::vector<Value>&, PBCInterpreterRuntime&)
>;

// ── VsimInterpreter ───────────────────────────────────────────────────────────

class VsimInterpreter {
public:
	// Set the runtime context. Must be called before register_*_builtins().
	void set_runtime(PBCInterpreterRuntime& rt) { runtime_ = &rt; }

	// Register a named builtin function.
	void register_builtin(const std::string& name, BuiltinFn fn) {
		builtins_[name] = std::move(fn);
	}

	// Evaluate a single expression and return its value.
	// Supports:
	//   namespace.function(args...)   — builtin call
	//   function(args...)             — builtin call (no namespace)
	//   xyzvec3(x, y, z)             — vector literal constructor
	//   numeric literal              — int64 or double
	//   quoted string literal        — std::string
	//   variable_name                — lookup in scope_
	//   expr.x  expr.y  expr.z      — field access on XYZVec3 / Int3
	Value eval(const std::string& expr);

	// Execute a multi-line script block (one "name = expr" per line).
	void exec(const std::string& script_block);

	// Variable scope (writable for test setup).
	std::unordered_map<std::string, Value> scope;

private:
	PBCInterpreterRuntime*                     runtime_ = nullptr;
	std::unordered_map<std::string, BuiltinFn> builtins_;

	Value eval_call(const std::string& fn_name,
					const std::string& args_str);
	Value eval_atom(const std::string& token);
	std::vector<Value> parse_args(const std::string& args_str);
};

// ── Registration declarations ─────────────────────────────────────────────────

void register_pbc_builtins     (VsimInterpreter& interp);
void register_particle_builtins(VsimInterpreter& interp);

} // namespace vsim
