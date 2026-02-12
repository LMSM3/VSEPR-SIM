/*
cmd_therm.cpp
-------------
Thermal properties analysis and object generation command.

Usage:
  vsepr therm <input.xyz> [--temperature <T>] [--generate-object]

Features:
- Bonding type classification
- Thermal conductivity estimation
- Heat capacity calculation
- Spatial tracking on 20x20x20 grid
- Interactive object generation prompt

===============================================================================
MATHEMATICAL FOUNDATIONS - Thermal Property Calculations
===============================================================================

1. THERMAL CONDUCTIVITY (k)
   -------------------------
   Based on kinetic theory for gases and Debye model for solids:
   
   k = (1/3) * C_v * v_avg * lambda
   
   Where:
   - C_v     = Heat capacity at constant volume (J/mol·K)
   - v_avg   = Average molecular velocity = sqrt(8*k_B*T / (pi*m))
   - lambda  = Mean free path between collisions
   - k_B     = Boltzmann constant = 1.380649e-23 J/K
   - T       = Temperature (K)
   - m       = Molecular mass (kg)
   
   For molecular systems:
   k = k_base * (T/T_ref)^n * f_bonding * f_structure
   
   Where:
   - k_base     = Reference conductivity
   - T_ref      = Reference temperature (298.15 K)
   - n          = Temperature exponent (0.5-0.7 for gases, 1.0-1.5 for solids)
   - f_bonding  = Bonding type correction factor
   - f_structure= Structural arrangement factor

2. HEAT CAPACITY (C_v)
   --------------------
   From equipartition theorem and statistical mechanics:
   
   C_v = (f/2) * N * k_B
   
   Where:
   - f  = Degrees of freedom
   - N  = Number of molecules (Avogadro's number for 1 mol)
   - k_B= Boltzmann constant
   
   Degrees of freedom:
   - Translational: 3 (always)
   - Rotational:    2 (linear molecules), 3 (non-linear)
   - Vibrational:   3N - 5 (linear), 3N - 6 (non-linear)
   
   Total: f = f_trans + f_rot + f_vib
   
   Temperature-dependent vibrational contribution:
   C_v(vib) = R * sum_i[ (theta_i/T)^2 * exp(theta_i/T) / (exp(theta_i/T)-1)^2 ]
   
   Where:
   - theta_i = h*nu_i / k_B (characteristic vibrational temperature)
   - nu_i    = Vibrational frequency of mode i
   - h       = Planck's constant = 6.62607e-34 J·s
   - R       = Gas constant = 8.314462 J/(mol·K)

3. ELECTRICAL CONDUCTIVITY (sigma)
   --------------------------------
   For molecular systems (Drude model approximation):
   
   sigma = n * q^2 * tau / m_eff
   
   Where:
   - n     = Charge carrier density (electrons/m^3)
   - q     = Elementary charge = 1.602176e-19 C
   - tau   = Relaxation time (scattering time)
   - m_eff = Effective mass of charge carriers
   
   For ionic systems:
   sigma = sum_i[ n_i * q_i^2 * mu_i ]
   
   Where:
   - n_i  = Ion concentration
   - q_i  = Ion charge
   - mu_i = Ion mobility

4. SEEBECK COEFFICIENT (S)
   ------------------------
   Temperature-dependent charge carrier contribution:
   
   S = (k_B / q) * ln(N_c / n) * (E_F / k_B*T + 2)
   
   Where:
   - E_F = Fermi energy
   - N_c = Effective density of states
   - n   = Carrier concentration
   
   Simplified for molecular systems:
   S = S_base * (T/T_ref)^alpha * f_bonding

5. THERMAL DIFFUSIVITY (alpha)
   ----------------------------
   Heat diffusion rate:
   
   alpha = k / (rho * C_p)
   
   Where:
   - rho = Mass density (kg/m^3)
   - C_p = Heat capacity at constant pressure
   - C_p = C_v + R (for ideal gases)

6. MOLECULAR PERTURBATION
   -----------------------
   Random walk simulation for thermal motion:
   
   delta_x = sqrt(2*D*dt) * N(0,1)
   
   Where:
   - D      = Diffusion coefficient = k_B*T / (6*pi*eta*r)
   - dt     = Time step
   - N(0,1) = Standard normal random variable
   - eta    = Viscosity
   - r      = Particle radius
   
   Simplified perturbation:
   dx = scale * rand(-1,1)
   
   Where:
   - scale = sqrt(k_B*T/E_bond) * length_scale
   - E_bond= Bond energy
   
===============================================================================
*/

#include "cmd_therm.hpp"
#include "cli/display.hpp"
#include "io/xyz_format.hpp"
#include "thermal/thermal_properties.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

