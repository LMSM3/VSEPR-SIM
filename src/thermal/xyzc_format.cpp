#include "xyzc_format.h"
#include <fstream>
#include <sstream>

namespace vsepr {
namespace thermal {

bool read_xyzc(const std::string& filename, XYZCTrajectory& trajectory) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    trajectory.frames.clear();
    
    while (file) {
        int num_atoms = 0;
        file >> num_atoms;
        if (!file || num_atoms <= 0) {
            break;
        }
        file.ignore(1024, '\n');
        
        XYZCFrame frame;
        std::getline(file, frame.comment);
        
        frame.atoms.reserve(num_atoms);
        
        for (int i = 0; i < num_atoms; ++i) {
            XYZCAtom atom;
            file >> atom.element 
                 >> atom.position.x 
                 >> atom.position.y 
                 >> atom.position.z
                 >> atom.energy_transfer;
            
            if (!file) {
                return false;
            }
            frame.atoms.push_back(atom);
        }
        
        trajectory.frames.push_back(frame);
    }
    
    return !trajectory.frames.empty();
}

bool write_xyzc(const std::string& filename, const XYZCTrajectory& trajectory) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& frame : trajectory.frames) {
        file << frame.atoms.size() << "\n";
        file << frame.comment << "\n";
        
        for (const auto& atom : frame.atoms) {
            file << atom.element << " "
                 << atom.position.x << " "
                 << atom.position.y << " "
                 << atom.position.z << " "
                 << atom.energy_transfer << "\n";
        }
    }
    
    return true;
}

bool append_xyzc_frame(const std::string& filename, const XYZCFrame& frame) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        return false;
    }
    
    file << frame.atoms.size() << "\n";
    file << frame.comment << "\n";
    
    for (const auto& atom : frame.atoms) {
        file << atom.element << " "
             << atom.position.x << " "
             << atom.position.y << " "
             << atom.position.z << " "
             << atom.energy_transfer << "\n";
    }
    
    return true;
}

} // namespace thermal
} // namespace vsepr
