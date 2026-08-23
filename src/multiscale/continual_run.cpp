/**
 * continual_run.cpp  --  WO-74A / WO-74D
 *
 * All logic is inline in continual_run.hpp; this translation unit
 * satisfies the CMake target and provides a link-time anchor.
 */

#include "multiscale/continual_run.hpp"

// No out-of-line definitions required: uncertainty_weighted_update and
// apply_prior_update are fully inline in the header.