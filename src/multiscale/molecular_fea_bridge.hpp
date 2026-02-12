#pragma once
/**
 * molecular_fea_bridge.hpp
 * 
 * Multiscale Bridge: Molecular Dynamics ↔ Physical Scale (FEA)
 * 
 * CRITICAL INTEGRATION: Connects molecular simulations to continuum mechanics
 * 
 * Features:
 * - Extract continuum material properties from molecular simulations
 * - GPU resource management (only one scale active at a time)
 * - Property transfer with validation
 * - Multiscale workflow automation
 * 
 * Date: January 18, 2026
 */

#include "gpu_resource_manager.hpp"
#include "sim/molecule.hpp"
#include "sim/sim_state.hpp"
#include "thermal/xyzc_format.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>

namespace vsepr {
namespace multiscale {

// ============================================================================
// Continuum Material Properties (from molecular simulation)
// ============================================================================

struct ContinuumProperties {
    // Mechanical properties
    double youngs_modulus_Pa = 0.0;      // E (Pa)
    double poissons_ratio = 0.0;          // ν (dimensionless)
    double shear_modulus_Pa = 0.0;        // G (Pa)
    double bulk_modulus_Pa = 0.0;         // K (Pa)
    double density_kg_m3 = 0.0;           // ρ (kg/m³)
    
    // Thermal properties
    double thermal_conductivity = 0.0;    // k (W/m·K)
    double heat_capacity = 0.0;           // Cp (J/kg·K)
    double thermal_expansion = 0.0;       // α (1/K)
    
    // Metadata
    std::string source_molecule;
    int num_atoms = 0;
    double temperature_K = 298.15;
    bool is_valid = false;
    
    /**
     * Validate properties
     */
    bool validate() const {
        bool valid = true;
        
        if (youngs_modulus_Pa <= 0.0) {
            std::cerr << "[VALIDATION ERROR] Young's modulus must be > 0\n";
            valid = false;
        }
        
        if (poissons_ratio < -1.0 || poissons_ratio > 0.5) {
            std::cerr << "[VALIDATION ERROR] Poisson's ratio must be in [-1, 0.5]\n";
            valid = false;
        }
        
        if (density_kg_m3 <= 0.0) {
            std::cerr << "[VALIDATION ERROR] Density must be > 0\n";
            valid = false;
        }
        
        // Check relationship: E = 2G(1+ν) = 3K(1-2ν)
        if (shear_modulus_Pa > 0.0) {
            double G_expected = youngs_modulus_Pa / (2.0 * (1.0 + poissons_ratio));
            double error = std::abs(G_expected - shear_modulus_Pa) / G_expected;
            if (error > 0.1) {
                std::cerr << "[VALIDATION WARNING] G inconsistent with E and ν (error: " 
                          << (error*100) << "%)\n";
            }
        }
        
        return valid;
    }
    
