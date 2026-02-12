#pragma once
#include "../core/state.hpp"
#include "model.hpp"
#include <memory>

namespace atomistic {

/**
 * Bonded force field based on molecular mechanics
 * 
 * Physics:
 * --------
 * 
 * 1. Harmonic Bonds: U = kb(r - r0)²
 *    - Hooke's law approximation near equilibrium
 *    - Force: F = -2kb(r - r0)r̂
 *    - Typical kb ~ 300-500 kcal/mol/Ų (C-C, C-H bonds)
 * 
 * 2. Harmonic Angles: U = kθ(θ - θ0)²
 *    - Small-angle approximation of bending
 *    - Force derived from ∂U/∂xi using chain rule
 *    - Typical kθ ~ 50-100 kcal/mol/rad² (H-C-H, C-C-C)
 * 
 * 3. Periodic Torsions: U = Σ Vn[1 + cos(nφ - γ)]
 *    - Fourier series for dihedral rotation
 *    - Multiple periodicities (n=1,2,3,6) for different barriers
 *    - Typical V ~ 0.5-3 kcal/mol (C-C rotation barriers)
 *    - γ = phase offset (0° for trans, 180° for cis)
 * 
 * 4. Improper Torsions: U = kimp(ψ - ψ0)² 
 *    - Maintain planarity (sp² carbons, amide bonds)
 *    - Out-of-plane angle ψ defined via cross product
 *    - Typical kimp ~ 10-50 kcal/mol/rad²
 * 
 * References:
 * -----------
 * - MacKerell, A.D. et al. (1998). "All-atom CHARMM27 force field." J. Phys. Chem. B 102(18), 3586.
 * - Cornell, W.D. et al. (1995). "AMBER force field." J. Am. Chem. Soc. 117(19), 5179.
 * - Jorgensen, W.L. (1996). "OPLS all-atom force field." J. Am. Chem. Soc. 118(45), 11225.
 * - Blondel, A. & Karplus, M. (1996). "New formulation for derivatives of torsion angles." 
 *   J. Comp. Chem. 17(9), 1132-1141.
 */

/**
 * Bond parameters
 */
struct BondParams {
    uint32_t i, j;      // Atom indices
    double kb;          // Force constant (kcal/mol/Ų)
    double r0;          // Equilibrium length (Å)
    
    BondParams(uint32_t i_, uint32_t j_, double kb_, double r0_)
        : i(i_), j(j_), kb(kb_), r0(r0_) {}
};

/**
 * Angle parameters (i-j-k)
 */
struct AngleParams {
    uint32_t i, j, k;   // Atom indices (j is vertex)
    double ktheta;      // Force constant (kcal/mol/rad²)
    double theta0;      // Equilibrium angle (radians)
    
    AngleParams(uint32_t i_, uint32_t j_, uint32_t k_, double kt_, double t0_)
        : i(i_), j(j_), k(k_), ktheta(kt_), theta0(t0_) {}
};

/**
 * Dihedral (torsion) parameters (i-j-k-l)
 * Multiple terms allowed for same dihedral
 */
struct DihedralParams {
    uint32_t i, j, k, l;  // Atom indices (j-k is rotation axis)
    int n;                // Periodicity (1, 2, 3, 4, 6, etc.)
    double Vn;            // Barrier height (kcal/mol)
    double gamma;         // Phase offset (radians)
    
    DihedralParams(uint32_t i_, uint32_t j_, uint32_t k_, uint32_t l_,
                   int n_, double V_, double g_)
        : i(i_), j(j_), k(k_), l(l_), n(n_), Vn(V_), gamma(g_) {}
};

/**
 * Improper (out-of-plane) parameters
 */
struct ImproperParams {
    uint32_t i, j, k, l;  // Central atom j, plane defined by i,k,l
    double kimp;          // Force constant (kcal/mol/rad²)
    double psi0;          // Equilibrium out-of-plane angle (radians, usually 0)
    
    ImproperParams(uint32_t i_, uint32_t j_, uint32_t k_, uint32_t l_,
                   double kimp_, double psi0_)
        : i(i_), j(j_), k(k_), l(l_), kimp(kimp_), psi0(psi0_) {}
};

/**
 * Complete bonded force field specification
 */
struct BondedTopology {
    std::vector<BondParams> bonds;
    std::vector<AngleParams> angles;
    std::vector<DihedralParams> dihedrals;
    std::vector<ImproperParams> impropers;
    
    // Auto-generate angles from bond graph (all i-j-k triplets)
    void generate_angles_from_bonds();
    
    // Auto-generate dihedrals from bond graph (all i-j-k-l quartets)
    void generate_dihedrals_from_bonds();
    
    // Assign default parameters based on element types
    void assign_default_parameters(const State& s);
};

/**
 * Bonded force model
 * Computes forces and energies from bonds, angles, torsions, impropers
 */
class BondedModel : public IModel {
public:
    explicit BondedModel(const BondedTopology& topo) : topology(topo) {}
    
    void eval(State& s, const ModelParams& p) const override;
    
    // Evaluate individual terms (for debugging/analysis)
    double eval_bonds(State& s) const;
    double eval_angles(State& s) const;
    double eval_dihedrals(State& s) const;
    double eval_impropers(State& s) const;
    
private:
    BondedTopology topology;
    
    // Helper: compute dihedral angle φ for atoms i-j-k-l
    static double compute_dihedral_angle(const Vec3& ri, const Vec3& rj,
                                         const Vec3& rk, const Vec3& rl);
    
    // Helper: compute dihedral forces using Blondel-Karplus formulation
    static void dihedral_forces(const Vec3& ri, const Vec3& rj,
                                const Vec3& rk, const Vec3& rl,
                                double dU_dphi,
                                Vec3& fi, Vec3& fj, Vec3& fk, Vec3& fl);
};

/**
 * Factory: create bonded model from State.B (edge list)
 * Automatically infers angles/dihedrals from bond graph
 * Uses generic harmonic parameters (for testing)
 */
std::unique_ptr<IModel> create_generic_bonded_model(const State& s);

} // namespace atomistic
