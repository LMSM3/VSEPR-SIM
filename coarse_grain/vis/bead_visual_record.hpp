#pragma once
/**
 * bead_visual_record.hpp — Visualization Adapter for Coarse-Grained Beads
 *
 * Defines the BeadVisualRecord: a read-only diagnostic snapshot that
 * converts the internal bead data model into renderable primitives.
 * This is the one-way bridge between the solver/descriptor pipeline
 * and the post-computation visualization layer.
 *
 * Data domains:
 *   1. Effective (atomistic) domain — radius, anisotropy, confidence
 *   2. Structural domain — solved direction vectors, planar/ring normals,
 *      coordination spokes, local frame
 *   3. Residual domain — mapping error, descriptor residual, frame
 *      ambiguity, compression mismatch
 *
 * The renderer consumes BeadVisualRecords. It never re-solves bead
 * geometry or infers VSEPR structure. That work belongs upstream.
 *
 * Anti-black-box: every field is explicitly inspectable. No hidden
 * state, no inferred geometry that was not produced by the solver.
 *
 * Architecture:
 *   Bead + FragmentView + UnifiedDescriptor → build_visual_record()
 *                                           → BeadVisualRecord
 *                                           → renderer primitives
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/fragment_view.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {
namespace vis {

// ============================================================================
// Frame Provenance
// ============================================================================

/**
 * FrameMethod — how the local coordinate frame was constructed.
 *
 * The renderer may style the frame axes differently depending on
 * provenance (e.g. dashed axes for fallback methods).
 */
enum class FrameMethod {
    InertiaFit,         // Principal-axis decomposition (standard)
    RingPlaneFit,       // Plane fit through ring members
    MetalCoordination,  // Coordination-sphere fit around metal center
    PCA_Fallback,       // Generic PCA (lower confidence)
    SingleAtom,         // Degenerate: single atom, no meaningful frame
    Unknown             // No frame information available
};

inline const char* frame_method_name(FrameMethod m) {
    switch (m) {
        case FrameMethod::InertiaFit:        return "Inertia fit";
        case FrameMethod::RingPlaneFit:      return "Ring plane fit";
        case FrameMethod::MetalCoordination: return "Metal coordination";
        case FrameMethod::PCA_Fallback:      return "PCA fallback";
        case FrameMethod::SingleAtom:        return "Single atom";
        case FrameMethod::Unknown:           return "Unknown";
        default:                             return "?";
    }
}

// ============================================================================
// View Mode
// ============================================================================

/**
 * ViewMode — visual inspection depth for the bead.
 *
 * Each mode reveals a different level of internal structure.
 * All modes read from the same BeadVisualRecord.
 */
enum class ViewMode {
    Shell,          // Layer 1: Outer shell + optional surface patches (scene overview)
    Scaffold,       // Shell + local frame triad + interior scaffold
    SurfaceState,   // Layer 2: Spherical heatmap of effective directional state
    InternalRef,    // Layer 3: Internal structural reference (atomistic fragment)
    Skeleton,       // Shell with reduced source-fragment stick model inside
    Cutaway,        // Clipping plane removes part of shell to expose interior
    Residual,       // Error bands, mismatch halos, displacement vectors
    Comparison      // Side-by-side fragment geometry vs compressed bead (L2+L3)
};

inline const char* view_mode_name(ViewMode m) {
    switch (m) {
        case ViewMode::Shell:        return "Shell";
        case ViewMode::Scaffold:     return "Scaffold";
        case ViewMode::SurfaceState: return "Surface State";
        case ViewMode::InternalRef:  return "Internal Ref";
        case ViewMode::Skeleton:     return "Skeleton";
        case ViewMode::Cutaway:      return "Cutaway";
        case ViewMode::Residual:     return "Residual";
        case ViewMode::Comparison:   return "Comparison";
        default:                     return "?";
    }
}

// ============================================================================
// Scale Level (multi-scale navigation)
// ============================================================================

/**
 * ScaleLevel — the current inspection zoom level.
 *
 * Navigation descends continuously through these levels:
 *   SceneCloud → Cluster → BeadDetail → BeadInterior → FragmentCompare
 */
