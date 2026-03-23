#pragma once
/**
 * cg_commands.hpp — Coarse-Grained CLI Command Layer
 *
 * Scientific operator console for the coarse-grained engine.
 * Provides five clean operations:
 *
 *   1. scene   — Load or build a scene (bead configurations)
 *   2. inspect — Inspect bead/system state
 *   3. env     — Run environment update pipeline
 *   4. interact — Evaluate interactions / step the system
 *   5. viz     — Launch lightweight visualization (stub)
 *
 * Architecture:
 *   CLI → cg_dispatch() → CGSystemState → kernel/environment engine
 *
 * Grammar:
 *   vsepr cg <action> [options]
 *
 * Reference: Layer C1 (CLI Frontend)
 */

#include <string>
#include <vector>

namespace vsepr {
namespace cli {

/**
 * Main CG dispatcher.  Called from vsepr main() when argv[1] == "cg".
 *
 * Routes to scene / inspect / env / interact / viz based on argv[2].
 *
 * @param argc  Original argc
 * @param argv  Original argv (argv[0] = "vsepr", argv[1] = "cg", argv[2] = action)
 * @return Exit code (0 = success)
 */
int cg_dispatch(int argc, char** argv);

}} // namespace vsepr::cli