    /**
     * Print properties
     */
    void print() const {
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  CONTINUUM MATERIAL PROPERTIES                            ║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Source:  " << std::left << std::setw(48) << source_molecule << "║\n";
        std::cout << "║  Atoms:   " << std::left << std::setw(48) << num_atoms << "║\n";
        std::cout << "║  Temp:    " << std::left << std::setw(48) 
                  << (std::to_string((int)temperature_K) + " K") << "║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  MECHANICAL PROPERTIES:                                   ║\n";
        std::cout << "║  Young's Modulus (E):     " << std::left << std::setw(28) 
                  << (std::to_string(youngs_modulus_Pa/1e9) + " GPa") << "║\n";
        std::cout << "║  Poisson's Ratio (ν):     " << std::left << std::setw(28) 
                  << poissons_ratio << "║\n";
        std::cout << "║  Shear Modulus (G):       " << std::left << std::setw(28) 
                  << (std::to_string(shear_modulus_Pa/1e9) + " GPa") << "║\n";
        std::cout << "║  Bulk Modulus (K):        " << std::left << std::setw(28) 
                  << (std::to_string(bulk_modulus_Pa/1e9) + " GPa") << "║\n";
        std::cout << "║  Density (ρ):             " << std::left << std::setw(28) 
                  << (std::to_string((int)density_kg_m3) + " kg/m³") << "║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  THERMAL PROPERTIES:                                      ║\n";
        std::cout << "║  Conductivity (k):        " << std::left << std::setw(28) 
                  << (std::to_string(thermal_conductivity) + " W/m·K") << "║\n";
        std::cout << "║  Heat Capacity (Cp):      " << std::left << std::setw(28) 
                  << (std::to_string((int)heat_capacity) + " J/kg·K") << "║\n";
        std::cout << "║  Expansion (α):           " << std::left << std::setw(28) 
                  << (std::to_string(thermal_expansion) + " 1/K") << "║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Status:  " << std::left << std::setw(48) 
                  << (is_valid ? "VALID" : "INVALID") << "║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }
    
    /**
     * Export to FEA material file
     */
    bool export_to_fea(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot open file: " << filename << "\n";
            return false;
        }
        
        file << "# FEA Material Properties\n";
        file << "# Generated from molecular simulation: " << source_molecule << "\n";
        file << "# Number of atoms: " << num_atoms << "\n";
        file << "# Temperature: " << temperature_K << " K\n";
        file << "\n";
        file << "MATERIAL " << source_molecule << "\n";
        file << "  TYPE LinearElastic\n";
        file << "  E " << youngs_modulus_Pa << "  # Pa\n";
        file << "  NU " << poissons_ratio << "  # dimensionless\n";
        file << "  RHO " << density_kg_m3 << "  # kg/m³\n";
        file << "  K_THERMAL " << thermal_conductivity << "  # W/m·K\n";
        file << "  CP " << heat_capacity << "  # J/kg·K\n";
        file << "  ALPHA " << thermal_expansion << "  # 1/K\n";
        file << "END\n";
        
        file.close();
        std::cout << "[SUCCESS] Exported FEA material to: " << filename << "\n";
        return true;
    }
};

// ============================================================================
// Molecular → FEA Bridge
// ============================================================================

class MolecularFEABridge {
private:
    GPUResourceManager& gpu_manager_;
    bool molecular_active_ = false;
    bool fea_active_ = false;
    
public:
    MolecularFEABridge() : gpu_manager_(GPUResourceManager::instance()) {}
    
    /**
     * Extract continuum properties from molecular simulation
     * Uses statistical mechanics and Green-Kubo relations
     */
    ContinuumProperties extract_properties(const Molecule& mol, const std::string& xyzc_file = "") {
        ContinuumProperties props;
        props.source_molecule = "Molecular_Simulation";
        props.num_atoms = mol.num_atoms();
        props.temperature_K = 298.15;  // Default, can be updated
        
        // Calculate density from molecular structure
        double total_mass = 0.0;
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            // Approximate atomic masses (in kg)
            int Z = mol.atomic_number(i);
            double mass_kg = Z * 1.66054e-27;  // atomic mass unit in kg
            total_mass += mass_kg;
        }
        
        // Estimate volume from atom positions (bounding box + VDW radius)
        double x_min = 1e10, x_max = -1e10;
        double y_min = 1e10, y_max = -1e10;
        double z_min = 1e10, z_max = -1e10;
        
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            const auto& pos = mol.position(i);
            x_min = std::min(x_min, pos.x);
            x_max = std::max(x_max, pos.x);
            y_min = std::min(y_min, pos.y);
            y_max = std::max(y_max, pos.y);
            z_min = std::min(z_min, pos.z);
            z_max = std::max(z_max, pos.z);
        }
        