enum class ScaleLevel {
    SceneCloud,       // All beads, scalar overlay, neighbour lines
    Cluster,          // Focus bead + first shell, rest faded
    BeadDetail,       // Shell + local frame + descriptor patches
    BeadInterior,     // Cutaway/clipping, scaffold from solved directions
    FragmentCompare   // Source fragment miniature vs compressed bead
};

inline const char* scale_level_name(ScaleLevel s) {
    switch (s) {
        case ScaleLevel::SceneCloud:      return "Scene cloud";
        case ScaleLevel::Cluster:         return "Cluster";
        case ScaleLevel::BeadDetail:      return "Bead detail";
        case ScaleLevel::BeadInterior:    return "Bead interior";
        case ScaleLevel::FragmentCompare: return "Fragment compare";
        default:                          return "?";
    }
}

// ============================================================================
// Source Fragment Miniature
// ============================================================================

/**
 * SourceAtomVis — minimal atom record for source fragment visualization.
 */
struct SourceAtomVis {
    atomistic::Vec3 position{};     // World-space position (Angstrom)
    int   atomic_number{};          // Element
    float radius{};                 // Van der Waals radius for rendering
    uint32_t flags{};               // AtomRecord flags (aromatic, ring, metal, etc.)
};

/**
 * SourceBondVis — minimal bond record for fragment stick model.
 */
struct SourceBondVis {
    int i{};            // Index into source_atoms (local)
    int j{};
    int order{1};       // Bond order
};

/**
 * SourceFragmentMiniature — the original atomistic fragment,
 * stored for comparison/overlay rendering.
 */
struct SourceFragmentMiniature {
    std::vector<SourceAtomVis> atoms;
    std::vector<SourceBondVis> bonds;
    atomistic::Vec3 center_of_mass{};
    atomistic::Vec3 center_of_geometry{};
    bool populated{false};
};

// ============================================================================
// Surface-State Channel Selection (Layer 2)
// ============================================================================

/**
 * SurfaceStateChannel — selects which descriptor channel to render
 * on the Layer 2 spherical heatmap.
 */
enum class SurfaceStateChannel {
    Steric,             // k=0: geometric accessibility / excluded volume
    Electrostatic,      // k=1: charge-field structure
    Dispersion,         // k=2: attractive strength distribution
    Synthesized         // Weighted combination: w_s*S0 + w_e*S1 + w_d*S2
};

inline const char* surface_state_channel_name(SurfaceStateChannel ch) {
    switch (ch) {
        case SurfaceStateChannel::Steric:        return "Steric";
        case SurfaceStateChannel::Electrostatic: return "Electrostatic";
        case SurfaceStateChannel::Dispersion:    return "Dispersion";
        case SurfaceStateChannel::Synthesized:   return "Synthesized";
        default:                                 return "?";
    }
}

// ============================================================================
// Descriptor Surface Patch
// ============================================================================

/**
 * SurfacePatchSample — one directional sample of the descriptor field.
 *
 * The renderer uses an array of these to paint the shell surface
 * as a heatmap or glyph field.
 */
struct SurfacePatchSample {
    atomistic::Vec3 direction_local{};  // Unit direction in bead-local frame
    double steric_value{};              // Steric channel value at this direction
    double electrostatic_value{};       // Electrostatic channel value
    double dispersion_value{};          // Dispersion channel value
};

// ============================================================================
// Layer 2: Effective Surface-State Grid
// ============================================================================

/**
 * SurfaceStateVertex — one vertex of the Layer 2 spherical heatmap grid.
 *
 * Stores angular coordinates, direction vector, and per-channel field
 * values. Used to render the bead's directional effective state as a
 * coloured quad-strip tessellation over the sphere.
 */
struct SurfaceStateVertex {
    double theta{};                     // Polar angle [0, π]
    double phi{};                       // Azimuthal angle [0, 2π)
    atomistic::Vec3 direction_local{};  // Unit direction in bead-local frame
    double steric{};                    // Steric channel S^(0)(θ,φ)
    double electrostatic{};             // Electrostatic channel S^(1)(θ,φ)
    double dispersion{};                // Dispersion channel S^(2)(θ,φ)
    double synthesized{};               // Weighted effective: w_s*S0 + w_e*S1 + w_d*S2
};

