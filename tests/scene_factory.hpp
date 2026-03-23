#pragma once
/**
 * scene_factory.hpp — Backward-compatible aggregator header
 *
 * This header includes the split test infrastructure:
 *   - scene_builders.hpp: SceneBead, Xorshift32, scene generators,
 *     transforms (translate/rotate), validation helpers, neighbour builder
 *   - test_runners.hpp: EtaTrajectory, run_trajectory, run_all_beads,
 *     behavioral assertions, statistics, sweep harness, records
 *
 * All existing #include "tests/scene_factory.hpp" statements continue
 * to work without modification.
 *
 * Reference: Suite #2/#3 specification from development sessions
 */

#include "tests/scene_builders.hpp"
#include "tests/test_runners.hpp"
