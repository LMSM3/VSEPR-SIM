#pragma once

#include <string>
#include <vector>

namespace vsepr {
namespace thermal {

struct Vec3 {
    double x, y, z;
};

struct XYZCAtom {
    std::string element;
    Vec3 position;
    double energy_transfer = 0.0; // Energy transfer in this frame (C component)
};

struct XYZCFrame {
    std::vector<XYZCAtom> atoms;
    std::string comment;
    double total_energy = 0.0;
    double temperature = 0.0;
};

struct XYZCTrajectory {
    std::vector<XYZCFrame> frames;
};

// Read XYZC file (thermal trajectory with energy transfer data)
bool read_xyzc(const std::string& filename, XYZCTrajectory& trajectory);

// Write XYZC file
bool write_xyzc(const std::string& filename, const XYZCTrajectory& trajectory);

// Append frame to XYZC file
bool append_xyzc_frame(const std::string& filename, const XYZCFrame& frame);

} // namespace thermal
} // namespace vsepr
