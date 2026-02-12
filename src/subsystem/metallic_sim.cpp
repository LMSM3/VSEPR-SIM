/**
 * VSEPR-Sim Subsystem Integration - Materials Simulation Package
 * Implementation with Hastelloy alloys database
 */

#include "subsystem/metallic_sim.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace subsystem {

// ============================================================================
// Matrix Implementation
// ============================================================================

Matrix::Matrix(size_t rows, size_t cols) 
    : rows_(rows), cols_(cols), data_(rows * cols, 0.0) {}

Matrix::Matrix(const std::vector<std::vector<double>>& data) 
    : rows_(data.size()), cols_(data.empty() ? 0 : data[0].size()) {
    
    data_.reserve(rows_ * cols_);
    for (const auto& row : data) {
        for (double val : row) {
            data_.push_back(val);
        }
    }
}

Matrix Matrix::operator+(const Matrix& other) const {
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Matrix dimensions must match for addition");
    }
    
    Matrix result(rows_, cols_);
    for (size_t i = 0; i < data_.size(); ++i) {
        result.data_[i] = data_[i] + other.data_[i];
    }
    return result;
}

Matrix Matrix::operator-(const Matrix& other) const {
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Matrix dimensions must match for subtraction");
    }
    
    Matrix result(rows_, cols_);
    for (size_t i = 0; i < data_.size(); ++i) {
        result.data_[i] = data_[i] - other.data_[i];
    }
    return result;
}

Matrix Matrix::operator*(const Matrix& other) const {
    if (cols_ != other.rows_) {
        throw std::invalid_argument("Matrix dimensions incompatible for multiplication");
    }
    
    Matrix result(rows_, other.cols_);
    for (size_t i = 0; i < rows_; ++i) {
        for (size_t j = 0; j < other.cols_; ++j) {
            double sum = 0.0;
            for (size_t k = 0; k < cols_; ++k) {
                sum += (*this)(i, k) * other(k, j);
            }
            result(i, j) = sum;
        }
    }
    return result;
}

Matrix Matrix::transpose() const {
    Matrix result(cols_, rows_);
    for (size_t i = 0; i < rows_; ++i) {
        for (size_t j = 0; j < cols_; ++j) {
            result(j, i) = (*this)(i, j);
        }
    }
    return result;
}

double Matrix::det() const {
    if (rows_ != cols_) {
        throw std::invalid_argument("Determinant only defined for square matrices");
    }
    
    if (rows_ == 1) return data_[0];
    if (rows_ == 2) return data_[0] * data_[3] - data_[1] * data_[2];
    
    // For larger matrices, use simplified calculation (placeholder)
    return 0.0;
}

double& Matrix::operator()(size_t row, size_t col) {
    return data_[row * cols_ + col];
}

double Matrix::operator()(size_t row, size_t col) const {
    return data_[row * cols_ + col];
}

// ============================================================================
// MetallicSimulator Implementation
// ============================================================================

MetallicSimulator::MetallicSimulator() {
    init_default_database();
}

