#pragma once
/**
 * body_force_model.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * EHD body force models for the three primary pumping mechanisms:
 *
 *   1. Coulomb / Ion-Drag Force:
 *        f_C = ρ_e · E
 *      Space charge ρ_e interacts with the electric field E to produce
 *      a volumetric force that drags the bulk fluid.  This is the dominant
 *      mechanism in ion-injection devices (needle-ring, corona-driven).
 *
 *   2. Dielectrophoretic (DEP) Force:
 *        f_DEP = (1/2) · ε₀ · ∇(χ · |E|²)
 *              ≈ ε₀ · Re[K(ω)] · ∇(|E|²)
 *      Arises from permittivity gradients in non-uniform fields.  K(ω) is
 *      the frequency-dependent Clausius-Mossotti factor.  Effective for
 *      particle manipulation and in geometries with strong field gradients
 *      (prism tips, constrictions).
 *
 *   3. Electroosmotic (EOF) Slip:
 *        u_slip = -(ε · ζ / μ) · E_tangential
 *      Helmholtz-Smoluchowski slip velocity at charged surfaces with an
 *      electric double layer.  ζ is the wall zeta potential.  Dominant
 *      in micro-channels with thin Debye layers (κ·H >> 1).
 *
 * Each mechanism provides:
 *   - A point-wise force or slip evaluation
 *   - A field-level initialiser that populates a force field
 *   - Characteristic scaling expressions
 *
 * All forces are in SI (N/m³ for volumetric, m/s for slip).
 */

#include "sim/ehd/ehd_types.hpp"
#include "sim/ehd/physics/flow_model.hpp"
#include "sim/ehd/physics/electrostatic_model.hpp"
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Force Cell (volumetric body force at a grid point)
// ============================================================================

struct ForceCell {
    Vec3   force;         // f (N/m³) — volumetric body force
    double magnitude = 0.0;
};

struct ForceField {
    int    nr = 0, nz = 0;
    double dr = 0.0, dz = 0.0;
    std::vector<ForceCell> cells;

    void resize(int nr_, int nz_, double dr_, double dz_) {
        nr = nr_; nz = nz_; dr = dr_; dz = dz_;
        cells.resize(static_cast<size_t>(nr * nz));
    }

    ForceCell& at(int ir, int iz) { return cells[static_cast<size_t>(ir * nz + iz)]; }
    const ForceCell& at(int ir, int iz) const { return cells[static_cast<size_t>(ir * nz + iz)]; }

    double max_force_magnitude() const {
        double fmax = 0.0;
        for (const auto& c : cells) {
            if (c.magnitude > fmax) fmax = c.magnitude;
        }
        return fmax;
    }

    double total_force_z() const {
        double sum = 0.0;
        for (const auto& c : cells) {
            sum += c.force.z;
        }
        return sum;
    }
};

// ============================================================================
// 1. Coulomb / Ion-Drag Body Force
// ============================================================================

/**
 * Coulomb body force at a point:
 *   f_C = ρ_e · E
 */
inline Vec3 coulomb_force(double rho_e, const Vec3& E) {
    return E * rho_e;
}

/**
 * Assemble the Coulomb force field from electric field data.
 * f_{ij} = ρ_{e,ij} · E_{ij}
 */
inline ForceField assemble_coulomb_field(const ElectricField& ef) {
    ForceField ff;
    ff.resize(ef.nr, ef.nz, ef.dr, ef.dz);

    for (int ir = 0; ir < ef.nr; ++ir) {
        for (int iz = 0; iz < ef.nz; ++iz) {
            const auto& ec = ef.at(ir, iz);
            Vec3 f = coulomb_force(ec.charge_density, ec.field);
            double mag = f.norm();
            ff.at(ir, iz) = {f, mag};
        }
    }
    return ff;
}

/**
 * Characteristic Coulomb pressure scale:
 *   P_C ~ ε · E² = ε · (ΔV/d)²
 */
inline double coulomb_pressure_scale(double permittivity,
                                      double delta_V,
                                      double gap) {
    if (gap <= 0.0) return 0.0;
    double E = delta_V / gap;
    return permittivity * E * E;
}

// ============================================================================
// 2. Dielectrophoretic (DEP) Force
// ============================================================================

/**
 * DEP force at a point:
 *   f_DEP = (1/2) · ε₀ · K_CM · ∇(|E|²)
 *
 * where K_CM = Re[K(ω)] is the Clausius-Mossotti factor.
 * ∇(|E|²) is computed from the electric field grid via finite differences.
 */
inline Vec3 dep_force(double epsilon_0, double K_CM,
                       const Vec3& grad_E_sq) {
    return grad_E_sq * (0.5 * epsilon_0 * K_CM);
}

/**
 * Compute ∇(|E|²) at interior grid points via central differences.
 */
