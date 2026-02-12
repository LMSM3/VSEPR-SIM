/**
 * HGST Matrix - Hierarchical Graph State Theory
 * Chemistry governor matrix for actinide and complex molecular systems
 * 
 * State vector: x = [ρD, Γ, S, Π, Q]
 * - ρD: Donor confidence (N→An coordination quality)
 * - Γ: Geometry score (VSEPR alignment, symmetry)
 * - S: Steric penalty (crowding, repulsion)
 * - Π: Agostic propensity (B-H-An interactions)
 * - Q: Oxidation state plausibility
 * 
 * Update: y = H_HGST * x
 */

#pragma once

#include <vector>
#include <array>
#include <string>
#include <map>

namespace vsepr {
namespace hgst {

// 5D state vector for HGST
struct StateVector {
    double donor_conf;      // ρD: Donor coordination confidence
    double geom_score;      // Γ: Geometry quality score
    double steric_penalty;  // S: Steric crowding penalty
    double agostic_prop;    // Π: Agostic interaction propensity
    double ox_plausibility; // Q: Oxidation state plausibility
    
    StateVector() : donor_conf(0.0), geom_score(0.0), steric_penalty(0.0),
                    agostic_prop(0.0), ox_plausibility(0.0) {}
    
    std::array<double, 5> to_array() const {
        return {donor_conf, geom_score, steric_penalty, agostic_prop, ox_plausibility};
    }
    
    void from_array(const std::array<double, 5>& arr) {
        donor_conf = arr[0];
        geom_score = arr[1];
        steric_penalty = arr[2];
        agostic_prop = arr[3];
        ox_plausibility = arr[4];
    }
};

// 5×5 HGST operator matrix
class HGSTMatrix {
public:
    // Default actinide chemistry-tuned matrix
    static constexpr double DEFAULT_MATRIX[5][5] = {
        // donor    geom    steric  agostic  ox
        {  1.00,   0.25,  -0.40,   0.30,   0.15 },  // donor channel
        {  0.20,   1.00,  -0.35,   0.10,   0.25 },  // geometry
        { -0.30,  -0.20,   1.00,  -0.15,  -0.10 },  // steric penalty
        {  0.35,   0.15,  -0.25,   1.00,   0.05 },  // agostic
        {  0.10,   0.30,  -0.15,   0.05,   1.00 }   // oxidation state
    };
    
    HGSTMatrix();
    explicit HGSTMatrix(const double matrix[5][5]);
    
    // Apply H_HGST transformation: y = H * x
    StateVector apply(const StateVector& x) const;
    
    // Matrix element access
    double operator()(int i, int j) const { return matrix_[i][j]; }
    double& operator()(int i, int j) { return matrix_[i][j]; }
    
    // Print matrix for debugging
    void print(const std::string& label = "HGST") const;
    
private:
    double matrix_[5][5];
};

// Bond scoring using HGST-style feature vector
struct BondFeatures {
    double distance;        // d: Bond distance (Å)
    double angle;          // θ: Bond angle deviation from ideal
    double donor_type;     // t: Donor atom type score (0-1)
    double ox_plausibility;// o: Oxidation state match (0-1)
    double steric_crowding;// σ: Local steric crowding (0-1)
    double symmetry_role;  // κ: Symmetry contribution (0-1)
    
    BondFeatures() : distance(0.0), angle(0.0), donor_type(0.0),
                     ox_plausibility(0.0), steric_crowding(0.0),
                     symmetry_role(0.0) {}
    
    std::array<double, 6> to_array() const {
        return {distance, angle, donor_type, ox_plausibility, steric_crowding, symmetry_role};
    }
};

// Bond scorer using weighted feature vector
class BondScorer {
public:
    // Default weights for bond scoring
    static constexpr double DEFAULT_WEIGHTS[6] = {
        -0.5,  // w_d: distance penalty (closer to ideal = better)
        -0.3,  // w_θ: angle penalty
         0.4,  // w_donor: donor type bonus
         0.3,  // w_ox: oxidation plausibility
        -0.6,  // w_steric: steric penalty
         0.2   // w_sym: symmetry bonus
    };
    
    BondScorer();
    explicit BondScorer(const double weights[6]);
    
    // Compute bond score: s = W · f
    double score(const BondFeatures& features) const;
    
    // Map score to visual properties
    double opacity(double score) const;      // clamp(s, 0, 1)
    double thickness(double score) const;    // bond order estimate
    bool is_dashed(double score) const;      // multicenter/agostic indicator
    
private:
    double weights_[6];
};

// Chemical dashboard - live HGST state display
class ChemicalDashboard {
public:
    ChemicalDashboard();
    
    void update(const StateVector& state);
    void print() const;
    
    StateVector get_state() const { return current_state_; }
    
private:
    StateVector current_state_;
    HGSTMatrix hgst_matrix_;
};

}} // namespace vsepr::hgst