using namespace vsepr;
using namespace vsepr::cli;

namespace {

// ============================================================================
// Thermal Evolution Tracking
// ============================================================================
struct ThermalSnapshot {
    int generation;
    double temperature;
    ThermalProperties props;
};

struct ThermalEvolution {
    std::vector<ThermalSnapshot> snapshots;
    int total_generations;
    int sample_interval;
    
    void add_snapshot(int gen, double T, const ThermalProperties& props) {
        snapshots.push_back({gen, T, props});
    }
    
    void clear() {
        snapshots.clear();
    }
};

void print_thermal_evolution(const ThermalEvolution& evolution) {
    Display::Subheader("Thermal Evolution Analysis");
    
    std::cout << "  Total generations: " << evolution.total_generations << "\n";
    std::cout << "  Sample interval:   " << evolution.sample_interval << "\n";
    std::cout << "  Snapshots taken:   " << evolution.snapshots.size() << "\n\n";
    
    if (evolution.snapshots.empty()) {
        std::cout << "  No snapshots recorded.\n";
        return;
    }
    
    // Statistical summary
    double min_conductivity = 1e9, max_conductivity = -1e9;
    double avg_conductivity = 0.0;
    double min_cv = 1e9, max_cv = -1e9;
    
    for (const auto& snap : evolution.snapshots) {
        min_conductivity = std::min(min_conductivity, snap.props.thermal_conductivity);
        max_conductivity = std::max(max_conductivity, snap.props.thermal_conductivity);
        avg_conductivity += snap.props.thermal_conductivity;
        min_cv = std::min(min_cv, snap.props.heat_capacity_Cv);
        max_cv = std::max(max_cv, snap.props.heat_capacity_Cv);
    }
    avg_conductivity /= evolution.snapshots.size();
    
    std::cout << "  Thermal Conductivity Range:\n";
    std::cout << "    Min: " << std::fixed << std::setprecision(2) << min_conductivity << " W/m·K\n";
    std::cout << "    Avg: " << avg_conductivity << " W/m·K\n";
    std::cout << "    Max: " << max_conductivity << " W/m·K\n\n";
    
    std::cout << "  Heat Capacity (Cv) Range:\n";
    std::cout << "    Min: " << std::fixed << std::setprecision(1) << min_cv << " J/mol·K\n";
    std::cout << "    Max: " << max_cv << " J/mol·K\n\n";
    
    // Timeline visualization
    std::cout << "  Thermal Conductivity Timeline:\n";
    const int bar_width = 50;
    double range = max_conductivity - min_conductivity;
    if (range < 1e-6) range = 1.0;  // Avoid division by zero
    
    for (size_t i = 0; i < evolution.snapshots.size(); ++i) {
        const auto& snap = evolution.snapshots[i];
        int bar_len = static_cast<int>(((snap.props.thermal_conductivity - min_conductivity) / range) * bar_width);
        std::cout << "    Gen " << std::setw(6) << snap.generation << ": [";
        std::cout << std::string(bar_len, '█') << std::string(bar_width - bar_len, '░');
        std::cout << "] " << std::fixed << std::setprecision(2) << snap.props.thermal_conductivity << " W/m·K\n";
    }
}

void print_bonding_analysis(const BondingAnalysis& bonding) {
Display::Subheader("Bonding Analysis");
    
    std::string type_str;
    switch (bonding.primary_type) {
        case BondingType::IONIC: type_str = "Ionic"; break;
        case BondingType::COVALENT: type_str = "Covalent"; break;
        case BondingType::METALLIC: type_str = "Metallic"; break;
        case BondingType::MOLECULAR: type_str = "Molecular"; break;
        case BondingType::HYDROGEN: type_str = "Hydrogen-bonded"; break;
        default: type_str = "Unknown"; break;
    }
    
    std::cout << "  Primary bonding:   " << type_str << "\n";
    
    if (bonding.secondary_type != BondingType::UNKNOWN) {
        std::string sec_str;
        switch (bonding.secondary_type) {
            case BondingType::IONIC: sec_str = "Ionic"; break;
            case BondingType::COVALENT: sec_str = "Covalent"; break;
            case BondingType::HYDROGEN: sec_str = "Hydrogen"; break;
            default: sec_str = "Mixed"; break;
        }
        std::cout << "  Secondary bonding: " << sec_str << "\n";
    }
    
    std::cout << "  Ionic character:   " << std::fixed << std::setprecision(2) 
              << bonding.ionic_character * 100.0 << "%\n";
    std::cout << "  Covalent character:" << std::fixed << std::setprecision(2) 
              << bonding.covalent_character * 100.0 << "%\n";
    
    if (bonding.metallic_character > 0.0) {
        std::cout << "  Metallic character:" << std::fixed << std::setprecision(2) 
                  << bonding.metallic_character * 100.0 << "%\n";
        std::cout << "  Free electrons:    ~" << bonding.num_free_electrons << " carriers\n";
    }
    
    if (bonding.has_delocalization) {
        std::cout << "  ★ Delocalized electrons detected\n";
    }
    
    std::cout << "\n  " << bonding.description << "\n";
}

void print_thermal_properties(const ThermalProperties& props) {
Display::Subheader("Thermal Properties");
    
    std::cout << "  Temperature:       " << std::fixed << std::setprecision(1) 
              << props.temperature << " K\n";
    std::cout << "  Phase state:       " << props.phase_state << "\n";
    std::cout << "\n";
    
    std::cout << "  Thermal conductivity:   " << std::fixed << std::setprecision(2) 
              << props.thermal_conductivity << " W/m·K\n";
    std::cout << "  Heat capacity (Cv):     " << std::fixed << std::setprecision(1) 
              << props.heat_capacity_Cv << " J/mol·K\n";
    std::cout << "  Heat capacity (Cp):     " << std::fixed << std::setprecision(1) 
              << props.heat_capacity_Cp << " J/mol·K\n";
    std::cout << "\n";
    
    std::cout << "  Transport mechanism:    " << props.transport_mechanism << "\n";
    
    if (props.is_conductor) {
        std::cout << "  Electrical conductivity:" << std::scientific << std::setprecision(2) 
                  << props.electrical_conductivity << " S/m\n";
        std::cout << "  ★ Conductor: free electron transport\n";
    } else {
        std::cout << "  ★ Insulator: phonon-only heat transfer\n";
    }
}

void print_spatial_tracking(const ThermalProperties& props) {
Display::Subheader("Spatial Tracking (20×20×20 Grid)");
    
    std::cout << ThermalPropertyCalculator::get_particle_location_summary(props.spatial_grid);
    std::cout << "\n" << ThermalPropertyCalculator::render_spatial_grid_2d(props.spatial_grid);
}

bool prompt_generate_object() {
    std::cout << "\n";
    Display::Info("Generate molecular object from this structure?");
    std::cout << "  This will create a reusable object file with thermal properties.\n";
    std::cout << "  Enter 'yes' or 'y' to generate, any other key to skip: ";
    
    std::string response;
    std::getline(std::cin, response);
    
    // Trim whitespace
    response.erase(0, response.find_first_not_of(" \t\n\r"));
    response.erase(response.find_last_not_of(" \t\n\r") + 1);
    
    // Convert to lowercase
    for (char& c : response) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    
    return (response == "yes" || response == "y");
}

void save_thermal_object(const Molecule& mol, const ThermalProperties& props, 
                         const std::string& base_name) {
    std::string obj_filename = base_name + "_thermal.json";
    
    Display::Info("Generating thermal object: " + obj_filename);
    
    // Create JSON output
    std::ostringstream json;
    json << "{\n";
    json << "  \"molecule\": {\n";
    json << "    \"atoms\": " << mol.num_atoms() << ",\n";
    json << "    \"bonds\": " << mol.num_bonds() << "\n";
    json << "  },\n";
    json << "  \"thermal\": {\n";
    json << "    \"temperature\": " << props.temperature << ",\n";
    json << "    \"phase\": \"" << props.phase_state << "\",\n";
    json << "    \"thermal_conductivity\": " << props.thermal_conductivity << ",\n";
    json << "    \"heat_capacity_Cv\": " << props.heat_capacity_Cv << ",\n";
    json << "    \"heat_capacity_Cp\": " << props.heat_capacity_Cp << ",\n";
    json << "    \"is_conductor\": " << (props.is_conductor ? "true" : "false") << "\n";
    json << "  },\n";
    json << "  \"bonding\": {\n";
    
    std::string type_str;
    switch (props.bonding.primary_type) {
        case BondingType::IONIC: type_str = "ionic"; break;
        case BondingType::COVALENT: type_str = "covalent"; break;
        case BondingType::METALLIC: type_str = "metallic"; break;
        case BondingType::MOLECULAR: type_str = "molecular"; break;
        default: type_str = "unknown"; break;
    }
    
    json << "    \"primary_type\": \"" << type_str << "\",\n";
    json << "    \"ionic_character\": " << props.bonding.ionic_character << ",\n";
    json << "    \"covalent_character\": " << props.bonding.covalent_character << ",\n";
    json << "    \"metallic_character\": " << props.bonding.metallic_character << "\n";
    json << "  }\n";
    json << "}\n";
    
    // Write to file
    std::ofstream outfile(obj_filename);
    if (outfile) {
        outfile << json.str();
        Display::Success("Thermal object saved: " + obj_filename);
    } else {
        Display::Error("Failed to write thermal object file");
    }
}

} // anonymous namespace

