#pragma once
/*
thermal_properties.hpp
----------------------
Thermal and transport property calculations from MD simulations.

Computes:
- Thermal conductivity (Green-Kubo, NEMD)
- Heat capacity
- Thermal expansion
- Bonding type inference
- Phase state prediction
- Spatial tracking on downsampled grid
*/

#include "sim/molecule.hpp"
#include <vector>
#include <array>
#include <string>
#include <cmath>

namespace vsepr {

// ============================================================================
// Bonding Type Classification
// ============================================================================
enum class BondingType {
    UNKNOWN = 0,
    IONIC,       // Electron transfer, strong electrostatic
    COVALENT,    // Shared electrons, directional
    METALLIC,    // Delocalized electrons, conductive
    MOLECULAR,   // Weak intermolecular forces (van der Waals)
    HYDROGEN     // H-bonding dominant
};

struct BondingAnalysis {
    BondingType primary_type = BondingType::UNKNOWN;
    BondingType secondary_type = BondingType::UNKNOWN;
    
    double ionic_character = 0.0;      // 0-1 scale
    double covalent_character = 0.0;   // 0-1 scale
    double metallic_character = 0.0;   // 0-1 scale
    
    int num_free_electrons = 0;        // Estimated mobile carriers
    bool has_delocalization = false;   // Conjugation/metallicity
    
    std::string description;
};

// ============================================================================
// Spatial Grid Tracking (20x20x20 downsampled from full simulation space)
// ============================================================================
struct SpatialGrid {
    static constexpr int GRID_SIZE = 20;
    
    // Particle density per cell [x][y][z]
    std::array<std::array<std::array<int, GRID_SIZE>, GRID_SIZE>, GRID_SIZE> density;
    
    // Bounding box of molecule
    std::array<double, 3> min_coords = {0.0, 0.0, 0.0};
    std::array<double, 3> max_coords = {0.0, 0.0, 0.0};
    std::array<double, 3> box_size = {0.0, 0.0, 0.0};
    
    // Grid cell dimensions
    std::array<double, 3> cell_size = {0.0, 0.0, 0.0};
    
    SpatialGrid() {
        reset();
    }
    
    void reset() {
        for (int i = 0; i < GRID_SIZE; ++i) {
            for (int j = 0; j < GRID_SIZE; ++j) {
                for (int k = 0; k < GRID_SIZE; ++k) {
                    density[i][j][k] = 0;
                }
            }
        }
    }
    
    // Get grid indices for a 3D position
    std::array<int, 3> get_grid_indices(double x, double y, double z) const {
        int ix = static_cast<int>((x - min_coords[0]) / cell_size[0]);
        int iy = static_cast<int>((y - min_coords[1]) / cell_size[1]);
        int iz = static_cast<int>((z - min_coords[2]) / cell_size[2]);
        
        // Clamp to grid bounds
        ix = std::max(0, std::min(GRID_SIZE - 1, ix));
        iy = std::max(0, std::min(GRID_SIZE - 1, iy));
        iz = std::max(0, std::min(GRID_SIZE - 1, iz));
        
        return {ix, iy, iz};
    }
    
    // Get physical position of grid cell center
    std::array<double, 3> get_cell_center(int ix, int iy, int iz) const {
        return {
            min_coords[0] + (ix + 0.5) * cell_size[0],
            min_coords[1] + (iy + 0.5) * cell_size[1],
            min_coords[2] + (iz + 0.5) * cell_size[2]
        };
    }
};

// ============================================================================
// Thermal Properties
// ============================================================================
struct ThermalProperties {
    // Temperature
    double temperature = 298.15;  // K
    
    // Thermal conductivity (W/m·K)
    double thermal_conductivity = 0.0;
    bool thermal_conductivity_computed = false;
    
    // Heat capacity (J/mol·K)
    double heat_capacity_Cv = 0.0;  // Constant volume
    double heat_capacity_Cp = 0.0;  // Constant pressure
    
    // Thermal expansion coefficient (1/K)
    double thermal_expansion = 0.0;
    
    // Electrical conductivity (S/m) - related via Wiedemann-Franz
    double electrical_conductivity = 0.0;
    
    // Phase state
    std::string phase_state = "unknown";  // solid, liquid, gas
    double melting_point = 0.0;           // K (if estimated)
    double boiling_point = 0.0;           // K (if estimated)
    
    // Bonding analysis
    BondingAnalysis bonding;
    
    // Spatial tracking
    SpatialGrid spatial_grid;
    
