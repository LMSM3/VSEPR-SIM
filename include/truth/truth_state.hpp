#pragma once
/*
truth_state.hpp
---------------
Minimal reproducibility ledger for molecular simulations.

Collects scattered state into one serializable record:
- Atoms, bonds, geometry
- Energy, convergence, health
- Model version, run ID
- Shape hypotheses (HGST-like)

Usage:
  TruthState truth;
  truth.capture_from_molecule(mol);
  truth.capture_energy(E_total, components);
  truth.capture_convergence(opt_result);
  truth.print_oneline();
  truth.save_json("run_12345.truth.json");
*/

#include "sim/molecule.hpp"
#include "pot/energy.hpp"
#include "sim/optimizer.hpp"
#include "core/chemistry.hpp"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <cmath>

namespace vsepr {

// ============================================================================
// Local Geometry Classification
// ============================================================================
// Note: Hybridization enum is now defined in core/chemistry.hpp

struct LocalGeometry {
    uint32_t atom_id = 0;
    Hybridization hybrid = Hybridization::UNKNOWN;
    double planar_score = 0.0;   // 0-1: how planar is local environment
    double linear_score = 0.0;   // 0-1: how linear
    int coordination = 0;         // Number of bonds
};

// ============================================================================
// Global Shape Hypothesis (HGST-like)
// ============================================================================
struct ShapeHypothesis {
    std::string shape_type;  // "helix", "bilayer", "cell", "cluster", "chain", "ring"
    double score = 0.0;      // Confidence score
    std::string evidence;    // Why we think this
    
    // Optional metrics
    double periodicity = 0.0;      // For helices, crystals
    double layer_spacing = 0.0;    // For bilayers
    std::vector<double> cell_params;  // For unit cells: a, b, c, α, β, γ
};

// ============================================================================
// Health Status
// ============================================================================
struct HealthStatus {
    bool has_nan = false;
    bool exploded = false;          // Atoms flew apart
    bool colocated = false;         // Atoms on top of each other
    std::vector<std::string> warnings;
    
    bool is_healthy() const {
        return !has_nan && !exploded && !colocated && warnings.empty();
    }
};

// ============================================================================
// Bond Record (with reasoning)
// ============================================================================
struct BondRecord {
    uint32_t i, j;
    uint8_t order = 1;
    std::string reason;  // "covalent_distance", "topology", "user_specified"
};

// ============================================================================
// TruthState - The Reproducibility Ledger
// ============================================================================
class TruthState {
public:
    // ========================================================================
    // Core Identity
    // ========================================================================
    std::string run_id;              // Timestamp + hash
    std::string input_formula;       // Original input
    std::map<std::string, std::string> flags;  // Command-line flags
    std::string model_version = "v2.0.0";  // Git hash or semver
    
    // ========================================================================
    // Atomic Structure
    // ========================================================================
    struct AtomRecord {
        uint8_t Z;
        double x, y, z;
    };
    std::vector<AtomRecord> atoms;
    std::vector<BondRecord> bonds;
    
    // Optional: hash instead of full coords for privacy/size
    std::string atom_hash;
    bool use_hash = false;
    
    // ========================================================================
    // Geometry Analysis
    // ========================================================================
    std::vector<LocalGeometry> local_geom;
    std::vector<ShapeHypothesis> shape_candidates;  // Ranked by score
    
    // ========================================================================
    // Energy & Convergence
    // ========================================================================
    double E_total = 0.0;
    double E_bond = 0.0;
    double E_angle = 0.0;
    double E_torsion = 0.0;
    double E_nonbonded = 0.0;
    double E_vsepr = 0.0;
    
    int iterations = 0;
    double rms_force = 0.0;
    double max_force = 0.0;
    bool converged = false;
    std::string termination_reason;
    
    // ========================================================================
    // Health Status
    // ========================================================================
    HealthStatus health;
    
    // ========================================================================
    // Model Weights (3-scale or other mixing)
    // ========================================================================
    double weight_alpha = 1.0;  // Bond weight
    double weight_beta = 1.0;   // Angle weight
    double weight_gamma = 1.0;  // Nonbonded weight
    
    // ========================================================================
    // Timestamps
    // ========================================================================
    std::string timestamp_start;
    std::string timestamp_end;
    double elapsed_seconds = 0.0;
    
