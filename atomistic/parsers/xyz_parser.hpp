#pragma once
#include "../core/state.hpp"
#include "io/xyz_format.hpp"
#include "pot/periodic_db.hpp"
#include <string>

namespace atomistic {
namespace parsers {

// Convert vsepr::io::XYZMolecule → atomistic::State
State from_xyz(const vsepr::io::XYZMolecule& mol);

// Convert vsepr::io::XYZMolecule with charges/velocities → atomistic::State
State from_xyza(const vsepr::io::XYZMolecule& mol);

} // namespace parsers
} // namespace atomistic