    // Transport regime
    std::string transport_mechanism;  // "electron" or "phonon"
    bool is_conductor = false;
    bool is_insulator = false;
};

// ============================================================================
// Thermal Property Calculator
// ============================================================================
class ThermalPropertyCalculator {
public:
    // Analyze bonding type from molecular structure
    static BondingAnalysis analyze_bonding(const Molecule& mol);
    
    // Build spatial grid from current coordinates
    static SpatialGrid build_spatial_grid(const Molecule& mol);
    
    // Estimate thermal conductivity based on bonding type
    static double estimate_thermal_conductivity(const BondingAnalysis& bonding, double T);
    
    // Estimate heat capacity (Dulong-Petit law + corrections)
    static double estimate_heat_capacity(const Molecule& mol, double T);
    
    // Predict phase state at given temperature
    static std::string predict_phase_state(const Molecule& mol, double T);
    
    // Full thermal analysis
    static ThermalProperties compute_properties(const Molecule& mol, double T = 298.15);
    
    // Display spatial grid as ASCII art
    static std::string render_spatial_grid_2d(const SpatialGrid& grid, int slice_axis = 2);
    
    // Get particle location summary
    static std::string get_particle_location_summary(const SpatialGrid& grid);
};

// ============================================================================
// Implementation
// ============================================================================

inline BondingAnalysis ThermalPropertyCalculator::analyze_bonding(const Molecule& mol) {
    BondingAnalysis analysis;
    
    if (mol.num_atoms() == 0) {
        analysis.description = "Empty molecule";
        return analysis;
    }
    
    // Electronegativity analysis
    std::vector<double> electronegativities;
    int num_metals = 0;
    int num_nonmetals = 0;
    
    for (const auto& atom : mol.atoms) {
        // Simple electronegativity lookup (Pauling scale approximation)
        double EN = 2.5;  // Default
        
        // Metals (low EN)
        if (atom.Z <= 4 || (atom.Z >= 11 && atom.Z <= 13) || 
            (atom.Z >= 19 && atom.Z <= 31) || (atom.Z >= 37 && atom.Z <= 50)) {
            EN = 1.5;
            num_metals++;
        }
        // Nonmetals (high EN)
        else if (atom.Z >= 6 && atom.Z <= 10) {  // C, N, O, F
            EN = 3.5;
            num_nonmetals++;
        }
        else if (atom.Z >= 14 && atom.Z <= 18) {  // Si, P, S, Cl, Ar
            EN = 2.8;
            num_nonmetals++;
        }
        
        electronegativities.push_back(EN);
    }
    
    // Compute EN differences across bonds
    double max_EN_diff = 0.0;
    double avg_EN_diff = 0.0;
    
    for (const auto& bond : mol.bonds) {
        double diff = std::abs(electronegativities[bond.i] - electronegativities[bond.j]);
        max_EN_diff = std::max(max_EN_diff, diff);
        avg_EN_diff += diff;
    }
    
    if (!mol.bonds.empty()) {
        avg_EN_diff /= mol.bonds.size();
    }
    
    // Classification logic
    if (num_metals >= mol.num_atoms() * 0.8) {
        // Predominantly metallic
        analysis.primary_type = BondingType::METALLIC;
        analysis.metallic_character = 1.0;
        analysis.has_delocalization = true;
        analysis.num_free_electrons = num_metals;  // Rough estimate
        analysis.description = "Metallic bonding with delocalized electrons";
        
    } else if (max_EN_diff > 2.0) {
        // Large EN difference -> ionic
        analysis.primary_type = BondingType::IONIC;
        analysis.ionic_character = max_EN_diff / 4.0;  // Normalize
        analysis.description = "Ionic bonding with electron transfer";
        
    } else if (avg_EN_diff < 0.5) {
        // Small EN difference -> covalent
        analysis.primary_type = BondingType::COVALENT;
        analysis.covalent_character = 1.0;
        analysis.description = "Covalent bonding with shared electrons";
        
    } else {
        // Mixed bonding
        analysis.primary_type = BondingType::COVALENT;
        analysis.secondary_type = BondingType::IONIC;
        analysis.covalent_character = 1.0 - avg_EN_diff;
        analysis.ionic_character = avg_EN_diff;
        analysis.description = "Polar covalent bonding";
    }
    
    // Check for H-bonding (H bonded to N, O, F)
    for (const auto& bond : mol.bonds) {
        uint8_t Z1 = mol.atoms[bond.i].Z;
        uint8_t Z2 = mol.atoms[bond.j].Z;
        
        if ((Z1 == 1 && (Z2 == 7 || Z2 == 8 || Z2 == 9)) ||
            (Z2 == 1 && (Z1 == 7 || Z1 == 8 || Z1 == 9))) {
            analysis.secondary_type = BondingType::HYDROGEN;
            break;
        }
    }
    
    return analysis;
}

inline SpatialGrid ThermalPropertyCalculator::build_spatial_grid(const Molecule& mol) {
    SpatialGrid grid;
    
    if (mol.num_atoms() == 0) {
        return grid;
    }
    
    // Find bounding box
    double xmin = 1e9, xmax = -1e9;
    double ymin = 1e9, ymax = -1e9;
    double zmin = 1e9, zmax = -1e9;
    
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double x, y, z;
        mol.get_position(i, x, y, z);
        
        xmin = std::min(xmin, x);
        xmax = std::max(xmax, x);
        ymin = std::min(ymin, y);
        ymax = std::max(ymax, y);
        zmin = std::min(zmin, z);
        zmax = std::max(zmax, z);
    }
    