/**
 * SurfaceStateGrid — structured angular grid for Layer 2 rendering.
 *
 * The grid stores (n_theta+1) × n_phi vertices in row-major order
 * (theta varies slowest). This enables direct quad-strip rendering
 * by iterating over adjacent theta bands.
 *
 * Vertex index: grid[i * n_phi + j] where i ∈ [0, n_theta], j ∈ [0, n_phi)
 */
struct SurfaceStateGrid {
    std::vector<SurfaceStateVertex> vertices;
    int n_theta{};                      // Number of theta divisions
    int n_phi{};                        // Number of phi divisions
    bool populated{false};

    /// Per-channel value ranges (for colourmap normalisation)
    double steric_min{}, steric_max{};
    double elec_min{}, elec_max{};
    double disp_min{}, disp_max{};
    double synth_min{}, synth_max{};

    /// Vertex access: row i (theta band), column j (phi slice)
    const SurfaceStateVertex& at(int i, int j) const {
        return vertices[i * n_phi + (j % n_phi)];
    }

    /// Get the active channel value for a vertex
    double channel_value(const SurfaceStateVertex& v,
                         SurfaceStateChannel ch) const {
        switch (ch) {
            case SurfaceStateChannel::Steric:        return v.steric;
            case SurfaceStateChannel::Electrostatic: return v.electrostatic;
            case SurfaceStateChannel::Dispersion:    return v.dispersion;
            case SurfaceStateChannel::Synthesized:   return v.synthesized;
            default:                                 return v.steric;
        }
    }

    /// Get the value range for a given channel
    void channel_range(SurfaceStateChannel ch,
                       double& vmin, double& vmax) const {
        switch (ch) {
            case SurfaceStateChannel::Steric:
                vmin = steric_min; vmax = steric_max; break;
            case SurfaceStateChannel::Electrostatic:
                vmin = elec_min; vmax = elec_max; break;
            case SurfaceStateChannel::Dispersion:
                vmin = disp_min; vmax = disp_max; break;
            case SurfaceStateChannel::Synthesized:
                vmin = synth_min; vmax = synth_max; break;
        }
    }
};

// ============================================================================
// Layer 3: Internal Structural Reference Options
// ============================================================================

/**
 * InternalRefOptions — rendering options for Layer 3 display.
 *
 * Controls how the internal structural reference is presented
 * within the bead shell.
 */
struct InternalRefOptions {
    bool show_atoms{true};              // Draw atom spheres
    bool show_bonds{true};              // Draw bond sticks
    bool show_orientation_markers{true};// Draw fragment local frame axes
    bool element_coloring{true};        // CPK element colours (vs uniform)
    float atom_scale{0.25f};            // Fraction of VdW radius
    float bond_width{1.5f};             // Bond line width
};

// ============================================================================
// BeadVisualRecord — the adapter object
// ============================================================================

/**
 * BeadVisualRecord — complete visual representation of one CG bead.
 *
 * Built from the solved bead data by build_visual_record().
 * The renderer consumes this record to produce all view modes
 * without re-solving any geometry.
 *
 * Three data domains:
 *   1. Effective (atomistic) — radius, anisotropy, confidence
 *   2. Structural — solved directions, planar/ring flags, frame
 *   3. Residual — mapping error, descriptor residual, frame ambiguity
 */
struct BeadVisualRecord {
    // --- Identity ---
    int         bead_id{-1};
    std::string bead_class;             // Type name (e.g. "BB", "SC1", "W")
    std::string fragment_name;          // Source fragment label

    // --- Effective (atomistic) domain ---
    atomistic::Vec3 center_world{};     // Bead center in world space
    double effective_radius{1.0};       // Shell radius (Angstrom)
    atomistic::Vec3 anisotropy_axes{};  // Principal deformation (eigenvalue ratios)
    double mapping_confidence{1.0};     // 0=unknown, 1=perfect mapping
    double mapping_residual{};          // |COM - COG| distance (Angstrom)

