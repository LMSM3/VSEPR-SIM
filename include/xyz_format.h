#pragma once

#include <string>
#include <vector>

namespace vsepr {
namespace io {

struct Vec3 {
    double x, y, z;
};

struct XYZAtom {
    std::string element;
    Vec3 position;
};

struct XYZMolecule {
    std::vector<XYZAtom> atoms;
    std::string comment;
};

// Read XYZ file
bool read_xyz(const std::string& filename, XYZMolecule& molecule);

// Write XYZ file
bool write_xyz(const std::string& filename, const XYZMolecule& molecule);

} // namespace io
} // namespace vsepr
