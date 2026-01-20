#include "xyz_format.h"
#include <fstream>
#include <sstream>

namespace vsepr {
namespace io {

bool read_xyz(const std::string& filename, XYZMolecule& molecule) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    int num_atoms = 0;
    file >> num_atoms;
    file.ignore(1024, '\n');
    
    std::getline(file, molecule.comment);
    
    molecule.atoms.clear();
    molecule.atoms.reserve(num_atoms);
    
    for (int i = 0; i < num_atoms; ++i) {
        XYZAtom atom;
        file >> atom.element >> atom.position.x >> atom.position.y >> atom.position.z;
        if (!file) {
            return false;
        }
        molecule.atoms.push_back(atom);
    }
    
    return true;
}

bool write_xyz(const std::string& filename, const XYZMolecule& molecule) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << molecule.atoms.size() << "\n";
    file << molecule.comment << "\n";
    
    for (const auto& atom : molecule.atoms) {
        file << atom.element << " " 
             << atom.position.x << " " 
             << atom.position.y << " " 
             << atom.position.z << "\n";
    }
    
    return true;
}

} // namespace io
} // namespace vsepr