namespace vsepr {
namespace cli {

std::string ThermCommand::Name() const {
    return "therm";
}

std::string ThermCommand::Description() const {
    return "Analyze thermal properties and bonding types";
}

std::string ThermCommand::Help() const {
    std::ostringstream help;
    help << "Thermal Properties Analysis\n\n";
    help << "USAGE:\n";
    help << "  vsepr therm <input.xyz> [options]\n\n";
    help << "OPTIONS:\n";
    help << "  --temperature, -T <value>  Set temperature in Kelvin (default: 298.15)\n";
    help << "  --generate-object, -g      Force generation of thermal object file\n";
    help << "  --viz                      Enable enhanced visualization output\n";
    help << "  --generations <N>          Run thermal analysis over N generations\n";
    help << "  --sample-interval <M>      Sample every Mth generation (default: 100)\n\n";
    help << "FEATURES:\n";
    help << "  • Bonding type classification (ionic/covalent/metallic)\n";
    help << "  • Thermal conductivity estimation\n";
    help << "  • Heat capacity calculation (Cv and Cp)\n";
    help << "  • Spatial particle tracking on 20×20×20 grid\n";
    help << "  • Phase state prediction\n";
    help << "  • Interactive object generation\n\n";
    help << "EXAMPLES:\n";
    help << "  vsepr therm water.xyz\n";
    help << "  vsepr therm molecule.xyz --temperature 373.15\n";
    help << "  vsepr therm diamond.xyz -T 1000 --generate-object\n";
    help << "  vsepr therm molecule.xyz --viz\n";
    help << "  vsepr therm molecule.xyz --generations 10000 --sample-interval 120\n";
    return help.str();
}

int ThermCommand::Execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        Display::Error("Missing input file");
        Display::Info("Usage: vsepr therm <input.xyz> [--temperature <T>] [--generate-object]");
        Display::Info("Example: vsepr therm water.xyz --temperature 373.15");
        return 1;
    }
    
