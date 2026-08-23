/**
 * VSEPR-Sim Subsystem Integration - Materials Simulation Package
 * Demonstrates MatLabForC++ style deterministic metallic simulation
 * Feature 9/8: External package interoperability
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace vsepr {
namespace subsystem {

// ============================================================================
// Mechanical Properties (from Hastelloy table)
// ============================================================================

struct MechanicalProperties {
    std::string material;
    double tensile_strength_MPa;    // σ_b
    double yield_strength_MPa;      // σ_0.2
    double elongation_percent;      // σ_s
    int hardness_HRB;
    
    // Constructor
    MechanicalProperties(const std::string& mat = "", 
                        double tensile = 0.0, 
                        double yield = 0.0,
                        double elong = 0.0, 
                        int hard = 0)
        : material(mat), 
          tensile_strength_MPa(tensile),
          yield_strength_MPa(yield),
          elongation_percent(elong),
          hardness_HRB(hard) {}
};

// ============================================================================
// MatLabForC++ Style Matrix Operations
// ============================================================================

class Matrix {
public:
    Matrix(size_t rows, size_t cols);
    Matrix(const std::vector<std::vector<double>>& data);
    
    // MATLAB-style operations
    Matrix operator+(const Matrix& other) const;
    Matrix operator-(const Matrix& other) const;
    Matrix operator*(const Matrix& other) const;
    Matrix transpose() const;
    double det() const;
    
    // Element access
    double& operator()(size_t row, size_t col);
    double operator()(size_t row, size_t col) const;
    
    // Dimensions
    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
    
private:
    size_t rows_, cols_;
    std::vector<double> data_;
};

// ============================================================================
// Deterministic Metallic Simulation Engine
// ============================================================================

class MetallicSimulator {
public:
    MetallicSimulator();
    
    // MATLAB-style function: Predict properties from composition
    // [props, confidence] = predict_properties(composition, temperature)
    struct PredictionResult {
        MechanicalProperties properties;
        double confidence;  // 0.0 - 1.0
        std::string method; // "database" or "interpolation"
    };
    
    PredictionResult predict_properties(
        const std::map<std::string, double>& composition,
        double temperature_K = 298.15
    );
    
    // Load materials database
    void load_database(const std::string& filename);
    
    // Get material by name (deterministic lookup)
    MechanicalProperties get_material(const std::string& name) const;
    
    // Get all materials
    std::vector<MechanicalProperties> get_all_materials() const;
    
    // Search materials by property ranges
    std::vector<MechanicalProperties> search_materials(
        double min_tensile_MPa,
        double max_tensile_MPa,
        double min_yield_MPa,
        double max_yield_MPa
    ) const;
    
    // Fit stress-strain curve (MATLAB-style polyfit)
    std::vector<double> fit_stress_strain(
        const std::vector<double>& strain,
        const std::vector<double>& stress,
        int polynomial_degree = 3
    );
    
    // Predict failure (deterministic FEA-lite)
    struct FailureAnalysis {
        double safety_factor;
        double max_stress_MPa;
        double von_mises_stress_MPa;
        bool will_fail;
        std::string failure_mode;  // "tensile", "yield", "fatigue"
    };
    
    FailureAnalysis analyze_failure(
        const MechanicalProperties& material,
        double applied_stress_MPa,
        double temperature_K = 298.15
    );
    
private:
    void init_default_database();
    
    std::map<std::string, MechanicalProperties> database_;
};

// ============================================================================
// Subsystem Call Interface (like MATLAB Engine API)
// ============================================================================

class SubsystemInterface {
public:
    static SubsystemInterface& instance();
    
    // Register subsystem
    using SubsystemCallback = std::function<std::string(const std::string&)>;
    void register_subsystem(const std::string& name, SubsystemCallback callback);
    
    // Call subsystem (deterministic)
    std::string call_subsystem(const std::string& name, const std::string& input);
    
    // List available subsystems
    std::vector<std::string> list_subsystems() const;
    
private:
    SubsystemInterface() = default;
    std::map<std::string, SubsystemCallback> subsystems_;
};

// ============================================================================
// Demo Integration Functions
// ============================================================================

// Example: Call from VSEPR-Sim to materials package
inline MechanicalProperties demo_call_materials_package(const std::string& material_name) {
    MetallicSimulator sim;
    return sim.get_material(material_name);
}

// Example: Batch property lookup
inline std::vector<MechanicalProperties> demo_batch_lookup(
    const std::vector<std::string>& materials) {
    
    MetallicSimulator sim;
    std::vector<MechanicalProperties> results;
    
    for (const auto& mat : materials) {
        results.push_back(sim.get_material(mat));
    }
    
    return results;
}

// Example: Property prediction with confidence
inline MetallicSimulator::PredictionResult demo_predict_alloy(
    double ni_percent, double cr_percent, double mo_percent) {
    
    MetallicSimulator sim;
    
    std::map<std::string, double> composition;
    composition["Ni"] = ni_percent;
    composition["Cr"] = cr_percent;
    composition["Mo"] = mo_percent;
    composition["Fe"] = 100.0 - ni_percent - cr_percent - mo_percent;
    
    return sim.predict_properties(composition);
}

} // namespace subsystem
} // namespace vsepr
