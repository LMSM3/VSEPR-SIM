/**
 * decay_chains.hpp
 * ================
 * Natural radioactive decay series (Thorium, Neptunium, Uranium, Actinium)
 * Complete decay chain tracking with branching ratios and half-lives
 * 
 * Four Major Series:
 * 1. Thorium Series (4n)       - Th-232 → Pb-208
 * 2. Neptunium Series (4n+1)   - Np-237 → Bi-209 (extinct)
 * 3. Uranium Series (4n+2)     - U-238 → Pb-206
 * 4. Actinium Series (4n+3)    - U-235 → Pb-207
 */

#ifndef VSEPR_DECAY_CHAINS_HPP
#define VSEPR_DECAY_CHAINS_HPP

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace vsepr {
namespace nuclear {

// ============================================================================
// Decay Mode Enumeration
// ============================================================================

enum class DecayMode {
    Alpha,              // α decay (He-4 emission)
    BetaMinus,          // β⁻ decay (electron emission)
    BetaPlus,           // β⁺ decay (positron emission)
    ElectronCapture,    // EC (orbital electron capture)
    Fission,            // Spontaneous fission
    Stable              // No decay
};

// ============================================================================
// Decay Nuclide Structure
// ============================================================================

struct DecayNuclide {
    uint8_t Z;                      // Atomic number
    uint16_t A;                     // Mass number
    std::string symbol;             // Element symbol (e.g., "U")
    std::string isotope_name;       // Full name (e.g., "U-238")
    
    // Decay properties
    double half_life_years;         // Half-life in years
    std::string half_life_unit;     // "y", "d", "h", "m", "s", "ms", "μs"
    double half_life_seconds;       // Half-life in seconds
    
    DecayMode primary_decay;        // Primary decay mode
    double branching_ratio;         // Branching ratio (0.0-1.0)
    
    // Daughter products
    struct DaughterProduct {
        uint8_t Z;
        uint16_t A;
        DecayMode mode;
        double branching_ratio;     // If multiple decay paths
        double energy_MeV;          // Decay energy (Q-value)
    };
    std::vector<DaughterProduct> daughters;
    
    // Parent nuclides (for reverse lookup)
    std::vector<std::pair<uint8_t, uint16_t>> parents;
    
    // Energy data
    double decay_energy_MeV;        // Total decay energy (Q-value)
    double alpha_energy_MeV;        // Alpha particle energy (if α decay)
    double beta_endpoint_MeV;       // Beta endpoint energy (if β decay)
    
    // Activity calculations
    bool is_stable() const { return primary_decay == DecayMode::Stable; }
    double decay_constant() const;  // λ = ln(2) / t₁/₂
    double activity_Bq(double N) const;  // A = λN (Becquerels)
    double activity_Ci(double N) const;  // Activity in Curies
};

// ============================================================================
// Decay Chain Structure
// ============================================================================

struct DecayChain {
    std::string name;               // "Thorium Series", "Uranium Series", etc.
    std::string series_type;        // "4n", "4n+1", "4n+2", "4n+3"
    uint8_t parent_Z;               // Parent atomic number
    uint16_t parent_A;              // Parent mass number
    uint8_t stable_Z;               // Stable end product Z
    uint16_t stable_A;              // Stable end product A
    
    // Complete chain from parent to stable daughter
    std::vector<DecayNuclide> chain;
    
    // Branching chains (alternative paths)
    std::vector<std::vector<DecayNuclide>> branches;
    
    // Statistics
    int total_decays;               // Number of decays to reach stability
    int alpha_decays;               // Number of α decays
    int beta_decays;                // Number of β decays
    double total_energy_MeV;        // Total energy released
    double longest_half_life_years; // Longest-lived intermediate
    
    // Equilibrium calculations
    bool is_secular_equilibrium(double t_years) const;
    std::map<std::string, double> equilibrium_ratios() const;
};

// ============================================================================
// The Four Natural Decay Series
// ============================================================================

class NaturalDecaySeries {
public:
    NaturalDecaySeries();
    
    // Access decay chains
    const DecayChain& thorium_series() const { return thorium_; }
    const DecayChain& neptunium_series() const { return neptunium_; }
    const DecayChain& uranium_series() const { return uranium_; }
    const DecayChain& actinium_series() const { return actinium_; }
    
    // Lookup by series type
    const DecayChain* get_series(const std::string& series_type) const;
    
    // Lookup by parent isotope
    const DecayChain* get_series_for_isotope(uint8_t Z, uint16_t A) const;
    
    // Query functions
    std::optional<DecayNuclide> find_nuclide(uint8_t Z, uint16_t A) const;
    std::vector<DecayNuclide> get_daughters(uint8_t Z, uint16_t A) const;
    std::vector<DecayNuclide> get_parents(uint8_t Z, uint16_t A) const;
    
    // Decay path finding
    std::vector<DecayNuclide> trace_decay_path(uint8_t Z_start, uint16_t A_start) const;
    int steps_to_stability(uint8_t Z, uint16_t A) const;
    
    // Visualization helpers
    struct ChainGraphNode {
        uint8_t Z;
        uint16_t A;
        std::string label;          // "U-238"
        int level;                  // Depth in chain
        double half_life_years;
        DecayMode decay_mode;
        std::vector<int> children;  // Indices of daughter nodes
    };
    std::vector<ChainGraphNode> get_chain_graph(const std::string& series_type) const;
    
    // Export functions
    void export_to_json(const std::string& filepath) const;
    void export_to_dot(const std::string& filepath, const std::string& series_type) const;
    
private:
    DecayChain thorium_;     // 4n:   Th-232 → Pb-208
    DecayChain neptunium_;   // 4n+1: Np-237 → Bi-209 (extinct)
    DecayChain uranium_;     // 4n+2: U-238 → Pb-206
    DecayChain actinium_;    // 4n+3: U-235 → Pb-207
    
    void initialize_thorium_series();
    void initialize_neptunium_series();
    void initialize_uranium_series();
    void initialize_actinium_series();
    
    // Helper: create nuclide entry
    DecayNuclide make_nuclide(
        uint8_t Z, uint16_t A,
        double half_life, const std::string& unit,
        DecayMode mode, double energy_MeV
    );
};

// ============================================================================
// Decay Calculator Utilities
// ============================================================================

class DecayCalculator {
public:
    // Activity calculations
    static double atoms_remaining(double N0, double lambda, double t);
    static double activity_at_time(double A0, double lambda, double t);
    
    // Secular equilibrium (parent >> daughter half-life)
    static double secular_equilibrium_ratio(
        double lambda_parent,
        double lambda_daughter
    );
    
    // Transient equilibrium (parent > daughter half-life)
    static double transient_equilibrium_ratio(
        double lambda_parent,
        double lambda_daughter,
        double t
    );
    
    // Bateman equations (multi-step decay chains)
    static std::vector<double> bateman_solution(
        const std::vector<double>& lambdas,
        const std::vector<double>& N0,
        double t
    );
    
    // Dose calculations (simplified)
    struct DoseRate {
        double alpha_dose_Sv;
        double beta_dose_Sv;
        double gamma_dose_Sv;
        double total_dose_Sv;
    };
    static DoseRate calculate_dose(
        const DecayNuclide& nuclide,
        double activity_Bq,
        double exposure_time_s,
        double distance_m
    );
};

// ============================================================================
// Radon Subseries (Important Environmental Isotopes)
// ============================================================================

struct RadonIsotopes {
    // Rn-222 (Uranium series) - most important
    DecayNuclide rn222;     // t½ = 3.82 days
    
    // Rn-220 (Thorium series) - "Thoron"
    DecayNuclide rn220;     // t½ = 55.6 seconds
    
    // Rn-219 (Actinium series) - "Actinon"
    DecayNuclide rn219;     // t½ = 3.96 seconds
    
    // Environmental impact data
    struct RadonData {
        double background_Bq_per_m3;    // Typical indoor concentration
        double epa_action_level_Bq_per_m3;  // 148 Bq/m³ (4 pCi/L)
        double lung_dose_coefficient_Sv_per_Bq_h;
    };
    RadonData environmental_data;
};

// ============================================================================
// Singleton Access
// ============================================================================

const NaturalDecaySeries& get_decay_series();
void init_decay_series();

// ============================================================================
// Helper Functions
// ============================================================================

// Convert decay mode to string
std::string decay_mode_to_string(DecayMode mode);
std::string decay_mode_to_symbol(DecayMode mode);  // "α", "β⁻", "β⁺", "EC"

// Time unit conversions
double years_to_seconds(double years);
double seconds_to_years(double seconds);
std::string format_half_life(double seconds);  // Auto-select best unit

// Mass number relationships
int series_type_4n(uint16_t A);      // Returns 0, 1, 2, or 3
std::string series_name_for_A(uint16_t A);  // "4n", "4n+1", "4n+2", "4n+3"

} // namespace nuclear
} // namespace vsepr

#endif // VSEPR_DECAY_CHAINS_HPP