    // --- Local frame ---
    Mat3         local_frame{};         // 3x3 rotation (columns = axes)
    FrameMethod  frame_method{FrameMethod::Unknown};
    double       frame_confidence{};    // 0=degenerate, 1=well-conditioned
    bool         frame_valid{false};

    // --- Structural domain (solved geometry) ---
    std::vector<atomistic::Vec3> solved_directions_local;   // Direction vectors in local frame
    bool   has_planar_component{false};
    atomistic::Vec3 planar_normal_local{};     // Ring/planar normal in local frame
    bool   is_cyclic{false};
    bool   is_metal_centered{false};
    bool   is_aromatic{false};
    int    coordination_number{};               // Number of solved directions

    // --- Descriptor field (surface patches) ---
    std::vector<SurfacePatchSample> descriptor_field;
    int    descriptor_l_max{};
    bool   has_descriptor{false};

    // --- Residual domain ---
    double descriptor_residual{1.0};    // Reconstruction residual (0=perfect)
    double geometry_distortion{};       // Deviation from ideal VSEPR class
    double frame_ambiguity{};           // How degenerate the eigenvalues were

    // --- Source fragment (for comparison mode / Layer 3) ---
    SourceFragmentMiniature source_fragment;

    // --- Layer 2: Effective Surface-State Grid ---
    SurfaceStateGrid surface_state_grid;
    SurfaceStateChannel active_surface_channel{SurfaceStateChannel::Steric};
    float surface_state_opacity{0.85f};  // Heatmap opacity
    float radial_exaggeration{0.0f};     // 0 = pure sphere, >0 = radial scaling by value

    // --- Layer 3: Internal Structural Reference ---
    InternalRefOptions internal_ref_options;

    // --- Environment state snapshot ---
    double eta{};
    double rho{};
    double target_f{};
    double P2{};
    double coordination_C{};

    // --- Connectivity ---
    struct NeighbourLink {
        int    neighbour_bead_id{};
        double distance{};
        double interaction_strength{};
        std::string interaction_type;   // "face-to-face", "edge-to-face", etc.
    };
    std::vector<NeighbourLink> neighbours;
};

// ============================================================================
// SH Evaluation Helper
// ============================================================================

/**
 * Evaluate SH expansion at (theta, phi) for a given coefficient vector.
 * Lightweight inline version that uses the project's SH infrastructure.
 */
inline double evaluate_sh_expansion_at(
    const std::vector<double>& coeffs,
    int l_max,
    double theta,
    double phi)
{
    auto Y = coarse_grain::evaluate_all_harmonics_dynamic(theta, phi, l_max);
    double val = 0.0;
    int n = std::min(static_cast<int>(coeffs.size()), static_cast<int>(Y.size()));
    for (int i = 0; i < n; ++i) {
        val += coeffs[i] * Y[i];
    }
    return val;
}

// ============================================================================
// Builder: Bead → BeadVisualRecord
// ============================================================================

/**
 * Estimate Van der Waals radius from atomic number (simple lookup).
 */
inline float vdw_radius(int Z) {
    // Simplified radii for common elements (Angstrom)
    switch (Z) {
        case 1:  return 1.20f;  // H
        case 6:  return 1.70f;  // C
        case 7:  return 1.55f;  // N
        case 8:  return 1.52f;  // O
        case 9:  return 1.47f;  // F
        case 15: return 1.80f;  // P
        case 16: return 1.80f;  // S
        case 17: return 1.75f;  // Cl
        case 35: return 1.85f;  // Br
        case 53: return 1.98f;  // I
        default: return 1.70f;  // Default
    }
}

/**
 * Build a BeadVisualRecord from bead data and an optional fragment view.
 *
 * This is the primary conversion point. The record inherits all
 * solved geometry from the bead and fragment without re-solving.
 *
 * @param bead       The coarse-grained bead
 * @param bead_id    Index in the bead system
 * @param frag       Optional source fragment (null if not available)
 * @param env        Optional environment state snapshot
 * @return Complete visual record ready for rendering
 */