    std::string input_file = args[0];
    double temperature = 298.15;  // Default: room temperature
    bool force_generate = false;
    bool enable_viz = false;
    int num_generations = 0;  // 0 = single analysis
    int sample_interval = 100;
    
    // Parse optional arguments
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg == "--temperature" || arg == "-T") {
            if (i + 1 < args.size()) {
                temperature = std::stod(args[++i]);
            }
        } else if (arg == "--generate-object" || arg == "-g") {
            force_generate = true;
        } else if (arg == "--viz") {
            enable_viz = true;
        } else if (arg == "--generations") {
            if (i + 1 < args.size()) {
                num_generations = std::stoi(args[++i]);
            }
        } else if (arg == "--sample-interval") {
            if (i + 1 < args.size()) {
                sample_interval = std::stoi(args[++i]);
            }
        }
    }
    
    // Load molecule from XYZ file
    Display::Header("VSEPR Thermal Properties Analyzer");
    Display::Info("Loading molecule from: " + input_file);
    
    // Read XYZ file using XYZReader
    io::XYZReader reader;
    io::XYZMolecule xyz_mol;
    if (!reader.read(input_file, xyz_mol)) {
        Display::Error("Failed to load XYZ file: " + reader.get_error());
        return 1;
    }
    
    // Convert XYZMolecule to Molecule
    Molecule mol;
    for (const auto& atom : xyz_mol.atoms) {
        int atomic_num = io::xyz_utils::get_atomic_number(atom.element);
        mol.add_atom(atomic_num, atom.position[0], atom.position[1], atom.position[2]);
    }
    for (const auto& bond : xyz_mol.bonds) {
        mol.add_bond(bond.atom_i, bond.atom_j, static_cast<uint8_t>(bond.bond_order));
    }
    if (mol.num_bonds() > 0) {
        mol.generate_angles_from_bonds();
    }
    
    if (mol.num_atoms() == 0) {
        Display::Error("Molecule is empty");
        return 1;
    }
    
    Display::Success("Loaded " + std::to_string(mol.num_atoms()) + " atoms");
    
    // Multi-generation mode
    if (num_generations > 0) {
        Display::Info("Running thermal evolution over " + std::to_string(num_generations) + " generations");
        Display::Info("Sampling every " + std::to_string(sample_interval) + " generations");
        
        ThermalEvolution evolution;
        evolution.total_generations = num_generations;
        evolution.sample_interval = sample_interval;
        
        // Generation loop
        for (int gen = 0; gen <= num_generations; gen++) {
            // Sample at intervals
            if (gen % sample_interval == 0 || gen == num_generations) {
                ThermalProperties props = ThermalPropertyCalculator::compute_properties(mol, temperature);
                evolution.add_snapshot(gen, temperature, props);
                
                // Progress display
                if (gen % (sample_interval * 10) == 0 || gen == num_generations) {
                    Display::Progress("Analyzing", gen, num_generations);
                }
            }
            
            // Simulate molecular evolution (simple perturbation)
            // In a real simulation, this would be MD steps, optimization, etc.
            if (gen < num_generations) {
                for (size_t i = 0; i < mol.num_atoms(); ++i) {
                    double x, y, z;
                    mol.get_position(i, x, y, z);
                    // Small random perturbation to simulate dynamics
                    x += (rand() % 1000 - 500) * 0.00001;
                    y += (rand() % 1000 - 500) * 0.00001;
                    z += (rand() % 1000 - 500) * 0.00001;
                    mol.set_position(i, x, y, z);
                }
            }
        }
        Display::ProgressDone();
        
        // Display evolution results
        std::cout << "\n";
        print_thermal_evolution(evolution);
        std::cout << "\n";
        
        Display::Success("Thermal evolution analysis complete");
        return 0;
    }
    
    // Single-shot analysis mode
    Display::Info("Computing thermal properties at T = " + std::to_string(temperature) + " K");
    
    ThermalProperties props = ThermalPropertyCalculator::compute_properties(mol, temperature);
    
    // Display results
    std::cout << "\n";
    print_bonding_analysis(props.bonding);
    std::cout << "\n";
    print_thermal_properties(props);
    std::cout << "\n";
    
    if (enable_viz) {
        print_spatial_tracking(props);
        std::cout << "\n";
        
        // Enhanced visualization output
        Display::Subheader("Enhanced Visualization Mode");
        std::cout << "  Bonding Visualization:\n";
        std::cout << "    • Primary:   [" << std::string(static_cast<int>(props.bonding.ionic_character * 20), '█') 
                  << std::string(20 - static_cast<int>(props.bonding.ionic_character * 20), '░') << "] Ionic\n";
        std::cout << "    • Primary:   [" << std::string(static_cast<int>(props.bonding.covalent_character * 20), '█') 
                  << std::string(20 - static_cast<int>(props.bonding.covalent_character * 20), '░') << "] Covalent\n";
        
        if (props.bonding.metallic_character > 0.0) {
            std::cout << "    • Metallic:  [" << std::string(static_cast<int>(props.bonding.metallic_character * 20), '█') 
                      << std::string(20 - static_cast<int>(props.bonding.metallic_character * 20), '░') << "]\n";
        }
        
        std::cout << "\n  Conductivity Scale:\n";
        double conductivity_scale = std::min(1.0, props.thermal_conductivity / 400.0);
        std::cout << "    Thermal:     [" << std::string(static_cast<int>(conductivity_scale * 20), '█') 
                  << std::string(20 - static_cast<int>(conductivity_scale * 20), '░') << "] " 
                  << props.thermal_conductivity << " W/m·K\n";
        
        if (props.is_conductor) {
            double elec_scale = std::min(1.0, std::log10(props.electrical_conductivity) / 8.0);
            std::cout << "    Electrical:  [" << std::string(static_cast<int>(elec_scale * 20), '█') 
                      << std::string(20 - static_cast<int>(elec_scale * 20), '░') << "] " 
                      << std::scientific << std::setprecision(2) << props.electrical_conductivity << " S/m\n";
        }
        std::cout << "\n";
    } else {
        print_spatial_tracking(props);
        std::cout << "\n";
    }
    
    // Interactive object generation (only if temperature mode is active)
    bool should_generate = force_generate;
    
    if (!force_generate && temperature != 298.15) {
        // In thermal mode (non-default temperature), ask user
        should_generate = prompt_generate_object();
    }
    
    if (should_generate) {
        // Extract base filename
        size_t last_slash = input_file.find_last_of("/\\");
        size_t last_dot = input_file.find_last_of('.');
        std::string base_name;
        
        if (last_dot != std::string::npos && last_dot > last_slash) {
            base_name = input_file.substr(0, last_dot);
        } else {
            base_name = input_file;
        }
        
        save_thermal_object(mol, props, base_name);
    }
    
    Display::Success("Thermal analysis complete");
    return 0;
}

}} // namespace vsepr::cli
