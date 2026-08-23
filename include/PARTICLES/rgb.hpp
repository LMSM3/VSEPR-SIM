#pragma once
/**
 * rgb.hpp  -  RGB Colour Triple (float, [0,1])
 *
 * Shared primitive used by both the transient and QCD vis layers.
 * Extracted from coarse_grain/vis/transient_renderer.hpp so that both
 * transient_particle.hpp and qcd_colour.hpp can include it without a
 * fragile include-ordering dependency.
 *
 * v5.1.3~2  |  WO-63A  |  v5.0.0-main
 */

namespace vsepr {
namespace particles {

struct RGB {
	float r{}, g{}, b{};
};

} // namespace particles
} // namespace vsepr
