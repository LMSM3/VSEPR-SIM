/**
 * sensor.hpp
 * ----------
 * Virtual Sensor System for VSEPR-SIM.
 *
 * Sensors are tiny measurement objects embedded in a simulation domain.
 * Each sensor occupies a small linear segment (two endpoints in space)
 * and reports readings when fluid or material state passes through it.
 *
 * Three sensor types:
 *   WIND     — fluid speed sensor (local velocity magnitude + direction)
 *   MATERIAL — composition sensor (element fractions, species classification)
 *   ENERGY   — energy flux sensor (thermal, chemical, electrical contributions)
 *
 * Every sensor also carries a built-in friction correction:
 *   μ_dyn ≈ 0.0014 (0.14% dynamic friction factor for fluid interaction)
 *   This small linear drag is applied to any particle crossing the sensor
 *   segment, mimicking the physical perturbation of a real instrument.
 *
 * Sensor geometry:
 *   A sensor is a line segment [p0, p1] in 3D space.
 *   Length L = |p1 - p0|.
 *   Normal direction n̂ = (p1 - p0) / L.
 *   Cross-section area is infinitesimal (point-sensor limit).
 *
 * The sensor does NOT store simulation history — it reports instantaneous
 * readings. Accumulation and time-averaging are done by the caller.
 *
 * Anti-black-box: every field public, every calculation explicit.
 *
 * Reference architecture:
 *   io/xyz_format.hpp      — XYZAtom, XYZMolecule
 *   gas2/gas2_kinetic.hpp  — transport properties (viscosity, MFP)
 *   gas2/gas2_species.hpp  — species database
 *   coarse_grain/core/bead.hpp — Bead dynamics state
 */

#pragma once

#include <string>
#include <vector>
#include <array>
#include <map>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include "core/math_vec3.hpp"

namespace vsepr {
namespace sensor {

// ============================================================================
// Physical constants (sensor-specific)
// ============================================================================

// Dynamic friction factor for fluid sensor interaction.
// This is the perturbation a sensor imparts to a crossing particle.
// ~0.14% drag on the local fluid — tiny but physically honest.
static constexpr double MU_DYNAMIC_SENSOR = 0.0014;

// ============================================================================
// Sensor types
// ============================================================================

enum class SensorType : uint8_t {
    WIND,       // Fluid speed: velocity magnitude, direction, Mach number
    MATERIAL,   // Composition: element fractions, species class, density
    ENERGY      // Energy flux: thermal, chemical, electrical
};

inline const char* sensor_type_name(SensorType t) {
    switch (t) {
        case SensorType::WIND:     return "wind";
        case SensorType::MATERIAL: return "material";
        case SensorType::ENERGY:   return "energy";
    }
    return "unknown";
}

// ============================================================================
// 3D vector — Day #56: alias to vsepr::Vec3, no local struct.
// ============================================================================

using Vec3 = vsepr::Vec3;

// ============================================================================
// Sensor reading structs
// ============================================================================

/**
 * WindReading — instantaneous fluid velocity measurement.
 */
struct WindReading {
    Vec3   velocity;           // Local fluid velocity vector (Å/fs or m/s)
    double speed       = 0.0;  // |velocity| scalar
    Vec3   direction;          // Unit vector of velocity
    double temperature = 0.0;  // Local kinetic temperature (K)
    double mach_number = 0.0;  // speed / c_sound (if c_sound known)
    double density     = 0.0;  // Local number density (particles/Å³)
    int    particle_count = 0; // Number of particles in sensor region

    // Friction drag applied to crossing particles (N or kcal/(mol·Å))
    double friction_drag = 0.0;

    bool valid() const { return particle_count > 0; }
};

/**
 * MaterialReading — local composition and species analysis.
 */
struct MaterialReading {
    // Element fractions: element symbol -> mole fraction
    struct ElementFraction {
        std::string element;
        double fraction = 0.0;
        int count = 0;
    };
    std::vector<ElementFraction> composition;

