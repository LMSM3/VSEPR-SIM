/**
 * src/vsim/bindings/pbc_bindings.cpp
 * =====================================
 * Registration stub for the pbc.* VSIM scripting namespace.
 *
 * The six pbc.* binding functions are defined as inline helpers in
 * include/vsim/bindings/pbc_bindings.hpp.  This compilation unit provides
 * register_pbc_builtins(), which will be called once during interpreter
 * initialization (after VsimRuntime is configured, before script execution).
 *
 * Full interpreter dispatch wiring is deferred to the WO that introduces
 * VsimInterpreter.  Until then, register_pbc_builtins() is a well-typed
 * declaration target that satisfies forward references and link-time checks.
 *
 * Gate status:
 *   57B — inline binding helpers:  PASS
 *   57C — registration stub:       PASS
 *   57D — interpreter dispatch:    PENDING
 *
 * WO-VSEPR-SIM-57C  |  beta-8
 */

#include "vsim/bindings/pbc_bindings.hpp"

namespace vsim {

/**
 * register_pbc_builtins
 * ----------------------
 * Registers all six pbc.* built-in functions with the VSIM interpreter.
 *
 * Called once during interpreter initialization.
 * Must be called after VsimRuntime is configured from parsed document.
 * Must be called before script execution begins.
 *
 * When VsimInterpreter is introduced (57D), this function body will call:
 *
 *   interp.register_builtin("pbc.wrap",             pbc_bindings::pbc_wrap_binding);
 *   interp.register_builtin("pbc.delta",            pbc_bindings::pbc_delta_binding);
 *   interp.register_builtin("pbc.distance",         pbc_bindings::pbc_distance_binding);
 *   interp.register_builtin("pbc.crossed_boundary", pbc_bindings::pbc_crossed_boundary_binding);
 *   interp.register_builtin("pbc.image_count",      pbc_bindings::pbc_image_count_binding);
 *   interp.register_builtin("pbc.unwrap",           pbc_bindings::pbc_unwrap_binding);
 *
 * Particle IDs are 1-indexed in VSIM scripts.  All ID-accepting bindings
 * apply the 1→0 offset internally via require_particle_id().
 */
void register_pbc_builtins(/* VsimInterpreter& interp */) {
    // Body intentionally empty until VsimInterpreter is available (WO-57D).
    // The function is called at init time; its presence in the init sequence
    // is the contract, not its current body.
}

} // namespace vsim

