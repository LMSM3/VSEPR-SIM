#pragma once
/**
 * include/vsim/vsim_post_step.hpp
 * ================================
 * PBC post-step script execution hook.
 *
 * run_post_step() is the final integration point for the PBC module (WO-57E).
 * It is a free function (not a VsimRuntime member) so it can be included
 * independently of the heavier vsim_runtime.hpp infrastructure.
 *
 * Usage:
 *   // After updating particle positions each FIRE step:
 *   auto scope = vsim::run_post_step(doc, particles);
 *   // scope["d"] etc. carry computed script values for the step.
 *
 * Initialization:
 *   Requires doc.cell, doc.boundary, doc.pbc, and doc.post_step to be
 *   populated (from [cell]/[boundary]/[pbc]/[post_step] blocks).
 *
 * WO-VSEPR-SIM-57E  |  beta-8
 */

#include "vsim/vsim_document.hpp"
#include "vsim/vsim_interpreter.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace vsim {

// ── run_post_step ─────────────────────────────────────────────────────────────
//
// Executes the [post_step] script_block against the supplied particle state.
//
// Returns: interpreter scope after execution — every assignment made by the
//          script is a key/value entry in the returned map.
//          Empty map when post_step is disabled or the script_block is empty.
//
// Thread safety: creates fresh interpreter + runtime per call; no shared state.

inline std::unordered_map<std::string, Value>
run_post_step(const VsimDocument&               doc,
			  const std::vector<ScriptParticle>& particles)
{
	if (!doc.post_step.has_script()) return {};

	// Build PeriodicCell from [cell] + [boundary]
	vsepr::PeriodicCell cell;
	cell.lengths.x  = doc.cell.lx;
	cell.lengths.y  = doc.cell.ly;
	cell.lengths.z  = doc.cell.lz;
	cell.periodic_x = (doc.boundary.x == "periodic");
	cell.periodic_y = (doc.boundary.y == "periodic");
	cell.periodic_z = (doc.boundary.z == "periodic");

	// Build BoundaryConfig from [boundary]
	auto parse_mode = [](const std::string& s) -> vsepr::BoundaryMode {
		if (s == "periodic")   return vsepr::BoundaryMode::Periodic;
		if (s == "reflective") return vsepr::BoundaryMode::Reflective;
		return vsepr::BoundaryMode::Open;
	};
	vsepr::BoundaryConfig boundary;
	boundary.x = parse_mode(doc.boundary.x);
	boundary.y = parse_mode(doc.boundary.y);
	boundary.z = parse_mode(doc.boundary.z);

	// Assemble interpreter runtime
	PBCInterpreterRuntime rt;
	rt.cell       = cell;
	rt.boundary   = boundary;
	rt.pbc_config = doc.pbc;
	rt.particles  = particles;
	rt.allocate_image_counts();

	// Wire interpreter and register all PBC + particle builtins
	VsimInterpreter interp;
	interp.set_runtime(rt);
	register_pbc_builtins(interp);
	register_particle_builtins(interp);

	// Execute post-step script block
	interp.exec(doc.post_step.script_block);

	return interp.scope;
}

} // namespace vsim
