#pragma once
/**
 * xyz_to_snapshot.hpp -- XYZFrame → FrameSnapshot conversion
 * ===========================================================
 * VSEPR-SIM  |  vis layer utility
 *
 * Converts a vsepr::io::XYZFrame (the ground-truth trajectory record from
 * the IO layer) into a vsepr::FrameSnapshot (the data contract consumed by
 * the renderer / VizRouter).
 *
 * This is the only place that should perform this mapping so field additions
 * in either struct have a single update point.
 */

#include "core/frame_snapshot.hpp"
#include "io/xyz_unified.hpp"

namespace vsepr {
namespace vis {

/**
 * Convert an XYZFrame to a FrameSnapshot suitable for the renderer.
 *
 * Mapping:
 *   AtomRecord.Z          → FrameSnapshot.atomic_numbers
 *   AtomRecord.{x,y,z}   → FrameSnapshot.positions
 *   XYZFrame.energy       → FrameSnapshot.energy  (kcal/mol, if present)
 *   XYZFrame.comment      → FrameSnapshot.status_message
 *
 * Bonds are not carried because XYZFrame does not store topology.
 * Bond inference (if needed) is the caller's responsibility.
 */
inline FrameSnapshot xyz_frame_to_snapshot(const io::XYZFrame& frame) {
	FrameSnapshot snap;
	snap.positions.reserve(frame.atoms.size());
	snap.atomic_numbers.reserve(frame.atoms.size());

	for (const auto& a : frame.atoms) {
		snap.positions.emplace_back(
			static_cast<float>(a.x),
			static_cast<float>(a.y),
			static_cast<float>(a.z));
		snap.atomic_numbers.push_back(a.Z);
	}
/** XYZVec 3 direct mapping 
**/
	if (frame.energy.has_value())
		snap.energy = *frame.energy;

	snap.status_message = frame.comment;

	return snap;
}

} // namespace vis
} // namespace vsepr