inline BeadVisualRecord build_visual_record(
    const Bead& bead,
    int bead_id,
    const atomistic::FragmentView* frag = nullptr,
    const EnvironmentState* env = nullptr)
{
    BeadVisualRecord rec;
    rec.bead_id = bead_id;
    rec.center_world = bead.position;
    rec.mapping_residual = bead.mapping_residual;

    // --- Effective domain ---
    // Use surface descriptor frame asphericity for effective radius scaling
    if (bead.has_surface_data()) {
        const auto& surf = *bead.surface;
        rec.effective_radius = surf.probe_radius > 0 ? surf.probe_radius : 1.0;
        rec.anisotropy_axes = {
            surf.frame.eigenvalues[0],
            surf.frame.eigenvalues[1],
            surf.frame.eigenvalues[2]
        };
    } else {
        // Fallback: estimate from mass
        rec.effective_radius = std::cbrt(bead.mass / 12.0) * 1.5;
        if (rec.effective_radius < 0.5) rec.effective_radius = 0.5;
        if (rec.effective_radius > 4.0) rec.effective_radius = 4.0;
    }

    // --- Local frame ---
    if (bead.has_surface_data() && bead.surface->frame.valid) {
        const auto& f = bead.surface->frame;
        rec.local_frame(0, 0) = f.axis1.x; rec.local_frame(0, 1) = f.axis2.x; rec.local_frame(0, 2) = f.axis3.x;
        rec.local_frame(1, 0) = f.axis1.y; rec.local_frame(1, 1) = f.axis2.y; rec.local_frame(1, 2) = f.axis3.y;
        rec.local_frame(2, 0) = f.axis1.z; rec.local_frame(2, 1) = f.axis2.z; rec.local_frame(2, 2) = f.axis3.z;
        rec.frame_valid = true;
        rec.frame_method = FrameMethod::InertiaFit;

        // Frame confidence from eigenvalue separation
        double I1 = f.eigenvalues[0], I2 = f.eigenvalues[1], I3 = f.eigenvalues[2];
        double Isum = I1 + I2 + I3;
        if (Isum > 1e-20) {
            double spread = (I3 - I1) / Isum;
            rec.frame_confidence = std::min(1.0, spread * 3.0);
        }
        rec.frame_ambiguity = 1.0 - rec.frame_confidence;
    } else if (bead.has_orientation) {
        // Minimal frame from orientation normal
        const auto& n = bead.orientation.normal;
        rec.local_frame(0, 2) = n.x;
        rec.local_frame(1, 2) = n.y;
        rec.local_frame(2, 2) = n.z;
        // Build orthogonal axes via Gram-Schmidt
        atomistic::Vec3 up = (std::abs(n.y) < 0.9) ?
            atomistic::Vec3{0, 1, 0} : atomistic::Vec3{1, 0, 0};
        double dot = up.x * n.x + up.y * n.y + up.z * n.z;
        atomistic::Vec3 a1 = {up.x - dot * n.x, up.y - dot * n.y, up.z - dot * n.z};
        double len = std::sqrt(a1.x*a1.x + a1.y*a1.y + a1.z*a1.z);
        if (len > 1e-15) { a1.x /= len; a1.y /= len; a1.z /= len; }
        atomistic::Vec3 a2 = {
            n.y * a1.z - n.z * a1.y,
            n.z * a1.x - n.x * a1.z,
            n.x * a1.y - n.y * a1.x
        };
        rec.local_frame(0, 0) = a1.x; rec.local_frame(0, 1) = a2.x;
        rec.local_frame(1, 0) = a1.y; rec.local_frame(1, 1) = a2.y;
        rec.local_frame(2, 0) = a1.z; rec.local_frame(2, 1) = a2.z;
        rec.frame_valid = true;
        rec.frame_method = FrameMethod::PCA_Fallback;
        rec.frame_confidence = 0.5;
        rec.frame_ambiguity = 0.5;
    }

    // --- Structural domain from fragment ---
    if (frag && frag->is_valid()) {
        rec.is_cyclic = frag->cyclic;
        rec.is_aromatic = frag->aromatic;
        rec.is_metal_centered = frag->organometallic;
        rec.fragment_name = "fragment";

        // Classify frame method based on fragment properties
        if (frag->cyclic && rec.frame_valid) {
            rec.frame_method = FrameMethod::RingPlaneFit;
        } else if (frag->organometallic && rec.frame_valid) {
            rec.frame_method = FrameMethod::MetalCoordination;
        }
        if (frag->num_atoms() <= 1) {
            rec.frame_method = FrameMethod::SingleAtom;
            rec.frame_confidence = 0.0;
        }

        // Build solved direction vectors from bond geometry
        atomistic::Vec3 com = frag->center_of_mass();
        for (const auto& atom : frag->atoms) {
            atomistic::Vec3 dir = {
                atom.position.x - com.x,
                atom.position.y - com.y,
                atom.position.z - com.z
            };
            double len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
            if (len > 1e-10) {
                dir.x /= len; dir.y /= len; dir.z /= len;
                // Transform to local frame if available
                if (rec.frame_valid) {
                    atomistic::Vec3 local_dir = {
                        rec.local_frame(0,0)*dir.x + rec.local_frame(1,0)*dir.y + rec.local_frame(2,0)*dir.z,
                        rec.local_frame(0,1)*dir.x + rec.local_frame(1,1)*dir.y + rec.local_frame(2,1)*dir.z,
                        rec.local_frame(0,2)*dir.x + rec.local_frame(1,2)*dir.y + rec.local_frame(2,2)*dir.z
                    };
                    rec.solved_directions_local.push_back(local_dir);
                } else {
                    rec.solved_directions_local.push_back(dir);
                }
            }
        }
        rec.coordination_number = static_cast<int>(rec.solved_directions_local.size());

        // Planar detection: if cyclic/aromatic, use frame axis3 as planar normal
        if ((frag->cyclic || frag->aromatic) && rec.frame_valid) {
            rec.has_planar_component = true;
            rec.planar_normal_local = {0.0, 0.0, 1.0};  // axis3 in local frame
        }

        // Source fragment miniature for comparison mode
        rec.source_fragment.populated = true;
        rec.source_fragment.center_of_mass = com;
        rec.source_fragment.center_of_geometry = frag->center_of_geometry();
        for (const auto& atom : frag->atoms) {
            SourceAtomVis sa;
            sa.position = atom.position;
            sa.atomic_number = atom.atomic_number;
            sa.radius = vdw_radius(atom.atomic_number);
            sa.flags = atom.flags;
            rec.source_fragment.atoms.push_back(sa);
        }
        for (const auto& bond : frag->bonds) {
            SourceBondVis sb;
            sb.i = static_cast<int>(bond.i);
            sb.j = static_cast<int>(bond.j);
            sb.order = bond.order;
            rec.source_fragment.bonds.push_back(sb);
        }

        // Mapping confidence from parent atom count
        if (frag->num_atoms() > 0) {
            rec.mapping_confidence = std::min(1.0, 1.0 - rec.mapping_residual / 2.0);
            if (rec.mapping_confidence < 0.0) rec.mapping_confidence = 0.0;
        }
    }

    // --- Descriptor field ---
    if (bead.has_unified_data()) {
        const auto& ud = *bead.unified;
        rec.has_descriptor = true;
        rec.descriptor_l_max = ud.max_l_max();
        rec.descriptor_residual = ud.steric.residual;

        // Sample descriptor on a grid of directions (legacy patch samples)
        constexpr int N_THETA = 12;
        constexpr int N_PHI = 24;
        constexpr double PI = 3.14159265358979323846;
        for (int it = 0; it <= N_THETA; ++it) {
            double theta = PI * it / N_THETA;
            for (int ip = 0; ip < N_PHI; ++ip) {
                double phi = 2.0 * PI * ip / N_PHI;
                SurfacePatchSample sample;
                sample.direction_local = {
                    std::sin(theta) * std::cos(phi),
                    std::sin(theta) * std::sin(phi),
                    std::cos(theta)
                };
                // Evaluate each channel
                if (ud.steric.active && !ud.steric.coeffs.empty()) {
                    sample.steric_value = evaluate_sh_expansion_at(
                        ud.steric.coeffs, ud.steric.l_max, theta, phi);
                }
                if (ud.electrostatic.active && !ud.electrostatic.coeffs.empty()) {
                    sample.electrostatic_value = evaluate_sh_expansion_at(
                        ud.electrostatic.coeffs, ud.electrostatic.l_max, theta, phi);
                }
                if (ud.dispersion.active && !ud.dispersion.coeffs.empty()) {
                    sample.dispersion_value = evaluate_sh_expansion_at(
                        ud.dispersion.coeffs, ud.dispersion.l_max, theta, phi);
                }
                rec.descriptor_field.push_back(sample);
            }
        }

        // ---- Layer 2: Populate structured surface-state grid ----
        constexpr int L2_N_THETA = 24;
        constexpr int L2_N_PHI   = 48;

        auto& grid = rec.surface_state_grid;
        grid.n_theta = L2_N_THETA;
        grid.n_phi   = L2_N_PHI;
        grid.vertices.resize((L2_N_THETA + 1) * L2_N_PHI);

        // Synthesized channel weights (equal by default)
        constexpr double w_s = 1.0 / 3.0;
        constexpr double w_e = 1.0 / 3.0;
        constexpr double w_d = 1.0 / 3.0;

        grid.steric_min = 1e30;  grid.steric_max = -1e30;
        grid.elec_min   = 1e30;  grid.elec_max   = -1e30;
        grid.disp_min   = 1e30;  grid.disp_max   = -1e30;
        grid.synth_min  = 1e30;  grid.synth_max  = -1e30;

        for (int it = 0; it <= L2_N_THETA; ++it) {
            double theta = PI * it / L2_N_THETA;
            double sin_t = std::sin(theta);
            double cos_t = std::cos(theta);

            for (int ip = 0; ip < L2_N_PHI; ++ip) {
                double phi = 2.0 * PI * ip / L2_N_PHI;
                double sin_p = std::sin(phi);
                double cos_p = std::cos(phi);

                SurfaceStateVertex& v = grid.vertices[it * L2_N_PHI + ip];
                v.theta = theta;
                v.phi   = phi;
                v.direction_local = { sin_t * cos_p, sin_t * sin_p, cos_t };

                // Evaluate each channel
                v.steric = 0.0;
                v.electrostatic = 0.0;
                v.dispersion = 0.0;

                if (ud.steric.active && !ud.steric.coeffs.empty()) {
                    v.steric = evaluate_sh_expansion_at(
                        ud.steric.coeffs, ud.steric.l_max, theta, phi);
                }
                if (ud.electrostatic.active && !ud.electrostatic.coeffs.empty()) {
                    v.electrostatic = evaluate_sh_expansion_at(
                        ud.electrostatic.coeffs, ud.electrostatic.l_max, theta, phi);
                }
                if (ud.dispersion.active && !ud.dispersion.coeffs.empty()) {
                    v.dispersion = evaluate_sh_expansion_at(
                        ud.dispersion.coeffs, ud.dispersion.l_max, theta, phi);
                }

                v.synthesized = w_s * v.steric + w_e * v.electrostatic + w_d * v.dispersion;

                // Track ranges
                if (v.steric < grid.steric_min) grid.steric_min = v.steric;
                if (v.steric > grid.steric_max) grid.steric_max = v.steric;
                if (v.electrostatic < grid.elec_min) grid.elec_min = v.electrostatic;
                if (v.electrostatic > grid.elec_max) grid.elec_max = v.electrostatic;
                if (v.dispersion < grid.disp_min) grid.disp_min = v.dispersion;
                if (v.dispersion > grid.disp_max) grid.disp_max = v.dispersion;
                if (v.synthesized < grid.synth_min) grid.synth_min = v.synthesized;
                if (v.synthesized > grid.synth_max) grid.synth_max = v.synthesized;
            }
        }
        grid.populated = true;
    }

    // --- Environment snapshot ---
    if (env) {
        rec.eta = env->eta;
        rec.rho = env->rho;
        rec.target_f = env->target_f;
        rec.P2 = env->P2;
        rec.coordination_C = env->C;
    }

    return rec;
}

} // namespace vis
} // namespace coarse_grain