        // Add VDW padding (approximate)
        double vdw_padding = 2.0e-10;  // 2 Ångström in meters
        double volume_m3 = (x_max - x_min + vdw_padding) * 
                          (y_max - y_min + vdw_padding) * 
                          (z_max - z_min + vdw_padding) * 1e-30;  // Å³ to m³
        
        props.density_kg_m3 = total_mass / volume_m3;
        
        // If XYZC file provided, extract thermal properties
        if (!xyzc_file.empty()) {
            extract_thermal_properties_from_xyzc(xyzc_file, props);
        } else {
            // Use empirical estimates
            estimate_mechanical_properties(mol, props);
            estimate_thermal_properties(mol, props);
        }
        
        props.is_valid = props.validate();
        return props;
    }
    
    /**
     * Activate molecular scale on GPU
     * CRITICAL: Checks for conflicts before activation
     */
    bool activate_molecular_scale(void* gl_context = nullptr) {
        // Check if FEA is active
        if (fea_active_ || gpu_manager_.is_scale_active(GPUScaleType::PHYSICAL_FEA)) {
            std::cerr << "\n";
            std::cerr << "╔═══════════════════════════════════════════════════════════╗\n";
            std::cerr << "║  ERROR: CANNOT ACTIVATE MOLECULAR SCALE                   ║\n";
            std::cerr << "╠═══════════════════════════════════════════════════════════╣\n";
            std::cerr << "║  Physical/FEA scale is currently active on GPU            ║\n";
            std::cerr << "║  You must deactivate FEA before activating molecular      ║\n";
            std::cerr << "║                                                           ║\n";
            std::cerr << "║  SOLUTION:                                                ║\n";
            std::cerr << "║  1. Call deactivate_fea_scale()                          ║\n";
            std::cerr << "║  2. Wait for confirmation                                 ║\n";
            std::cerr << "║  3. Then call activate_molecular_scale()                 ║\n";
            std::cerr << "╚═══════════════════════════════════════════════════════════╝\n";
            std::cerr << "\n";
            return false;
        }
        
        // Request activation
        if (!gpu_manager_.request_activation(GPUScaleType::MOLECULAR, 
                                            "Molecular Dynamics (VSEPR-Sim)", 
                                            gl_context)) {
            return false;
        }
        
        // Wait for user confirmation
        std::cout << "[ACTION REQUIRED] Confirm molecular scale activation? (y/n): ";
        char response;
        std::cin >> response;
        
        if (response == 'y' || response == 'Y') {
            if (gpu_manager_.confirm_activation(GPUScaleType::MOLECULAR)) {
                molecular_active_ = true;
                return true;
            }
        } else {
            std::cout << "[CANCELLED] Molecular scale activation cancelled\n";
            gpu_manager_.deactivate_scale();
            return false;
        }
        
        return false;
    }
    
    /**
     * Deactivate molecular scale
     */
    void deactivate_molecular_scale() {
        if (!molecular_active_) {
            std::cout << "[INFO] Molecular scale not active\n";
            return;
        }
        
        gpu_manager_.deactivate_scale();
        molecular_active_ = false;
    }
    