    // Aggregate
    double total_mass      = 0.0;  // amu
    double avg_charge      = 0.0;  // e
    double local_density   = 0.0;  // amu/ų
    int    total_atoms     = 0;
    std::string dominant_element;    // Element with highest fraction
    std::string species_hint;        // Inferred species formula if recognisable

    bool valid() const { return total_atoms > 0; }
};

/**
 * EnergyReading — local energy flux decomposition.
 */
struct EnergyReading {
    double thermal_energy   = 0.0;  // Kinetic energy (eV or kcal/mol)
    double chemical_energy  = 0.0;  // Bond/potential energy (eV or kcal/mol)
    double electrical_energy = 0.0; // Coulombic energy (eV or kcal/mol)
    double total_energy     = 0.0;  // Sum of above

    // Flux through sensor cross-section
    double thermal_flux     = 0.0;  // Energy/time (eV/fs)
    double chemical_flux    = 0.0;
    double electrical_flux  = 0.0;
    double total_flux       = 0.0;

    // Temperature from KE
    double kinetic_temperature = 0.0;  // K

    int particle_count = 0;

    bool valid() const { return particle_count > 0; }
};

// ============================================================================
// Sensor — the measurement object
// ============================================================================

/**
 * A Sensor is a thin linear segment [p0, p1] embedded in the simulation domain.
 *
 * Physical model:
 *   - The segment acts as an infinitesimal probe.
 *   - Any particle within `capture_radius` of the line segment contributes
 *     to the sensor reading.
 *   - Particles crossing the sensor experience a tiny drag force:
 *       F_drag = -μ_dyn * |v_normal| * v̂_normal
 *     where v_normal is the velocity component perpendicular to the segment.
 *     μ_dyn ≈ 0.0014 (0.14% dynamic friction factor).
 */
struct Sensor {
    // --- Identity ---
    uint32_t    id       = 0;
    std::string name;
    SensorType  type     = SensorType::WIND;

    // --- Geometry: line segment [p0, p1] ---
    Vec3 p0;                    // Start point (Å)
    Vec3 p1;                    // End point (Å)
    double capture_radius = 2.0; // Detection radius around segment (Å)

    // --- Friction model ---
    double mu_dynamic = MU_DYNAMIC_SENSOR;  // Dynamic friction factor (0.0014)

    // --- Derived geometry (call update_geometry() after setting p0/p1) ---
    Vec3   axis;                // Unit vector along segment
    double length = 0.0;       // Segment length (Å)
    Vec3   midpoint;           // Centre of segment

    // --- Active state ---
    bool   enabled = true;
    uint64_t sample_count = 0; // Number of readings taken

    // --- Constructors ---
    Sensor() = default;

    Sensor(uint32_t id_, const std::string& name_, SensorType type_,
           const Vec3& p0_, const Vec3& p1_, double radius = 2.0)
        : id(id_), name(name_), type(type_),
          p0(p0_), p1(p1_), capture_radius(radius)
    {
        update_geometry();
    }

    // Recompute derived geometry from endpoints
    void update_geometry() {
        Vec3 delta = p1 - p0;
        length   = delta.norm();
        axis     = delta.normalized();
        midpoint = Vec3{(p0.x+p1.x)*0.5, (p0.y+p1.y)*0.5, (p0.z+p1.z)*0.5};
    }

    // ---- Distance from a point to the line segment ----
    // Returns the minimum distance from point `q` to the segment [p0, p1].
    double distance_to_segment(const Vec3& q) const {
        Vec3 d = p1 - p0;
        double L2 = d.norm2();
        if (L2 < 1e-30) return (q - p0).norm();  // Degenerate segment

        // Parameter t ∈ [0,1] for closest point on segment
        double t = std::max(0.0, std::min(1.0, (q - p0).dot(d) / L2));
        Vec3 closest = p0 + d * t;
        return (q - closest).norm();
    }