void MetallicSimulator::init_default_database() {
    // Hastelloy alloys database (from user's table)
    // Columns: Material, σ_b/MPa (tensile), σ_0.2/MPa (yield), σ_s (elongation %), HRB (hardness)
    
    database_["Hastelloy B"] = MechanicalProperties(
        "Hastelloy B", 760.0, 350.0, 60.0, 90
    );
    
    database_["Hastelloy B-2"] = MechanicalProperties(
        "Hastelloy B-2", 760.0, 350.0, 60.0, 90
    );
    
    database_["Hastelloy B-3"] = MechanicalProperties(
        "Hastelloy B-3", 690.0, 310.0, 64.0, 90
    );
    
    database_["Hastelloy C-276"] = MechanicalProperties(
        "Hastelloy C-276", 690.0, 280.0, 65.0, 89
    );
    
    database_["Hastelloy C-4"] = MechanicalProperties(
        "Hastelloy C-4", 785.0, 400.0, 54.0, 95
    );
    
    database_["Hastelloy C-22"] = MechanicalProperties(
        "Hastelloy C-22", 760.0, 310.0, 62.0, 90
    );
    
    database_["Hastelloy G"] = MechanicalProperties(
        "Hastelloy G", 690.0, 240.0, 71.0, 89
    );
    
    database_["Hastelloy G-3"] = MechanicalProperties(
        "Hastelloy G-3", 690.0, 310.0, 60.0, 96
    );
    
    database_["Hastelloy G-30"] = MechanicalProperties(
        "Hastelloy G-30", 690.0, 280.0, 60.0, 90
    );
    
    // Additional reference materials
    database_["Steel 316L"] = MechanicalProperties(
        "Steel 316L", 485.0, 170.0, 40.0, 79
    );
    
    database_["Inconel 625"] = MechanicalProperties(
        "Inconel 625", 830.0, 415.0, 42.5, 95
    );
}

MechanicalProperties MetallicSimulator::get_material(const std::string& name) const {
    auto it = database_.find(name);
    if (it != database_.end()) {
        return it->second;
    }
    
    // Return empty properties if not found
    return MechanicalProperties("Unknown", 0.0, 0.0, 0.0, 0);
}