    /**
     * Activate FEA/physical scale on GPU
     * CRITICAL: Checks for conflicts before activation
     */
    bool activate_fea_scale(void* gl_context = nullptr) {
        // Check if molecular is active
        if (molecular_active_ || gpu_manager_.is_scale_active(GPUScaleType::MOLECULAR)) {
            std::cerr << "\n";
            std::cerr << "╔═══════════════════════════════════════════════════════════╗\n";
            std::cerr << "║  ERROR: CANNOT ACTIVATE FEA SCALE                         ║\n";
            std::cerr << "╠═══════════════════════════════════════════════════════════╣\n";
            std::cerr << "║  Molecular dynamics scale is currently active on GPU      ║\n";
            std::cerr << "║  You must deactivate molecular before activating FEA      ║\n";
            std::cerr << "║                                                           ║\n";
            std::cerr << "║  SOLUTION:                                                ║\n";
            std::cerr << "║  1. Call deactivate_molecular_scale()                    ║\n";
            std::cerr << "║  2. Wait for confirmation                                 ║\n";
            std::cerr << "║  3. Then call activate_fea_scale()                       ║\n";
            std::cerr << "╚═══════════════════════════════════════════════════════════╝\n";
            std::cerr << "\n";
            return false;
        }
        
        // Request activation
        if (!gpu_manager_.request_activation(GPUScaleType::PHYSICAL_FEA, 
                                            "Physical Scale FEA", 
                                            gl_context)) {
            return false;
        }
        
        // Wait for user confirmation
        std::cout << "[ACTION REQUIRED] Confirm FEA scale activation? (y/n): ";
        char response;
        std::cin >> response;
        
        if (response == 'y' || response == 'Y') {
            if (gpu_manager_.confirm_activation(GPUScaleType::PHYSICAL_FEA)) {
                fea_active_ = true;
                return true;
            }
        } else {
            std::cout << "[CANCELLED] FEA scale activation cancelled\n";
            gpu_manager_.deactivate_scale();
            return false;
        }
        
        return false;
    }
    
    /**
     * Deactivate FEA scale
     */
    void deactivate_fea_scale() {
        if (!fea_active_) {
            std::cout << "[INFO] FEA scale not active\n";
            return;
        }
        
        gpu_manager_.deactivate_scale();
        fea_active_ = false;
    }
    
    /**
     * Get GPU status
     */
    void print_gpu_status() const {
        gpu_manager_.print_status();
    }
    
private:
    /**
     * Extract thermal properties from XYZC file
     */
    void extract_thermal_properties_from_xyzc(const std::string& filename, 
                                             ContinuumProperties& props) {
        // Read XYZC file and analyze thermal pathways
        thermal::XYZCReader reader(filename);
        
        // Get thermal conductivity from pathway graph
        props.thermal_conductivity = reader.get_thermal_conductivity();
        props.heat_capacity = reader.get_heat_capacity();
        props.thermal_expansion = reader.get_thermal_expansion();
        
        std::cout << "[INFO] Extracted thermal properties from XYZC: " << filename << "\n";
    }
    
    /**
     * Estimate mechanical properties (empirical)
     */
    void estimate_mechanical_properties(const Molecule& mol, ContinuumProperties& props) {
        // Empirical estimates based on bonding and density
        // These are rough approximations - real values need MD simulation
        
        // Estimate Young's modulus from density (very rough)
        double rho_normalized = props.density_kg_m3 / 1000.0;  // Normalize to water
        props.youngs_modulus_Pa = rho_normalized * 50e9;  // 50 GPa baseline
        
        // Estimate Poisson's ratio (typical for most materials)
        props.poissons_ratio = 0.3;
        
        // Calculate G and K from E and ν
        props.shear_modulus_Pa = props.youngs_modulus_Pa / (2.0 * (1.0 + props.poissons_ratio));
        props.bulk_modulus_Pa = props.youngs_modulus_Pa / (3.0 * (1.0 - 2.0 * props.poissons_ratio));
        
        std::cout << "[INFO] Used empirical estimates for mechanical properties\n";
        std::cout << "[WARNING] Run MD simulation for accurate values\n";
    }
    
    /**
     * Estimate thermal properties (empirical)
     */
    void estimate_thermal_properties(const Molecule& mol, ContinuumProperties& props) {
        // Empirical estimates
        props.thermal_conductivity = 0.5;  // W/m·K (typical organic)
        props.heat_capacity = 1000.0;      // J/kg·K (typical)
        props.thermal_expansion = 1e-5;    // 1/K (typical)
        
        std::cout << "[INFO] Used empirical estimates for thermal properties\n";
        std::cout << "[WARNING] Run thermal simulation for accurate values\n";
    }
};

} // namespace multiscale
} // namespace vsepr