    // ---- Check if a point is within capture range ----
    bool captures(const Vec3& q) const {
        return distance_to_segment(q) <= capture_radius;
    }

    // ---- Compute friction drag on a particle crossing the sensor ----
    // Given particle velocity v, returns the drag force vector.
    // F_drag = -μ_dyn * |v_perp| * v̂_perp
    // where v_perp = v - (v · â)â is the component perpendicular to the sensor axis.
    Vec3 friction_force(const Vec3& v) const {
        double v_par = v.dot(axis);
        Vec3 v_perp = v - axis * v_par;
        double v_perp_mag = v_perp.norm();
        if (v_perp_mag < 1e-30) return Vec3{0, 0, 0};
        Vec3 v_perp_hat = v_perp.normalized();
        return v_perp_hat * (-mu_dynamic * v_perp_mag);
    }

    // ---- Summary string ----
    std::string summary() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "[Sensor #" << id << " '" << name << "' "
           << sensor_type_name(type)
           << " L=" << length << "Å"
           << " r=" << capture_radius << "Å"
           << " μ=" << mu_dynamic
           << " at (" << midpoint.x << "," << midpoint.y << "," << midpoint.z << ")";
        if (!enabled) ss << " DISABLED";
        ss << "]";
        return ss.str();
    }
};

// ============================================================================
// Sensor measurement functions
// ============================================================================
//
// These operate on raw particle data (position, velocity, element, charge, etc.)
// and return typed readings. They are decoupled from any specific simulation
// state format — the caller extracts the arrays and passes them in.
//

/**
 * Particle data for sensor measurement.
 * Flat arrays, parallel indexed.
 */
struct ParticleSnapshot {
    std::vector<Vec3>        positions;
    std::vector<Vec3>        velocities;
    std::vector<std::string> elements;
    std::vector<double>      masses;       // amu
    std::vector<double>      charges;      // e
    std::vector<double>      pe_per_atom;  // Potential energy per atom (eV)

