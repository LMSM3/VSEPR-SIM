#pragma once
#include "../core/state.hpp"
#include "io/xyz_format.hpp"
#include <string>

namespace atomistic {
namespace compilers {

// Convert atomistic::State → vsepr::io::XYZMolecule (basic XYZ)
vsepr::io::XYZMolecule to_xyz(const State& s, const std::vector<std::string>& element_names);

// Save State as .xyzA with energy metadata
bool save_xyza(const std::string& filename, const State& s, const std::vector<std::string>& element_names);

// Template state (centroid + covariance)
struct TemplateState {
    uint32_t N{};
    std::vector<Vec3> centroid;           // mean positions
    std::vector<double> variance;         // per-atom positional variance
    EnergyTerms energy_mean;
    EnergyTerms energy_variance;
    uint32_t num_samples{};               // how many states averaged
};

// Save template as .xyzS (centroid + variance)
bool save_template(const std::string& filename, const TemplateState& tmpl, const std::vector<std::string>& element_names);

} // namespace compilers
} // namespace atomistic