inline ForceField assemble_dep_field(const ElectricField& ef,
                                      double K_CM) {
    ForceField ff;
    ff.resize(ef.nr, ef.nz, ef.dr, ef.dz);

    for (int ir = 1; ir < ef.nr - 1; ++ir) {
        for (int iz = 1; iz < ef.nz - 1; ++iz) {
            double Esq_ip = ef.at(ir + 1, iz).field.norm_sq();
            double Esq_im = ef.at(ir - 1, iz).field.norm_sq();
            double Esq_jp = ef.at(ir, iz + 1).field.norm_sq();
            double Esq_jm = ef.at(ir, iz - 1).field.norm_sq();

            Vec3 grad_Esq{
                (Esq_ip - Esq_im) / (2.0 * ef.dr),
                0.0,
                (Esq_jp - Esq_jm) / (2.0 * ef.dz)
            };

            Vec3 f = dep_force(constants::EPSILON_0, K_CM, grad_Esq);
            double mag = f.norm();
            ff.at(ir, iz) = {f, mag};
        }
    }
    return ff;
}

/**
 * Characteristic DEP velocity scale (Ramos et al.):
 *   u_DEP ~ ε · K_CM · E² · d / μ
 */
inline double dep_velocity_scale(double permittivity,
                                  double K_CM,
                                  double E_char,
                                  double length_scale,
                                  double viscosity) {
    if (viscosity <= 0.0) return 0.0;
    return permittivity * K_CM * E_char * E_char * length_scale / viscosity;
}

// ============================================================================
// 3. Electroosmotic (EOF) Slip Model
// ============================================================================

/**
 * Helmholtz-Smoluchowski slip velocity:
 *   u_slip = -(ε · ζ / μ) · E_tangential
 *
 * Returns the slip velocity magnitude (always non-negative).
 */
inline double eof_slip_velocity(double permittivity,
                                 double zeta,
                                 double viscosity,
                                 double E_tangential) {
    if (viscosity <= 0.0) return 0.0;
    return std::abs(permittivity * zeta * E_tangential / viscosity);
}

/**
 * Electroosmotic flow rate in a cylindrical capillary (thin EDL limit):
 *   Q_EOF = -(ε · ζ · E_z · π · R²) / μ
 */
inline double eof_flow_rate(double permittivity,
                             double zeta,
                             double viscosity,
                             double E_axial,
                             double radius) {
    if (viscosity <= 0.0) return 0.0;
    return std::abs(permittivity * zeta * E_axial
                  * constants::PI * radius * radius / viscosity);
}

/**
 * Debye length (characteristic EDL thickness):
 *   λ_D = √(ε · R · T / (2 · F² · c₀))
 */
inline double debye_length(double permittivity,
                            double temperature_K,
                            double concentration_mol_m3) {
    if (concentration_mol_m3 <= 0.0) return 1e30;
    return std::sqrt(permittivity * constants::GAS_CONSTANT * temperature_K
                   / (2.0 * constants::FARADAY * constants::FARADAY
                      * concentration_mol_m3));
}

/**
 * Thin-EDL validity check: κ·H >> 1
 *   κ = 1/λ_D,  returns κ·H
 */
inline double kappa_H(double debye_len, double channel_height) {
    if (debye_len <= 0.0) return 1e30;
    return channel_height / debye_len;
}

/**
 * Electroosmotic pressure head (closed-channel condition):
 *   ΔP_EOF = -(8 · ε · ζ · E_z · L) / R²     (cylindrical)
 *   ΔP_EOF = -(12 · ε · ζ · E_z · L) / H²    (planar)
 */
inline double eof_pressure_head_cylindrical(double permittivity,
                                             double zeta,
                                             double E_axial,
                                             double length,
                                             double radius) {
    if (radius <= 0.0) return 0.0;
    return std::abs(8.0 * permittivity * zeta * E_axial * length
                  / (radius * radius));
}

inline double eof_pressure_head_planar(double permittivity,
                                        double zeta,
                                        double E_axial,
                                        double length,
                                        double height) {
    if (height <= 0.0) return 0.0;
    return std::abs(12.0 * permittivity * zeta * E_axial * length
                  / (height * height));
}

// ============================================================================
// Combined Body Force (selects mechanism from parameters)
// ============================================================================

/**
 * Compute the total EHD body force for a given mechanism at a single point.
 */
inline Vec3 compute_ehd_body_force(
    PumpMechanism mechanism,
    double rho_e,
    const Vec3& E,
    const Vec3& grad_E_sq,
    double K_CM)
{
    switch (mechanism) {
        case PumpMechanism::COULOMB:
            return coulomb_force(rho_e, E);

        case PumpMechanism::DIELECTROPHORETIC:
            return dep_force(constants::EPSILON_0, K_CM, grad_E_sq);

        case PumpMechanism::ELECTROOSMOTIC:
            // EOF is a boundary condition (slip), not a volumetric force.
            // Return zero body force; the slip is applied at walls.
            return {0.0, 0.0, 0.0};
    }
    return {0.0, 0.0, 0.0};
}

} // namespace physics
} // namespace ehd
} // namespace vsepr
