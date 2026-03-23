#pragma once
/**
 * unified_descriptor.hpp — Unified Adaptive Descriptor
 *
 * Replaces the discrete two-tier model architecture with a single
 * descriptor formalism that supports adaptive truncation. All beads,
 * regardless of structural complexity, use the same data model:
 *
 *   S_B^(k)(θ,φ) = Σ_{ℓ,m} c_{ℓm}^(k) Y_{ℓm}(θ,φ)
 *
 * Model complexity is adjusted by varying:
 *   - ℓ_max (angular resolution)
 *   - number of active channels (steric, electrostatic, dispersion)
 *   - coefficient density
 *
 * The "reduced" and "enriched" regimes are truncation levels of one
 * continuous model family, NOT separate data models.
 *
 * Promotion is driven by descriptor residual rather than hard
 * if-statement thresholds (atom count, metal center, etc.).
 *
 * Universal energy form:
 *   U = F(r, Q_A, Q_B, {c_{ℓm}^(k)})
 *
 * Anti-black-box: all coefficients, channel activations, residual
 * metrics, and resolution decisions are explicitly inspectable.
 *
 * Reference: "Unified Descriptor Strategy" section of
 *            section_anisotropic_beads.tex
 */

#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Resolution Level
// ============================================================================

/**
 * ResolutionLevel — descriptive label for the active truncation depth.
 *
 * These are NOT discrete model tiers. They are human-readable labels
 * for ranges within a continuous resolution parameter (ℓ_max).
 *
 *   ISOTROPIC: ℓ_max = 0, single c_{00} coefficient
 *   AXIAL:     ℓ_max ≤ 2, captures dominant planar/axial anisotropy
 *   MODERATE:  ℓ_max ≤ 4, sufficient for rigid aromatic systems
 *   ENRICHED:  ℓ_max > 4, required for sterically complex systems
 */
enum class ResolutionLevel {
    ISOTROPIC = 0,
    AXIAL     = 1,
    MODERATE  = 2,
    ENRICHED  = 3
};

inline const char* resolution_level_name(ResolutionLevel level) {
    switch (level) {
        case ResolutionLevel::ISOTROPIC: return "Isotropic (l_max=0)";
        case ResolutionLevel::AXIAL:     return "Axial (l_max<=2)";
        case ResolutionLevel::MODERATE:  return "Moderate (l_max<=4)";
        case ResolutionLevel::ENRICHED:  return "Enriched (l_max>4)";
        default:                         return "unknown";
    }
}

/**
 * Classify an ℓ_max value into a resolution level.
 */
inline ResolutionLevel classify_resolution(int l_max) {
    if (l_max <= 0) return ResolutionLevel::ISOTROPIC;
    if (l_max <= 2) return ResolutionLevel::AXIAL;
    if (l_max <= 4) return ResolutionLevel::MODERATE;
    return ResolutionLevel::ENRICHED;
}

// ============================================================================
// Unified Channel
// ============================================================================

/**
 * UnifiedChannel — one channel within the unified descriptor.
 *
 * Same structure as ChannelDescriptor but with additional metadata
 * for residual-driven promotion and active/inactive tracking.
 */
struct UnifiedChannel {
    /// Which physical channel this represents
    DescriptorChannel channel{DescriptorChannel::STERIC};

    /// Maximum spherical harmonic order for this channel
    int l_max{0};

    /// SH expansion coefficients c_{ℓm}, length = (l_max+1)²
    std::vector<double> coeffs;

    /// Whether this channel is active (has been fitted/populated)
    bool active{false};

    /// Reconstruction residual from the last fit (0 = perfect, 1 = total failure)
    double residual{1.0};

    // ====================================================================
    // Construction
    // ====================================================================

    /**
     * Initialize with a given ℓ_max, zeroing all coefficients.
     */
    void init(DescriptorChannel ch, int lmax) {
        channel = ch;
        l_max = lmax;
        coeffs.assign(sh_num_coeffs(lmax), 0.0);
        active = true;
        residual = 1.0;
    }

    /**
     * Promote to a higher ℓ_max, preserving existing coefficients.
     * New coefficients are initialized to zero.
     */
    void promote(int new_l_max) {
        if (new_l_max <= l_max) return;
        int old_n = static_cast<int>(coeffs.size());
        int new_n = sh_num_coeffs(new_l_max);
        coeffs.resize(new_n, 0.0);
        l_max = new_l_max;
    }

    /**
     * Truncate to a lower ℓ_max, discarding higher-order coefficients.
     */
    void truncate(int new_l_max) {
        if (new_l_max >= l_max) return;
        int new_n = sh_num_coeffs(new_l_max);
        coeffs.resize(new_n);
        l_max = new_l_max;
    }

    // ====================================================================
    // Evaluation
    // ====================================================================

    /** Evaluate this channel's surface value at direction (θ, φ). */
    double evaluate(double theta, double phi) const {
        if (!active || coeffs.empty()) return 0.0;
        return evaluate_sh_expansion_dynamic(coeffs, theta, phi, l_max);
    }