    size_t size() const { return positions.size(); }
};

// ---- WIND sensor measurement ----
inline WindReading measure_wind(const Sensor& s, const ParticleSnapshot& snap,
                                double c_sound = 0.0)
{
    WindReading r;
    Vec3 v_sum{0,0,0};
    double ke_sum = 0.0;

    for (size_t i = 0; i < snap.size(); ++i) {
        if (!s.captures(snap.positions[i])) continue;

        r.particle_count++;
        v_sum = v_sum + snap.velocities[i];
        double m = (i < snap.masses.size()) ? snap.masses[i] : 1.0;
        double spd = snap.velocities[i].norm();
        ke_sum += 0.5 * m * spd * spd;
    }

    if (r.particle_count > 0) {
        double inv_n = 1.0 / r.particle_count;
        r.velocity  = v_sum * inv_n;
        r.speed     = r.velocity.norm();
        r.direction = r.velocity.normalized();

        // Kinetic temperature: <KE> = (3/2) kT per particle
        // In Å²·amu/fs² units → convert to K via kB
        // kB = 0.00198721 kcal/(mol·K) = 8.31446e-7 amu·Å²/(fs²·K)
        constexpr double kB_internal = 8.31446e-7; // amu·Å²/(fs²·K)
        r.temperature = (2.0 / 3.0) * (ke_sum / r.particle_count) / kB_internal;

        if (c_sound > 0.0) r.mach_number = r.speed / c_sound;

        // Estimate local number density (particles / Å³)
        // Sensor volume ≈ π * r² * L
        double vol = 3.14159265 * s.capture_radius * s.capture_radius * std::max(s.length, 0.1);
        r.density = r.particle_count / vol;

        // Friction drag magnitude applied to each crossing particle
        Vec3 avg_drag = s.friction_force(r.velocity);
        r.friction_drag = avg_drag.norm();
    }

    return r;
}

// ---- MATERIAL sensor measurement ----
inline MaterialReading measure_material(const Sensor& s, const ParticleSnapshot& snap) {
    MaterialReading r;
    std::map<std::string, int> elem_counts;
    double mass_sum = 0.0;
    double charge_sum = 0.0;

    for (size_t i = 0; i < snap.size(); ++i) {
        if (!s.captures(snap.positions[i])) continue;

        r.total_atoms++;
        std::string elem = (i < snap.elements.size()) ? snap.elements[i] : "X";
        elem_counts[elem]++;
        double m = (i < snap.masses.size()) ? snap.masses[i] : 1.0;
        mass_sum += m;
        if (i < snap.charges.size()) charge_sum += snap.charges[i];
    }

    if (r.total_atoms > 0) {
        r.total_mass = mass_sum;
        r.avg_charge = charge_sum / r.total_atoms;

        // Build composition fractions
        int max_count = 0;
        for (const auto& [elem, count] : elem_counts) {
            MaterialReading::ElementFraction ef;
            ef.element  = elem;
            ef.count    = count;
            ef.fraction = static_cast<double>(count) / r.total_atoms;
            r.composition.push_back(ef);
            if (count > max_count) {
                max_count = count;
                r.dominant_element = elem;
            }
        }

        // Sort by fraction descending
        std::sort(r.composition.begin(), r.composition.end(),
                  [](const auto& a, const auto& b) { return a.fraction > b.fraction; });

        // Local density
        double vol = 3.14159265 * s.capture_radius * s.capture_radius * std::max(s.length, 0.1);
        r.local_density = mass_sum / vol;

        // Species hint: build formula string from composition
        std::ostringstream formula;
        for (const auto& ef : r.composition) {
            formula << ef.element;
            if (ef.count > 1) formula << ef.count;
        }
        r.species_hint = formula.str();
    }

    return r;
}

// ---- ENERGY sensor measurement ----
inline EnergyReading measure_energy(const Sensor& s, const ParticleSnapshot& snap,
                                    double dt = 1.0)
{
    EnergyReading r;
    double ke_sum = 0.0;
    double pe_sum = 0.0;
    double elec_sum = 0.0;

    for (size_t i = 0; i < snap.size(); ++i) {
        if (!s.captures(snap.positions[i])) continue;

        r.particle_count++;
        double m = (i < snap.masses.size()) ? snap.masses[i] : 1.0;
        double spd = snap.velocities[i].norm();
        ke_sum += 0.5 * m * spd * spd;

        if (i < snap.pe_per_atom.size()) pe_sum += snap.pe_per_atom[i];
        if (i < snap.charges.size()) {
            // Simple Coulombic self-energy estimate: q² / (4πε₀ r)
            // Approximate with q² * 14.4 eV·Å / capture_radius
            double q = snap.charges[i];
            elec_sum += q * q * 14.4 / std::max(s.capture_radius, 0.1);
        }
    }

    if (r.particle_count > 0) {
        // Convert KE from amu·Å²/fs² to eV: 1 amu·Å²/fs² = 103.6427 eV
        constexpr double amu_A2_fs2_to_eV = 103.6427;
        r.thermal_energy    = ke_sum * amu_A2_fs2_to_eV;
        r.chemical_energy   = pe_sum;
        r.electrical_energy = elec_sum;
        r.total_energy      = r.thermal_energy + r.chemical_energy + r.electrical_energy;

        // Flux = energy / dt
        if (dt > 0.0) {
            r.thermal_flux    = r.thermal_energy / dt;
            r.chemical_flux   = r.chemical_energy / dt;
            r.electrical_flux = r.electrical_energy / dt;
            r.total_flux      = r.total_energy / dt;
        }

        // Kinetic temperature
        constexpr double kB_eV = 8.617333e-5; // eV/K
        r.kinetic_temperature = (2.0 / 3.0) * (r.thermal_energy / r.particle_count) / kB_eV;
    }

    return r;
}

// ============================================================================
// Sensor array (multiple sensors in a domain)
// ============================================================================

struct SensorArray {
    std::vector<Sensor> sensors;

