#include "bonded.hpp"
#include <cmath>
#include <algorithm>
#include <set>
#include <map>

namespace atomistic {

/**
 * Compute dihedral angle φ ∈ [-π, π] for atoms i-j-k-l
 * 
 * Geometry:
 * - b1 = rj - ri (bond i→j)
 * - b2 = rk - rj (bond j→k, rotation axis)
 * - b3 = rl - rk (bond k→l)
 * - n1 = b1 × b2 (normal to plane ijk)
 * - n2 = b2 × b3 (normal to plane jkl)
 * - φ = atan2(b2·(n1×n2)/|b2|, n1·n2)
 * 
 * Sign convention: looking down j→k axis, φ > 0 for clockwise rotation of l
 */
double BondedModel::compute_dihedral_angle(const Vec3& ri, const Vec3& rj,
                                            const Vec3& rk, const Vec3& rl) {
    Vec3 b1 = rj - ri;
    Vec3 b2 = rk - rj;
    Vec3 b3 = rl - rk;
    
    // Normals to planes
    Vec3 n1 = {b1.y*b2.z - b1.z*b2.y,
               b1.z*b2.x - b1.x*b2.z,
               b1.x*b2.y - b1.y*b2.x};  // b1 × b2
    
    Vec3 n2 = {b2.y*b3.z - b2.z*b3.y,
               b2.z*b3.x - b2.x*b3.z,
               b2.x*b3.y - b2.y*b3.x};  // b2 × b3
    
    double y = dot(b2, {n1.y*n2.z - n1.z*n2.y,
                        n1.z*n2.x - n1.x*n2.z,
                        n1.x*n2.y - n1.y*n2.x});  // b2·(n1×n2)
    y /= norm(b2);
    
    double x = dot(n1, n2);
    
    return std::atan2(y, x);
}

/**
 * Dihedral force distribution using Blondel-Karplus formulation
 * 
 * For U(φ), where φ = φ(ri,rj,rk,rl), the forces are:
 * fi = -∂U/∂ri = -(dU/dφ)(∂φ/∂ri)
 * 
 * Geometric derivatives (Blondel & Karplus 1996):
 * ∂φ/∂ri = -(|b2|/|n1|²) n1
 * ∂φ/∂rj = +(|b2|-|b1|cosθ1)/(|b2||n1|²) n1 - (cosθ2)/(|b2||n2|²) n2
 * ∂φ/∂rk = -(cosθ1)/(|b2||n1|²) n1 + (|b2|-|b3|cosθ2)/(|b2||n2|²) n2
 * ∂φ/∂rl = +(|b2|/|n2|²) n2
 * 
 * where cosθ1 = b1·b2/|b1||b2|, cosθ2 = b2·b3/|b2||b3|
 */
void BondedModel::dihedral_forces(const Vec3& ri, const Vec3& rj,
                                   const Vec3& rk, const Vec3& rl,
                                   double dU_dphi,
                                   Vec3& fi, Vec3& fj, Vec3& fk, Vec3& fl) {
    Vec3 b1 = rj - ri;
    Vec3 b2 = rk - rj;
    Vec3 b3 = rl - rk;
    
    double r1 = norm(b1);
    double r2 = norm(b2);
    double r3 = norm(b3);
    
    if (r1 < 1e-10 || r2 < 1e-10 || r3 < 1e-10) {
        fi = fj = fk = fl = {0, 0, 0};
        return;
    }
    
    Vec3 n1 = {b1.y*b2.z - b1.z*b2.y,
               b1.z*b2.x - b1.x*b2.z,
               b1.x*b2.y - b1.y*b2.x};
    
    Vec3 n2 = {b2.y*b3.z - b2.z*b3.y,
               b2.z*b3.x - b2.x*b3.z,
               b2.x*b3.y - b2.y*b3.x};
    
    double n1_sq = dot(n1, n1);
    double n2_sq = dot(n2, n2);
    
    if (n1_sq < 1e-12 || n2_sq < 1e-12) {
        fi = fj = fk = fl = {0, 0, 0};
        return;
    }
    
    double cos_theta1 = dot(b1, b2) / (r1 * r2);
    double cos_theta2 = dot(b2, b3) / (r2 * r3);
    
    // Blondel-Karplus coefficients
    double a1 = -r2 / n1_sq;
    double a2 = (r2 - r1*cos_theta1) / (r2*n1_sq);
    double a3 = cos_theta1 / (r2*n1_sq);
    double a4 = cos_theta2 / (r2*n2_sq);
    double a5 = (r2 - r3*cos_theta2) / (r2*n2_sq);
    double a6 = r2 / n2_sq;
    
    fi = n1 * (a1 * dU_dphi);
    fj = n1 * (a2 * dU_dphi) + n2 * (a4 * dU_dphi);
    fk = n1 * (-a3 * dU_dphi) + n2 * (a5 * dU_dphi);
    fl = n2 * (a6 * dU_dphi);
}

double BondedModel::eval_bonds(State& s) const {
    double U = 0;
    for (const auto& bond : topology.bonds) {
        Vec3 rij = s.X[bond.i] - s.X[bond.j];
        double r = norm(rij);
        double dr = r - bond.r0;
        
        // U = kb(r - r0)²
        U += bond.kb * dr * dr;
        
        // F = -2kb(r-r0)r̂
        if (r > 1e-12) {
            Vec3 f = rij * (-2.0 * bond.kb * dr / r);
            s.F[bond.i] = s.F[bond.i] + f;
            s.F[bond.j] = s.F[bond.j] - f;
        }
    }
    return U;
}

double BondedModel::eval_angles(State& s) const {
    double U = 0;
    for (const auto& ang : topology.angles) {
        Vec3 rij = s.X[ang.i] - s.X[ang.j];  // j is vertex
        Vec3 rkj = s.X[ang.k] - s.X[ang.j];
        
        double rij_len = norm(rij);
        double rkj_len = norm(rkj);
        
        if (rij_len < 1e-10 || rkj_len < 1e-10) continue;
        
        double cos_theta = dot(rij, rkj) / (rij_len * rkj_len);
        cos_theta = std::max(-1.0, std::min(1.0, cos_theta));  // Clamp for acos
        double theta = std::acos(cos_theta);
        double dtheta = theta - ang.theta0;
        
        // U = kθ(θ - θ0)²
        U += ang.ktheta * dtheta * dtheta;
        
        // Force: F = -∂U/∂r = -2kθ(θ-θ0) ∂θ/∂r
        // ∂θ/∂ri = (1/sinθ)[rkj/(rij·rkj) - rij cosθ/(rij²)]
        double sin_theta = std::sin(theta);
        if (std::abs(sin_theta) < 1e-6) continue;  // Linear angle, skip
        
        double k = -2.0 * ang.ktheta * dtheta / sin_theta;
        
        Vec3 fi = (rkj * (1.0/(rij_len*rkj_len)) - rij * (cos_theta/(rij_len*rij_len))) * k;
        Vec3 fk = (rij * (1.0/(rij_len*rkj_len)) - rkj * (cos_theta/(rkj_len*rkj_len))) * k;
        Vec3 fj = (fi + fk) * (-1.0);
        
        s.F[ang.i] = s.F[ang.i] + fi;
        s.F[ang.j] = s.F[ang.j] + fj;
        s.F[ang.k] = s.F[ang.k] + fk;
    }
    return U;
}

double BondedModel::eval_dihedrals(State& s) const {
    double U = 0;
    for (const auto& dih : topology.dihedrals) {
        double phi = compute_dihedral_angle(s.X[dih.i], s.X[dih.j],
                                             s.X[dih.k], s.X[dih.l]);
        
        // U = Vn[1 + cos(nφ - γ)]
        double arg = dih.n * phi - dih.gamma;
        U += dih.Vn * (1.0 + std::cos(arg));
        
        // dU/dφ = -Vn·n·sin(nφ - γ)
        double dU_dphi = -dih.Vn * dih.n * std::sin(arg);
        
        Vec3 fi, fj, fk, fl;
        dihedral_forces(s.X[dih.i], s.X[dih.j], s.X[dih.k], s.X[dih.l],
                        dU_dphi, fi, fj, fk, fl);
        
        s.F[dih.i] = s.F[dih.i] + fi;
        s.F[dih.j] = s.F[dih.j] + fj;
        s.F[dih.k] = s.F[dih.k] + fk;
        s.F[dih.l] = s.F[dih.l] + fl;
    }
    return U;
}

double BondedModel::eval_impropers(State& s) const {
    double U = 0;
    for (const auto& imp : topology.impropers) {
        // Improper: i-j-k-l where j is central, i,k,l define plane
        // Out-of-plane angle ψ: angle between j-k and plane(i,j,l)
        // Simplified: use dihedral formulation with harmonic potential
        
        double phi = compute_dihedral_angle(s.X[imp.i], s.X[imp.j],
                                             s.X[imp.k], s.X[imp.l]);
        double dpsi = phi - imp.psi0;
        
        // Wrap to [-π,π]
        while (dpsi > M_PI) dpsi -= 2*M_PI;
        while (dpsi < -M_PI) dpsi += 2*M_PI;
        
        // U = kimp(ψ - ψ0)²
        U += imp.kimp * dpsi * dpsi;
        
        // dU/dψ = 2kimp(ψ - ψ0)
        double dU_dphi = 2.0 * imp.kimp * dpsi;
        
        Vec3 fi, fj, fk, fl;
        dihedral_forces(s.X[imp.i], s.X[imp.j], s.X[imp.k], s.X[imp.l],
                        dU_dphi, fi, fj, fk, fl);
        
        s.F[imp.i] = s.F[imp.i] + fi;
        s.F[imp.j] = s.F[imp.j] + fj;
        s.F[imp.k] = s.F[imp.k] + fk;
        s.F[imp.l] = s.F[imp.l] + fl;
    }
    return U;
}

void BondedModel::eval(State& s, const ModelParams& p) const {
    (void)p;  // Bonded model doesn't use generic params
    
    // Initialize forces to zero
    std::fill(s.F.begin(), s.F.end(), Vec3{0,0,0});
    s.E = {};
    
    // Evaluate all bonded terms
    s.E.Ubond = eval_bonds(s);
    s.E.Uangle = eval_angles(s);
    s.E.Utors = eval_dihedrals(s);
    // Impropers contribute to Utors
    s.E.Utors += eval_impropers(s);
}

void BondedTopology::generate_angles_from_bonds() {
    // Build adjacency list from bonds
    std::map<uint32_t, std::set<uint32_t>> adj;
    for (const auto& bond : bonds) {
        adj[bond.i].insert(bond.j);
        adj[bond.j].insert(bond.i);
    }
    
    // For each atom j, find all pairs (i,k) bonded to j
    for (const auto& [j, neighbors] : adj) {
        std::vector<uint32_t> nbr(neighbors.begin(), neighbors.end());
        for (size_t a = 0; a < nbr.size(); ++a) {
            for (size_t b = a+1; b < nbr.size(); ++b) {
                uint32_t i = nbr[a];
                uint32_t k = nbr[b];
                // Default: 60 kcal/mol/rad², 109.5° (tetrahedral)
                angles.emplace_back(i, j, k, 60.0, 1.91);  // 109.5° ≈ 1.91 rad
            }
        }
    }
}

void BondedTopology::generate_dihedrals_from_bonds() {
    // Build adjacency list
    std::map<uint32_t, std::set<uint32_t>> adj;
    for (const auto& bond : bonds) {
        adj[bond.i].insert(bond.j);
        adj[bond.j].insert(bond.i);
    }
    
    // Find all j-k bonds
    for (const auto& jk_bond : bonds) {
        uint32_t j = jk_bond.i;
        uint32_t k = jk_bond.j;
        
        // Find all i bonded to j (but not k)
        for (uint32_t i : adj[j]) {
            if (i == k) continue;
            
            // Find all l bonded to k (but not j)
            for (uint32_t l : adj[k]) {
                if (l == j || l == i) continue;
                
                // Default: 3-fold barrier, 1.5 kcal/mol (C-C rotation)
                dihedrals.emplace_back(i, j, k, l, 3, 1.5, 0.0);
            }
        }
    }
}

void BondedTopology::assign_default_parameters(const State& s) {
(void)s;  // Reserved for element-type-based parameter assignment
    
// Assign element-type-based defaults for bonds
for (auto& bond : bonds) {
        // Simplified: assume all bonds are C-C like
        bond.kb = 310.0;   // kcal/mol/Ų
        bond.r0 = 1.54;    // Å (C-C single bond)
        
        // Could refine based on s.type[bond.i], s.type[bond.j]
    }
    
    // Angles already have defaults from generation
    // Dihedrals already have defaults from generation
}

std::unique_ptr<IModel> create_generic_bonded_model(const State& s) {
    BondedTopology topo;
    
    // Convert Edge list to BondParams
    for (const auto& edge : s.B) {
        topo.bonds.emplace_back(edge.i, edge.j, 310.0, 1.54);
    }
    
    // Auto-generate angles and dihedrals
    topo.generate_angles_from_bonds();
    topo.generate_dihedrals_from_bonds();
    
    return std::make_unique<BondedModel>(topo);
}

} // namespace atomistic