std::vector<MechanicalProperties> MetallicSimulator::get_all_materials() const {
    std::vector<MechanicalProperties> result;
    for (const auto& pair : database_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<MechanicalProperties> MetallicSimulator::search_materials(
    double min_tensile_MPa,
    double max_tensile_MPa,
    double min_yield_MPa,
    double max_yield_MPa) const {
    
    std::vector<MechanicalProperties> result;
    
    for (const auto& pair : database_) {
        const auto& props = pair.second;
        
        if (props.tensile_strength_MPa >= min_tensile_MPa &&
            props.tensile_strength_MPa <= max_tensile_MPa &&
            props.yield_strength_MPa >= min_yield_MPa &&
            props.yield_strength_MPa <= max_yield_MPa) {
            
            result.push_back(props);
        }
    }
    
    return result;
}

MetallicSimulator::PredictionResult MetallicSimulator::predict_properties(
    const std::map<std::string, double>& composition,
    double temperature_K) {
    
    PredictionResult result;
    result.confidence = 0.0;
    result.method = "interpolation";
    
    // Simple prediction based on composition
    // Ni-Cr-Mo alloys (Hastelloy-like)
    double ni = composition.count("Ni") ? composition.at("Ni") : 0.0;
    double cr = composition.count("Cr") ? composition.at("Cr") : 0.0;
    double mo = composition.count("Mo") ? composition.at("Mo") : 0.0;
    
    if (ni > 40.0 && ni < 70.0 && cr > 10.0 && cr < 25.0 && mo > 5.0 && mo < 20.0) {
        // Hastelloy-like composition
        result.properties.material = "Predicted Ni-Cr-Mo Alloy";
        result.properties.tensile_strength_MPa = 650.0 + (ni - 50.0) * 2.0 + cr * 1.5 + mo * 3.0;
        result.properties.yield_strength_MPa = 250.0 + (ni - 50.0) * 1.5 + cr * 2.0 + mo * 4.0;
        result.properties.elongation_percent = 65.0 - (mo - 10.0) * 0.5;
        result.properties.hardness_HRB = static_cast<int>(85.0 + cr * 0.3 + mo * 0.5);
        result.confidence = 0.85;
        
        // Temperature correction (simplified)
        double temp_factor = 1.0 - (temperature_K - 298.15) / 1000.0 * 0.2;
        result.properties.tensile_strength_MPa *= temp_factor;
        result.properties.yield_strength_MPa *= temp_factor;
    } else {
        // Unknown composition
        result.properties.material = "Unknown Composition";
        result.confidence = 0.1;
    }
    
    return result;
}

std::vector<double> MetallicSimulator::fit_stress_strain(
    const std::vector<double>& strain,
    const std::vector<double>& stress,
    int polynomial_degree) {
    
    if (strain.size() != stress.size() || strain.empty()) {
        throw std::invalid_argument("Strain and stress vectors must have same non-zero size");
    }
    
    // Simplified polynomial fit (least squares)
    // Returns coefficients [a0, a1, a2, ...] for stress = a0 + a1*ε + a2*ε² + ...
    
    std::vector<double> coeffs(polynomial_degree + 1, 0.0);
    
    // For demo, just do linear fit (degree 1)
    if (polynomial_degree == 1) {
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        size_t n = strain.size();
        
        for (size_t i = 0; i < n; ++i) {
            sum_x += strain[i];
            sum_y += stress[i];
            sum_xy += strain[i] * stress[i];
            sum_x2 += strain[i] * strain[i];
        }
        
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
        double intercept = (sum_y - slope * sum_x) / n;
        
        coeffs[0] = intercept;  // a0
        coeffs[1] = slope;      // a1 (Young's modulus approx)
    } else {
        // For higher degrees, return approximate values
        coeffs[0] = stress[0];
        coeffs[1] = stress.back() / strain.back();  // Approximate slope
    }
    
    return coeffs;
}

MetallicSimulator::FailureAnalysis MetallicSimulator::analyze_failure(
    const MechanicalProperties& material,
    double applied_stress_MPa,
    double temperature_K) {
    
    FailureAnalysis analysis;
    
    // Temperature correction
    double temp_factor = 1.0 - (temperature_K - 298.15) / 1000.0 * 0.2;
    double corrected_yield = material.yield_strength_MPa * temp_factor;
    double corrected_tensile = material.tensile_strength_MPa * temp_factor;
    
    // Simple von Mises stress (assuming uniaxial tension)
    analysis.von_mises_stress_MPa = applied_stress_MPa;
    analysis.max_stress_MPa = applied_stress_MPa;
    
    // Safety factors
    double sf_yield = corrected_yield / applied_stress_MPa;
    double sf_tensile = corrected_tensile / applied_stress_MPa;
    
    analysis.safety_factor = std::min(sf_yield, sf_tensile);
    
    // Failure prediction
    if (applied_stress_MPa >= corrected_tensile) {
        analysis.will_fail = true;
        analysis.failure_mode = "tensile";
    } else if (applied_stress_MPa >= corrected_yield) {
        analysis.will_fail = true;
        analysis.failure_mode = "yield";
    } else if (analysis.safety_factor < 1.5) {
        analysis.will_fail = false;
        analysis.failure_mode = "fatigue risk";
    } else {
        analysis.will_fail = false;
        analysis.failure_mode = "safe";
    }
    
    return analysis;
}

void MetallicSimulator::load_database(const std::string& filename) {
    // Placeholder for file loading
    // In real implementation, would parse CSV/JSON file
    throw std::runtime_error("Database loading not implemented in demo");
}

// ============================================================================
// SubsystemInterface Implementation
// ============================================================================

SubsystemInterface& SubsystemInterface::instance() {
    static SubsystemInterface inst;
    return inst;
}

void SubsystemInterface::register_subsystem(const std::string& name, SubsystemCallback callback) {
    subsystems_[name] = callback;
}

std::string SubsystemInterface::call_subsystem(const std::string& name, const std::string& input) {
    auto it = subsystems_.find(name);
    if (it != subsystems_.end()) {
        return it->second(input);
    }
    return "ERROR: Subsystem '" + name + "' not found";
}

std::vector<std::string> SubsystemInterface::list_subsystems() const {
    std::vector<std::string> result;
    for (const auto& pair : subsystems_) {
        result.push_back(pair.first);
    }
    return result;
}

} // namespace subsystem
} // namespace vsepr