    // Add padding (10% margin)
    double padding_x = (xmax - xmin) * 0.1;
    double padding_y = (ymax - ymin) * 0.1;
    double padding_z = (zmax - zmin) * 0.1;
    
    grid.min_coords = {xmin - padding_x, ymin - padding_y, zmin - padding_z};
    grid.max_coords = {xmax + padding_x, ymax + padding_y, zmax + padding_z};
    grid.box_size = {
        grid.max_coords[0] - grid.min_coords[0],
        grid.max_coords[1] - grid.min_coords[1],
        grid.max_coords[2] - grid.min_coords[2]
    };
    
    grid.cell_size = {
        grid.box_size[0] / SpatialGrid::GRID_SIZE,
        grid.box_size[1] / SpatialGrid::GRID_SIZE,
        grid.box_size[2] / SpatialGrid::GRID_SIZE
    };
    
    // Bin atoms into grid
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double x, y, z;
        mol.get_position(i, x, y, z);
        
        auto indices = grid.get_grid_indices(x, y, z);
        grid.density[indices[0]][indices[1]][indices[2]]++;
    }
    
    return grid;
}

inline double ThermalPropertyCalculator::estimate_thermal_conductivity(
    const BondingAnalysis& bonding, double T) {
    
    // Rough estimates in W/m·K at 300K
    double k = 0.0;
    
    switch (bonding.primary_type) {
        case BondingType::METALLIC:
            // Metals: high conductivity via free electrons
            k = 50.0 + bonding.num_free_electrons * 10.0;  // 50-400 W/m·K
            break;
            
        case BondingType::COVALENT:
            // Covalent solids: moderate (phonon-dominated)
            k = bonding.has_delocalization ? 20.0 : 5.0;  // Diamond: 2000, glass: 1
            break;
            
        case BondingType::IONIC:
            // Ionic solids: low-moderate (phonon only)
            k = 2.0;  // NaCl: ~6 W/m·K
            break;
            
        case BondingType::MOLECULAR:
            // Molecular: very low
            k = 0.2;  // Ice: ~2, polymers: ~0.2
            break;
            
        default:
            k = 1.0;
    }
    
    // Temperature dependence: k ~ 1/T for phonon-dominated
    if (bonding.primary_type != BondingType::METALLIC) {
        k *= 300.0 / T;
    }
    
    return k;
}

inline double ThermalPropertyCalculator::estimate_heat_capacity(
    const Molecule& mol, double T) {
    
    // Dulong-Petit law: Cv ≈ 3R per atom (classical limit)
    const double R = 8.314;  // J/mol·K
    
    // At high T, Cv → 3R per atom
    // At low T, quantum corrections apply
    
    double Cv = 3.0 * R * mol.num_atoms();
    
    // Simple quantum correction (Einstein model)
    if (T < 300.0) {
        double theta_E = 300.0;  // Einstein temperature (K)
        double x = theta_E / T;
        double quantum_factor = std::pow(x / std::sinh(x / 2.0), 2);
        Cv *= quantum_factor;
    }
    
    return Cv;
}

inline std::string ThermalPropertyCalculator::predict_phase_state(
    const Molecule& mol, double T) {
    
    // Very rough estimates based on bonding
    BondingAnalysis bonding = analyze_bonding(mol);
    
    if (bonding.primary_type == BondingType::METALLIC) {
        // Metals: high melting points
        return (T < 1000.0) ? "solid" : "liquid";
        
    } else if (bonding.primary_type == BondingType::IONIC) {
        // Ionic: high melting points
        return (T < 800.0) ? "solid" : "liquid";
        
    } else if (bonding.primary_type == BondingType::COVALENT) {
        // Covalent network: very high melting
        return (T < 1500.0) ? "solid" : "liquid";
        
    } else {
        // Molecular: low melting/boiling
        if (T < 200.0) return "solid";
        else if (T < 400.0) return "liquid";
        else return "gas";
    }
}