    void add(const Sensor& s) {
        sensors.push_back(s);
        sensors.back().id = static_cast<uint32_t>(sensors.size() - 1);
    }

    Sensor& operator[](size_t i) { return sensors[i]; }
    const Sensor& operator[](size_t i) const { return sensors[i]; }
    size_t size() const { return sensors.size(); }

    // Find sensor by name
    Sensor* find(const std::string& name) {
        for (auto& s : sensors) {
            if (s.name == name) return &s;
        }
        return nullptr;
    }

    // Measure all sensors against a particle snapshot
    std::vector<WindReading> measure_all_wind(const ParticleSnapshot& snap,
                                              double c_sound = 0.0) const
    {
        std::vector<WindReading> results;
        for (const auto& s : sensors) {
            if (s.enabled && s.type == SensorType::WIND)
                results.push_back(measure_wind(s, snap, c_sound));
        }
        return results;
    }

    std::vector<MaterialReading> measure_all_material(const ParticleSnapshot& snap) const {
        std::vector<MaterialReading> results;
        for (const auto& s : sensors) {
            if (s.enabled && s.type == SensorType::MATERIAL)
                results.push_back(measure_material(s, snap));
        }
        return results;
    }

    std::vector<EnergyReading> measure_all_energy(const ParticleSnapshot& snap,
                                                   double dt = 1.0) const {
        std::vector<EnergyReading> results;
        for (const auto& s : sensors) {
            if (s.enabled && s.type == SensorType::ENERGY)
                results.push_back(measure_energy(s, snap, dt));
        }
        return results;
    }

    // Summary of all sensors
    std::string summary() const {
        std::ostringstream ss;
        ss << "SensorArray [" << sensors.size() << " sensors]\n";
        for (const auto& s : sensors)
            ss << "  " << s.summary() << "\n";
        return ss.str();
    }
};

// ============================================================================
// Sensor placement helpers
// ============================================================================

// Create a wind sensor along the X axis at a given Y,Z position
inline Sensor make_wind_sensor(uint32_t id, const std::string& name,
                               double x0, double x1, double y, double z,
                               double radius = 2.0)
{
    return Sensor(id, name, SensorType::WIND,
                  Vec3{x0, y, z}, Vec3{x1, y, z}, radius);
}

// Create a material sensor along the Y axis
inline Sensor make_material_sensor(uint32_t id, const std::string& name,
                                   double y0, double y1, double x, double z,
                                   double radius = 2.0)
{
    return Sensor(id, name, SensorType::MATERIAL,
                  Vec3{x, y0, z}, Vec3{x, y1, z}, radius);
}

// Create an energy sensor along the Z axis
inline Sensor make_energy_sensor(uint32_t id, const std::string& name,
                                 double z0, double z1, double x, double y,
                                 double radius = 2.0)
{
    return Sensor(id, name, SensorType::ENERGY,
                  Vec3{x, y, z0}, Vec3{x, y, z1}, radius);
}

// Create a grid of wind sensors across a plane
inline SensorArray make_wind_grid(double x0, double x1,
                                  double y_min, double y_max, int ny,
                                  double z_min, double z_max, int nz,
                                  double radius = 2.0)
{
    SensorArray arr;
    uint32_t id = 0;
    double dy = (ny > 1) ? (y_max - y_min) / (ny - 1) : 0.0;
    double dz = (nz > 1) ? (z_max - z_min) / (nz - 1) : 0.0;

    for (int iy = 0; iy < ny; ++iy) {
        for (int iz = 0; iz < nz; ++iz) {
            double y = y_min + iy * dy;
            double z = z_min + iz * dz;
            std::string name = "wind_" + std::to_string(iy) + "_" + std::to_string(iz);
            arr.add(make_wind_sensor(id++, name, x0, x1, y, z, radius));
        }
    }
    return arr;
}

} // namespace sensor
} // namespace vsepr
