#pragma once
/**
 * disk_stack_generator.hpp
 *
 * Electrohydrodynamic Simulation — Stage 1: CAD / Geometry
 *
 * Configuration (c): Stacked perforated disk electrodes.
 * Alternating cathode/anode disks with through-hole perforations create
 * both axial (one-directional through-flow) and rotational (inter-disk
 * swirl) flow patterns.
 *
 * Geometry:
 *   - N_d disks of radius R_disk, thickness t_disk
 *   - Alternating polarity:  disk_0 = +V, disk_1 = −V, disk_2 = +V, ...
 *   - Each disk has N_perf perforations of radius r_perf
 *   - Axial spacing d_spacing between consecutive disks
 *   - Flow channels exist between adjacent disks and through perforations
 *
 * The pumping mechanism is primarily Coulomb (ion-drag), with the
 * inter-disk gaps acting as acceleration stages.
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace cad {

// ============================================================================
// Disk Stack Parameters
// ============================================================================

struct DiskStackParams {
    double disk_radius       = 8.0e-3;    // R_disk (m)
    double disk_thickness    = 0.5e-3;    // t_disk (m)
    int    disk_count        = 6;          // total disks (alternating polarity)
    double spacing           = 3.0e-3;    // axial gap between disks (m)
    int    perforations      = 12;         // through-holes per disk
    double perforation_radius = 1.0e-3;   // r_perf (m)
    int    angular_pts       = 72;         // angular resolution for disk edge
};

inline DiskStackParams from_ehd_disk_stack(const EHDParameters& p) {
    DiskStackParams ds;
    ds.disk_radius        = p.disk_radius_m;
    ds.disk_thickness     = p.disk_thickness_m;
    ds.disk_count         = p.disk_count;
    ds.spacing            = p.disk_spacing_m;
    ds.perforations       = p.perforations_per_disk;
    ds.perforation_radius = p.perforation_radius_m;
    return ds;
}

// ============================================================================
// Disk Descriptor
// ============================================================================

struct DiskDescriptor {
    int    index;           // 0-based disk number
    double z_bottom;        // axial start (m)
    double z_top;           // axial end   (m)
    bool   is_positive;     // polarity: even index = +, odd index = −
    double voltage;         // applied voltage (V)
};

/**
 * Generate the disk descriptors for the full stack.
 * Disks alternate polarity starting with positive.
 */
inline std::vector<DiskDescriptor> generate_disk_stack(
    const DiskStackParams& ds,
    double V_pos, double V_neg)
{
    std::vector<DiskDescriptor> disks;
    disks.reserve(static_cast<size_t>(ds.disk_count));

    for (int i = 0; i < ds.disk_count; ++i) {
        DiskDescriptor d;
        d.index       = i;
        d.z_bottom    = i * (ds.disk_thickness + ds.spacing);
        d.z_top       = d.z_bottom + ds.disk_thickness;
        d.is_positive = (i % 2 == 0);
        d.voltage     = d.is_positive ? V_pos : V_neg;
        disks.push_back(d);
    }
    return disks;
}

/**
 * Generate perforation centre positions for a given disk.
 * Perforations are equally spaced on a circle at radius R_disk/2.
 */
inline std::vector<Vec3> generate_perforation_centres(
    const DiskStackParams& ds,
    double z_mid)
{
    std::vector<Vec3> centres;
    centres.reserve(static_cast<size_t>(ds.perforations));

    double r_pattern = ds.disk_radius * 0.5;

    for (int i = 0; i < ds.perforations; ++i) {
        double theta = 2.0 * constants::PI * i / ds.perforations;
        centres.emplace_back(
            r_pattern * std::cos(theta),
            r_pattern * std::sin(theta),
            z_mid);
    }
    return centres;
}

/**
 * Generate the outer edge ring for a disk at a given z position.
 */
inline std::vector<Vec3> generate_disk_edge(
    const DiskStackParams& ds,
    double z)
{
    std::vector<Vec3> ring;
    ring.reserve(static_cast<size_t>(ds.angular_pts));

    for (int i = 0; i < ds.angular_pts; ++i) {
        double theta = 2.0 * constants::PI * i / ds.angular_pts;
        ring.emplace_back(
            ds.disk_radius * std::cos(theta),
            ds.disk_radius * std::sin(theta),
            z);
    }
    return ring;
}

/**
 * Total stack length including all disks and gaps.
 */
inline double stack_total_length(const DiskStackParams& ds) {
    if (ds.disk_count <= 0) return 0.0;
    return ds.disk_count * ds.disk_thickness
         + (ds.disk_count - 1) * ds.spacing;
}

/**
 * Open area fraction: ratio of perforation area to total disk area.
 */
inline double open_area_fraction(const DiskStackParams& ds) {
    double A_perf = ds.perforations * constants::PI
                  * ds.perforation_radius * ds.perforation_radius;
    double A_disk = constants::PI * ds.disk_radius * ds.disk_radius;
    if (A_disk <= 0.0) return 0.0;
    return A_perf / A_disk;
}

/**
 * Inter-disk field estimate (uniform between parallel disks):
 *   E ≈ ΔV / d_spacing
 */
inline double inter_disk_field(double delta_V, double spacing) {
    if (spacing <= 0.0) return 0.0;
    return delta_V / spacing;
}

} // namespace cad
} // namespace ehd
} // namespace vsepr