    // ========================================================================
    // Construction
    // ========================================================================
    TruthState() {
        generate_run_id();
        timestamp_start = get_timestamp();
    }
    
    // ========================================================================
    // Capture Methods (pull data from existing systems)
    // ========================================================================
    
    void capture_from_molecule(const Molecule& mol) {
        // Capture atoms
        atoms.clear();
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double x, y, z;
            mol.get_position(i, x, y, z);
            atoms.push_back({mol.atoms[i].Z, x, y, z});
        }
        
        // Capture bonds
        bonds.clear();
        for (const auto& bond : mol.bonds) {
            bonds.push_back({bond.i, bond.j, bond.order, "topology"});
        }
        
        // Check health
        health.colocated = mol.has_colocated_atoms(1e-6);
    }
    
    void capture_energy(double E, const EnergyResult& components) {
        E_total = E;
        E_bond = components.bond_energy;
        E_angle = components.angle_energy;
        E_torsion = components.torsion_energy;
        E_nonbonded = components.nonbonded_energy;
        
        // Health check
        health.has_nan = !std::isfinite(E);
    }
    
    void capture_convergence(const OptimizeResult& result) {
        iterations = result.iterations;
        rms_force = result.rms_force;
        max_force = result.max_force;
        converged = result.converged;
        termination_reason = result.termination_reason;
        
        capture_energy(result.energy, result.energy_breakdown);
    }
    
    void infer_local_geometry() {
        // Simple coordination number analysis
        local_geom.clear();
        
        for (size_t i = 0; i < atoms.size(); ++i) {
            LocalGeometry lg;
            lg.atom_id = i;
            
            // Count bonds
            for (const auto& bond : bonds) {
                if (bond.i == i || bond.j == i) {
                    lg.coordination++;
                }
            }
            
            // Simple hybridization guess from coordination
            switch (lg.coordination) {
                case 1: lg.hybrid = Hybridization::SP; break;
                case 2: lg.hybrid = Hybridization::SP; break;
                case 3: lg.hybrid = Hybridization::SP2; break;
                case 4: lg.hybrid = Hybridization::SP3; break;
                case 5: lg.hybrid = Hybridization::SP3D; break;
                case 6: lg.hybrid = Hybridization::SP3D2; break;
                default: lg.hybrid = Hybridization::UNKNOWN; break;
            }
            
            local_geom.push_back(lg);
        }
    }
    
    void add_shape_hypothesis(const std::string& type, double score, const std::string& evidence) {
        shape_candidates.push_back({type, score, evidence});
    }
    
    void finalize() {
        timestamp_end = get_timestamp();
        
        // Sort shape hypotheses by score (descending)
        std::sort(shape_candidates.begin(), shape_candidates.end(),
                  [](const ShapeHypothesis& a, const ShapeHypothesis& b) {
                      return a.score > b.score;
                  });
        
        // Final health checks
        if (std::abs(E_total) > 1e6) {
            health.exploded = true;
            health.warnings.push_back("Energy exceeds 1e6 (likely explosion)");
        }
        
        if (iterations >= 5000) {
            health.warnings.push_back("Max iterations reached");
        }
    }
    
    // ========================================================================
    // Output Methods
    // ========================================================================
    
    void print_oneline() const {
        std::cout << "[TRUTH] " << run_id << " | "
                  << input_formula << " | "
                  << atoms.size() << " atoms | "
                  << bonds.size() << " bonds | "
                  << "E=" << std::fixed << std::setprecision(3) << E_total << " | "
                  << "iter=" << iterations << " | "
                  << "conv=" << (converged ? "YES" : "NO") << " | "
                  << "health=" << (health.is_healthy() ? "OK" : "WARN") << "\n";
    }
    