    // ====================================================================
    // Metrics
    // ====================================================================

    /** Total L2 power. */
    double total_power() const {
        if (!active) return 0.0;
        return sh_power_dynamic(coeffs);
    }

    /** Power per ℓ band. */
    std::vector<double> band_power() const {
        return sh_band_power_dynamic(coeffs, l_max);
    }

    /** Anisotropy ratio (0 = isotropic, ~1 = strongly anisotropic). */
    double anisotropy_ratio() const {
        if (!active) return 0.0;
        return sh_anisotropy_ratio_dynamic(coeffs);
    }

    /** Number of SH coefficients. */
    int num_coeffs() const { return sh_num_coeffs(l_max); }

    /** Number of nonzero coefficients. */
    int num_nonzero() const {
        int count = 0;
        for (double c : coeffs) {
            if (std::abs(c) > 1e-15) ++count;
        }
        return count;
    }

    /** Resolution level of this channel. */
    ResolutionLevel resolution() const {
        if (!active) return ResolutionLevel::ISOTROPIC;
        return classify_resolution(l_max);
    }
};

// ============================================================================
// Unified Descriptor
// ============================================================================

/**
 * UnifiedDescriptor — single adaptive descriptor for all CG beads.
 *
 * Contains one UnifiedChannel per physical channel. All channels share
 * the same orientation frame. Channel activation and ℓ_max are
 * independently configurable — this is NOT a two-model architecture.
 *
 * The "resolution" of a bead is determined by the active channels and
 * their ℓ_max values, not by a discrete tier assignment.
 */
struct UnifiedDescriptor {
    /// Per-channel descriptors
    UnifiedChannel steric;
    UnifiedChannel electrostatic;
    UnifiedChannel dispersion;

    /// Local frame in which all channels are defined
    InertiaFrame frame{};

    /// Probe radius used during sampling (Å)
    double probe_radius{};

    /// Number of sample points used
    int n_samples{};

    // ====================================================================
    // Construction
    // ====================================================================

    /**
     * Initialize all channels with the same ℓ_max (uniform resolution).
     * All channels start active.
     */
    void init(int l_max) {
        steric.init(DescriptorChannel::STERIC, l_max);
        electrostatic.init(DescriptorChannel::ELECTROSTATIC, l_max);
        dispersion.init(DescriptorChannel::DISPERSION, l_max);
    }

    /**
     * Initialize with per-channel ℓ_max values.
     */
    void init(int l_max_steric, int l_max_elec, int l_max_disp) {
        steric.init(DescriptorChannel::STERIC, l_max_steric);
        electrostatic.init(DescriptorChannel::ELECTROSTATIC, l_max_elec);
        dispersion.init(DescriptorChannel::DISPERSION, l_max_disp);
    }

    /**
     * Initialize with a single active channel at given ℓ_max.
     * Other channels remain inactive.
     */
    void init_single_channel(DescriptorChannel ch, int l_max) {
        steric.active = false;
        electrostatic.active = false;
        dispersion.active = false;
        channel(ch).init(ch, l_max);
    }

    // ====================================================================
    // Channel access
    // ====================================================================

    /** Access channel by enum (mutable). */
    UnifiedChannel& channel(DescriptorChannel ch) {
        switch (ch) {
            case DescriptorChannel::STERIC:        return steric;
            case DescriptorChannel::ELECTROSTATIC: return electrostatic;
            case DescriptorChannel::DISPERSION:    return dispersion;
            default:                               return steric;
        }
    }

    /** Access channel by enum (const). */
    const UnifiedChannel& channel(DescriptorChannel ch) const {
        switch (ch) {
            case DescriptorChannel::STERIC:        return steric;
            case DescriptorChannel::ELECTROSTATIC: return electrostatic;
            case DescriptorChannel::DISPERSION:    return dispersion;
            default:                               return steric;
        }
    }

    /** Number of active channels. */
    int num_active_channels() const {
        int n = 0;
        if (steric.active)        ++n;
        if (electrostatic.active) ++n;
        if (dispersion.active)    ++n;
        return n;
    }

    /** Maximum ℓ_max across all active channels. */
    int max_l_max() const {
        int m = 0;
        if (steric.active)        m = std::max(m, steric.l_max);
        if (electrostatic.active) m = std::max(m, electrostatic.l_max);
        if (dispersion.active)    m = std::max(m, dispersion.l_max);
        return m;
    }

    // ====================================================================
    // Resolution
    // ====================================================================

    /** Overall resolution level (maximum across active channels). */
    ResolutionLevel resolution_level() const {
        return classify_resolution(max_l_max());
    }

    /** Total number of active coefficients (across all channels). */
    int total_active_coefficients() const {
        int n = 0;
        if (steric.active)        n += static_cast<int>(steric.coeffs.size());
        if (electrostatic.active) n += static_cast<int>(electrostatic.coeffs.size());
        if (dispersion.active)    n += static_cast<int>(dispersion.coeffs.size());
        return n;
    }

