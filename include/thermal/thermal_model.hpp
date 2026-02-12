/**
 * Thermal Module V2.0.0
 * 
 * Responsibilities:
 * - Track temperature field over atoms/clusters
 * - Convert mechanical energy ⇄ thermal energy
 * - Support thermostats (NVT) and heat baths
 * - Provide observables: T, heat flux, energy budget
 * - Drive visualization: per-atom color, heatmaps
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace vsepr {
namespace thermal {

// Thermostat types
enum class Thermostat {
    Off,
    Berendsen,   // Cheap, stable, not rigorous
    Langevin     // Better physical story (experimental)
};

// Thermal parameters
struct ThermalParams {
    double T0;              // Target temperature (K)
    double tau;             // Thermostat relaxation time (fs)
    double dt;              // Timestep (fs)
    bool enabled;
    Thermostat thermo;
    double kB;              // Boltzmann constant (reduced units)
    
    ThermalParams() 
        : T0(300.0), tau(100.0), dt(1.0), enabled(false),
          thermo(Thermostat::Off), kB(1.0) {}
};

// Per-atom thermal properties
struct ThermalAtom {
    double Ci;      // Heat capacity (J/K or reduced units)
    double Ti;      // Temperature field (K or reduced)
    double gamma;   // Damping coefficient
    
    ThermalAtom() : Ci(1.0), Ti(300.0), gamma(0.1) {}
};

// Thermal conductance edge between atoms
struct ThermalEdge {
    uint32_t i, j;  // Atom indices
    double gij;     // Conductance (W/K or reduced)
    
    ThermalEdge(uint32_t i_, uint32_t j_, double g_)
        : i(i_), j(j_), gij(g_) {}
};

// Energy ledger for conservation tracking
struct ThermalLedger {
    double Ekin;      // Kinetic energy
    double Epot;      // Potential energy
    double Etherm;    // Thermal energy
    double Ebath;     // Energy removed/added by thermostat
    double Tglobal;   // Global temperature
    double Tmin, Tmax;// Temperature range
    
    ThermalLedger()
        : Ekin(0.0), Epot(0.0), Etherm(0.0), Ebath(0.0),
          Tglobal(0.0), Tmin(0.0), Tmax(0.0) {}
    
    double total_energy() const { return Ekin + Epot + Etherm; }
    double conservation_error() const { return total_energy() + Ebath; }
};

// Main thermal model class
class ThermalModel {
public:
    ThermalModel();
    
    // Initialize thermal state for N atoms
    void initialize(size_t num_atoms, double initial_T = 300.0);
    
    // Set parameters
    void set_params(const ThermalParams& params);
    const ThermalParams& get_params() const { return params_; }
    
    // Build thermal conductance graph from bond topology
    void build_conductance_graph(
        const std::vector<std::pair<uint32_t, uint32_t>>& bonds,
        const std::vector<double>& bond_orders,
        const std::vector<double>& distances
    );
    
    // Update step (call every MD step)
    void update(
        const std::vector<double>& masses,
        const std::vector<double>& velocities,  // 3N vector (vx, vy, vz per atom)
        double potential_energy,
        double dt
    );
    
    // Thermostat application
    void apply_thermostat(
        std::vector<double>& velocities,  // Modified in-place
        const std::vector<double>& masses
    );
    
    // Thermal diffusion step
    void diffuse_heat(double dt);
    
    // Compute dissipation power from damping
    void compute_dissipation(
        const std::vector<double>& velocities,
        const std::vector<double>& masses
    );
    
    // Get current state
    const ThermalLedger& get_ledger() const { return ledger_; }
    const std::vector<ThermalAtom>& get_atoms() const { return atoms_; }
    
    // Compute global kinetic temperature
    double compute_global_temperature(
        const std::vector<double>& masses,
        const std::vector<double>& velocities,
        int dof = -1  // Degrees of freedom (-1 = auto: 3N)
    ) const;
    
    // Compute local cluster temperature
    double compute_cluster_temperature(
        const std::vector<uint32_t>& atom_indices,
        const std::vector<double>& masses,
        const std::vector<double>& velocities
    ) const;
    
    // Export ledger to CSV/JSON
    void export_ledger(const std::string& filename, const std::string& format = "csv") const;
    
    // Heat flux along bond i-j
    double heat_flux(uint32_t i, uint32_t j) const;
    
    // Status output
    void print_status() const;
    void print_dashboard() const;
    
private:
    ThermalParams params_;
    std::vector<ThermalAtom> atoms_;
    std::vector<ThermalEdge> edges_;
    ThermalLedger ledger_;
    
    // Conductance computation
    double compute_conductance(double bond_order, double distance, int Zi, int Zj) const;
    
    // Berendsen thermostat implementation
    void apply_berendsen(std::vector<double>& velocities, const std::vector<double>& masses);
    
    // Langevin thermostat implementation (experimental)
    void apply_langevin(std::vector<double>& velocities, const std::vector<double>& masses);
};

// Visualization helper - map temperature to RGB color
struct ThermalColor {
    float r, g, b;
    
    static ThermalColor from_temperature(double T, double Tmin = 200.0, double Tmax = 600.0);
};

}} // namespace vsepr::thermal