    std::string to_json() const {
        std::ostringstream json;
        json << "{\n";
        
        // Identity
        json << "  \"run_id\": \"" << run_id << "\",\n";
        json << "  \"timestamp_start\": \"" << timestamp_start << "\",\n";
        json << "  \"timestamp_end\": \"" << timestamp_end << "\",\n";
        json << "  \"model_version\": \"" << model_version << "\",\n";
        json << "  \"input_formula\": \"" << input_formula << "\",\n";
        
        // Structure
        json << "  \"atoms\": {\n";
        json << "    \"count\": " << atoms.size() << ",\n";
        json << "    \"data\": [\n";
        for (size_t i = 0; i < atoms.size(); ++i) {
            const auto& a = atoms[i];
            json << "      {\"Z\": " << (int)a.Z << ", \"xyz\": ["
                 << a.x << ", " << a.y << ", " << a.z << "]}";
            if (i < atoms.size() - 1) json << ",";
            json << "\n";
        }
        json << "    ]\n";
        json << "  },\n";
        
        // Bonds
        json << "  \"bonds\": {\n";
        json << "    \"count\": " << bonds.size() << ",\n";
        json << "    \"data\": [\n";
        for (size_t i = 0; i < bonds.size(); ++i) {
            const auto& b = bonds[i];
            json << "      {\"i\": " << b.i << ", \"j\": " << b.j 
                 << ", \"order\": " << (int)b.order 
                 << ", \"reason\": \"" << b.reason << "\"}";
            if (i < bonds.size() - 1) json << ",";
            json << "\n";
        }
        json << "    ]\n";
        json << "  },\n";
        
        // Local geometry
        json << "  \"local_geometry\": [\n";
        for (size_t i = 0; i < local_geom.size(); ++i) {
            const auto& lg = local_geom[i];
            json << "    {\"atom\": " << lg.atom_id 
                 << ", \"coord\": " << lg.coordination 
                 << ", \"hybrid\": \"" << hybrid_to_string(lg.hybrid) << "\"}";
            if (i < local_geom.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Shape candidates
        json << "  \"shape_candidates\": [\n";
        for (size_t i = 0; i < shape_candidates.size(); ++i) {
            const auto& sh = shape_candidates[i];
            json << "    {\"type\": \"" << sh.shape_type 
                 << "\", \"score\": " << sh.score 
                 << ", \"evidence\": \"" << sh.evidence << "\"}";
            if (i < shape_candidates.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Energy
        json << "  \"energy\": {\n";
        json << "    \"total\": " << E_total << ",\n";
        json << "    \"bond\": " << E_bond << ",\n";
        json << "    \"angle\": " << E_angle << ",\n";
        json << "    \"torsion\": " << E_torsion << ",\n";
        json << "    \"nonbonded\": " << E_nonbonded << "\n";
        json << "  },\n";
        
        // Convergence
        json << "  \"convergence\": {\n";
        json << "    \"iterations\": " << iterations << ",\n";
        json << "    \"rms_force\": " << rms_force << ",\n";
        json << "    \"max_force\": " << max_force << ",\n";
        json << "    \"converged\": " << (converged ? "true" : "false") << ",\n";
        json << "    \"reason\": \"" << termination_reason << "\"\n";
        json << "  },\n";
        
        // Health
        json << "  \"health\": {\n";
        json << "    \"has_nan\": " << (health.has_nan ? "true" : "false") << ",\n";
        json << "    \"exploded\": " << (health.exploded ? "true" : "false") << ",\n";
        json << "    \"colocated\": " << (health.colocated ? "true" : "false") << ",\n";
        json << "    \"warnings\": [";
        for (size_t i = 0; i < health.warnings.size(); ++i) {
            json << "\"" << health.warnings[i] << "\"";
            if (i < health.warnings.size() - 1) json << ", ";
        }
        json << "]\n";
        json << "  }\n";
        
        json << "}\n";
        return json.str();
    }
    
    void save_json(const std::string& filename) const {
        std::ofstream out(filename);
        if (out) {
            out << to_json();
        }
    }
    
private:
    void generate_run_id() {
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
        
        // Simple hash from timestamp
        unsigned int hash = static_cast<unsigned int>(t) % 10000;
        
        std::ostringstream oss;
        oss << buf << "_" << std::setw(4) << std::setfill('0') << hash;
        run_id = oss.str();
    }
    
    std::string get_timestamp() const {
        std::time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return std::string(buf);
    }
    
    std::string hybrid_to_string(Hybridization h) const {
        switch (h) {
            case Hybridization::SP: return "sp";
            case Hybridization::SP2: return "sp2";
            case Hybridization::SP3: return "sp3";
            case Hybridization::SP3D: return "sp3d";
            case Hybridization::SP3D2: return "sp3d2";
            default: return "unknown";
        }
    }
};

} // namespace vsepr