    /** Total number of nonzero coefficients. */
    int total_nonzero_coefficients() const {
        int n = 0;
        if (steric.active)        n += steric.num_nonzero();
        if (electrostatic.active) n += electrostatic.num_nonzero();
        if (dispersion.active)    n += dispersion.num_nonzero();
        return n;
    }

    // ====================================================================
    // Promotion (adaptive enrichment)
    // ====================================================================

    /**
     * Promote a specific channel to a higher ℓ_max.
     * Preserves existing coefficients; new ones start at zero.
     */
    void promote_channel(DescriptorChannel ch, int new_l_max) {
        auto& c = channel(ch);
        if (!c.active) {
            c.init(ch, new_l_max);
        } else {
            c.promote(new_l_max);
        }
    }

    /**
     * Promote all active channels to a new ℓ_max.
     */
    void promote_all(int new_l_max) {
        if (steric.active)        steric.promote(new_l_max);
        if (electrostatic.active) electrostatic.promote(new_l_max);
        if (dispersion.active)    dispersion.promote(new_l_max);
    }

    /**
     * Activate a channel that was previously inactive.
     */
    void activate_channel(DescriptorChannel ch, int l_max) {
        auto& c = channel(ch);
        if (!c.active) {
            c.init(ch, l_max);
        }
    }

    // ====================================================================
    // Metrics
    // ====================================================================

    /**
     * Maximum anisotropy ratio across all active channels.
     */
    double max_anisotropy() const {
        double m = 0.0;
        if (steric.active)        m = std::max(m, steric.anisotropy_ratio());
        if (electrostatic.active) m = std::max(m, electrostatic.anisotropy_ratio());
        if (dispersion.active)    m = std::max(m, dispersion.anisotropy_ratio());
        return m;
    }

    /**
     * Maximum residual across all active channels.
     * This is the key metric for deciding whether promotion is needed.
     */
    double max_residual() const {
        double m = 0.0;
        if (steric.active)        m = std::max(m, steric.residual);
        if (electrostatic.active) m = std::max(m, electrostatic.residual);
        if (dispersion.active)    m = std::max(m, dispersion.residual);
        return m;
    }

    /**
     * Total power summed across all active channels.
     */
    double total_power() const {
        double p = 0.0;
        if (steric.active)        p += steric.total_power();
        if (electrostatic.active) p += electrostatic.total_power();
        if (dispersion.active)    p += dispersion.total_power();
        return p;
    }

    /**
     * Asphericity from the inertia frame (convenience).
     */
    double asphericity() const {
        return frame.asphericity;
    }

    // ====================================================================
    // Conversion from legacy descriptors
    // ====================================================================

    /**
     * Construct a UnifiedDescriptor from a MultiChannelDescriptor.
     * Preserves all coefficients and frame data.
     */
    static UnifiedDescriptor from_multi_channel(const MultiChannelDescriptor& mcd) {
        UnifiedDescriptor ud;
        ud.frame = mcd.frame;
        ud.probe_radius = mcd.probe_radius;
        ud.n_samples = mcd.n_samples;

        // Copy steric channel
        ud.steric.channel = DescriptorChannel::STERIC;
        ud.steric.l_max = mcd.steric.l_max;
        ud.steric.coeffs = mcd.steric.coeffs;
        ud.steric.active = !mcd.steric.coeffs.empty();
        ud.steric.residual = 0.0;

        // Copy electrostatic channel
        ud.electrostatic.channel = DescriptorChannel::ELECTROSTATIC;
        ud.electrostatic.l_max = mcd.electrostatic.l_max;
        ud.electrostatic.coeffs = mcd.electrostatic.coeffs;
        ud.electrostatic.active = !mcd.electrostatic.coeffs.empty();
        ud.electrostatic.residual = 0.0;

        // Copy dispersion channel
        ud.dispersion.channel = DescriptorChannel::DISPERSION;
        ud.dispersion.l_max = mcd.dispersion.l_max;
        ud.dispersion.coeffs = mcd.dispersion.coeffs;
        ud.dispersion.active = !mcd.dispersion.coeffs.empty();
        ud.dispersion.residual = 0.0;

        return ud;
    }

    /**
     * Construct a UnifiedDescriptor from a single-channel SurfaceDescriptor.
     * Maps the fixed-size SH expansion into the steric channel.
     */
    static UnifiedDescriptor from_surface_descriptor(const SurfaceDescriptor& sd) {
        UnifiedDescriptor ud;
        ud.frame = sd.frame;
        ud.probe_radius = sd.probe_radius;
        ud.n_samples = sd.n_samples;

        ud.steric.channel = DescriptorChannel::STERIC;
        ud.steric.l_max = SH_L_MAX;
        ud.steric.coeffs.assign(sd.coeffs.begin(), sd.coeffs.end());
        ud.steric.active = true;
        ud.steric.residual = 0.0;

        // Other channels inactive
        ud.electrostatic.active = false;
        ud.dispersion.active = false;

        return ud;
    }
};

} // namespace coarse_grain
