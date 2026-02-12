#include "xyz_compiler.hpp"
#include "io/xyz_format.hpp"
#include <fstream>
#include <iomanip>

namespace atomistic {
namespace compilers {

vsepr::io::XYZMolecule to_xyz(const State& s, const std::vector<std::string>& element_names) {
    vsepr::io::XYZMolecule mol;
    mol.comment = "Generated from atomistic::State";
    
    for (uint32_t i = 0; i < s.N; ++i) {
        std::string elem = (i < element_names.size()) ? element_names[i] : "C";
        mol.atoms.emplace_back(elem, s.X[i].x, s.X[i].y, s.X[i].z);
    }

    for (const auto& edge : s.B) {
        mol.bonds.emplace_back(edge.i, edge.j, 1.0);
    }

    return mol;
}

bool save_xyza(const std::string& filename, const State& s, const std::vector<std::string>& element_names) {
    std::ofstream f(filename);
    if (!f.is_open()) return false;

    f << s.N << "\n";
    f << "E=" << std::setprecision(12) << s.E.total() 
      << " Ubond=" << s.E.Ubond 
      << " Uangle=" << s.E.Uangle
      << " Utors=" << s.E.Utors 
      << " UvdW=" << s.E.UvdW 
      << " UCoul=" << s.E.UCoul << "\n";

    for (uint32_t i = 0; i < s.N; ++i) {
        std::string elem = (i < element_names.size()) ? element_names[i] : "C";
        f << elem << " " 
          << std::setprecision(8) << s.X[i].x << " " << s.X[i].y << " " << s.X[i].z;
        
        // Add velocity/charge if present
        if (i < s.V.size() && (s.V[i].x != 0 || s.V[i].y != 0 || s.V[i].z != 0)) {
            f << " vx=" << s.V[i].x << " vy=" << s.V[i].y << " vz=" << s.V[i].z;
        }
        if (i < s.Q.size() && s.Q[i] != 0) {
            f << " q=" << s.Q[i];
        }
        f << "\n";
    }

    return true;
}

bool save_template(const std::string& filename, const TemplateState& tmpl, const std::vector<std::string>& element_names) {
    std::ofstream f(filename);
    if (!f.is_open()) return false;

    f << "# xyzS: Template State (centroid + variance)\n";
    f << "# N=" << tmpl.N << " samples=" << tmpl.num_samples << "\n";
    f << "# Energy: mean=" << tmpl.energy_mean.total() 
      << " var=" << tmpl.energy_variance.total() << "\n";
    
    f << tmpl.N << "\n";
    f << "Template centroid (samples=" << tmpl.num_samples << ")\n";

    for (uint32_t i = 0; i < tmpl.N; ++i) {
        std::string elem = (i < element_names.size()) ? element_names[i] : "C";
        f << elem << " "
          << std::setprecision(8) << tmpl.centroid[i].x << " " 
          << tmpl.centroid[i].y << " " << tmpl.centroid[i].z;
        
        if (i < tmpl.variance.size()) {
            f << " var=" << tmpl.variance[i];
        }
        f << "\n";
    }

    return true;
}

} // namespace compilers
} // namespace atomistic
