/**
 * src/vsim/register_particle_builtins.cpp
 * ==========================================
 * Registers particle.* builtins and the xyzvec3 vector constructor.
 *
 * Builtins registered:
 *   xyzvec3(x, y, z)         — construct XYZVec3 from three doubles
 *   particle.position(id)    — returns XYZVec3 wrapped position for particle id
 *
 * Particle IDs are 1-indexed in scripts.
 *
 * WO-VSEPR-SIM-57D  |  beta-8
 */

#include "vsim/vsim_interpreter.hpp"

namespace vsim {

void register_particle_builtins(VsimInterpreter& interp) {

	// ── xyzvec3(x, y, z) → XYZVec3 ───────────────────────────────────────
	interp.register_builtin("xyzvec3",
	[](const std::vector<Value>& args, PBCInterpreterRuntime&) -> Value {
		if (args.size() != 3)
			throw VsimRuntimeError(
				"xyzvec3 expects 3 arguments (x, y, z), got "
				+ std::to_string(args.size()) + ".");
		auto to_d = [&](const Value& v) -> double {
			if (value_is_double(v)) return as_double(v);
			if (value_is_int(v))    return static_cast<double>(as_int(v));
			throw VsimRuntimeError("xyzvec3: arguments must be numeric.");
		};
		XYZVec3 r{to_d(args[0]), to_d(args[1]), to_d(args[2])};
		return r;
	});

	// ── particle.position(id) → XYZVec3 ──────────────────────────────────
	interp.register_builtin("particle.position",
	[](const std::vector<Value>& args, PBCInterpreterRuntime& rt) -> Value {
		if (args.size() != 1)
			throw VsimRuntimeError(
				"particle.position expects 1 argument (int id).");

		if (!value_is_int(args[0]))
			throw VsimRuntimeError(
				"particle.position: argument must be an integer particle ID.");

		int id  = static_cast<int>(as_int(args[0]));
		int idx = id - 1;

		if (id < 1 || idx >= rt.particle_count())
			throw VsimRuntimeError(
				"particle.position: ID " + std::to_string(id) +
				" out of range. System has " +
				std::to_string(rt.particle_count()) + " particles.");

		return rt.particles[idx].position;
	});
}

} // namespace vsim