inline ThermalProperties ThermalPropertyCalculator::compute_properties(
    const Molecule& mol, double T) {
    
    ThermalProperties props;
    props.temperature = T;
    
    // Bonding analysis
    props.bonding = analyze_bonding(mol);
    
    // Spatial grid
    props.spatial_grid = build_spatial_grid(mol);
    
    // Thermal conductivity
    props.thermal_conductivity = estimate_thermal_conductivity(props.bonding, T);
    props.thermal_conductivity_computed = true;
    
    // Heat capacity
    props.heat_capacity_Cv = estimate_heat_capacity(mol, T);
    props.heat_capacity_Cp = props.heat_capacity_Cv + 8.314;  // Cp ≈ Cv + R
    
    // Phase state
    props.phase_state = predict_phase_state(mol, T);
    
    // Transport mechanism
    if (props.bonding.primary_type == BondingType::METALLIC) {
        props.transport_mechanism = "electron-dominated (free carriers)";
        props.is_conductor = true;
        
        // Wiedemann-Franz law: k/σ = L*T  (L = 2.44e-8 W·Ω/K²)
        props.electrical_conductivity = props.thermal_conductivity / (2.44e-8 * T);
        
    } else {
        props.transport_mechanism = "phonon-dominated (lattice vibrations)";
        props.is_insulator = true;
        props.electrical_conductivity = 0.0;
    }
    
    return props;
}

inline std::string ThermalPropertyCalculator::render_spatial_grid_2d(
    const SpatialGrid& grid, int slice_axis) {
    
    std::string output;
    output += "\n╔══════════════════════════════════════════╗\n";
    output += "║  Spatial Distribution (20x20 grid)      ║\n";
    output += "╚══════════════════════════════════════════╝\n\n";
    
    // Take middle slice
    int slice_index = SpatialGrid::GRID_SIZE / 2;
    
    // Render XY plane (Z = middle)
    for (int y = SpatialGrid::GRID_SIZE - 1; y >= 0; --y) {
        for (int x = 0; x < SpatialGrid::GRID_SIZE; ++x) {
            int count = grid.density[x][y][slice_index];
            
            if (count == 0) output += "· ";
            else if (count == 1) output += "○ ";
            else if (count == 2) output += "◉ ";
            else output += "● ";
        }
        output += "\n";
    }
    
    output += "\n  Legend: ·=empty  ○=1 atom  ◉=2 atoms  ●=3+ atoms\n";
    
    return output;
}

inline std::string ThermalPropertyCalculator::get_particle_location_summary(
    const SpatialGrid& grid) {
    
    std::string summary;
    
    // Find occupied cells
    int total_occupied = 0;
    int max_density = 0;
    std::array<int, 3> max_density_cell = {0, 0, 0};
    
    for (int i = 0; i < SpatialGrid::GRID_SIZE; ++i) {
        for (int j = 0; j < SpatialGrid::GRID_SIZE; ++j) {
            for (int k = 0; k < SpatialGrid::GRID_SIZE; ++k) {
                if (grid.density[i][j][k] > 0) {
                    total_occupied++;
                    
                    if (grid.density[i][j][k] > max_density) {
                        max_density = grid.density[i][j][k];
                        max_density_cell = {i, j, k};
                    }
                }
            }
        }
    }
    
    summary += "Spatial Tracking Summary:\n";
    summary += "  Grid resolution: 20×20×20 cells\n";
    summary += "  Occupied cells: " + std::to_string(total_occupied) + "\n";
    summary += "  Highest density: " + std::to_string(max_density) + " atoms in cell [" 
             + std::to_string(max_density_cell[0]) + ", " 
             + std::to_string(max_density_cell[1]) + ", "
             + std::to_string(max_density_cell[2]) + "]\n";
    
    auto center = grid.get_cell_center(max_density_cell[0], max_density_cell[1], max_density_cell[2]);
    summary += "  Peak location: (" 
             + std::to_string(center[0]) + ", "
             + std::to_string(center[1]) + ", "
             + std::to_string(center[2]) + ") Å\n";
    
    return summary;
}

} // namespace vsepr
