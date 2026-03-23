#pragma once
/**
 * multi_channel_descriptor.hpp — Multi-Channel Anisotropic Surface Descriptor
 *
 * NOTE: <algorithm> is required for std::max(initializer_list) on all compilers.
 *
 * Extends the single-channel SurfaceDescriptor to a vector-valued
 * representation with independent spherical harmonic expansions
 * for distinct physical interaction channels:
 *
 *   S_B(θ,φ) → { S^(steric)(θ,φ), S^(elec)(θ,φ), S^(disp)(θ,φ) }
 *
 * Each channel has its own:
 *   - Runtime-configurable ℓ_max (higher order for complex structures)
 *   - Independent SH coefficient vector
 *   - Per-channel metrics (power, anisotropy, band spectrum)
 *
 * Anti-black-box: all channels, coefficients, and metrics are
 * independently inspectable and traceable to their probe source.
 *
 * Reference: "Descriptor Enrichment for Complex Anisotropic Structures"
 *            subsection of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {

/**
 * DescriptorChannel — identifies the physical interaction type.
 *
 * Each channel captures a distinct contribution:
 *   STERIC:        geometric accessibility and excluded volume
 *   ELECTROSTATIC: electrostatic potential or charge distribution
 *   DISPERSION:    dispersion and non-bonded interaction strength
 */
enum class DescriptorChannel {
    STERIC        = 0,
    ELECTROSTATIC = 1,
    DISPERSION    = 2,
    NUM_CHANNELS  = 3
};

/**
 * Return human-readable name for a descriptor channel.
 */
inline const char* channel_name(DescriptorChannel ch) {
    switch (ch) {
        case DescriptorChannel::STERIC:        return "steric";
        case DescriptorChannel::ELECTROSTATIC: return "electrostatic";
        case DescriptorChannel::DISPERSION:    return "dispersion";
        default:                               return "unknown";
    }
}

/**
 * ChannelDescriptor — one channel of the multi-channel surface.
 *
 * Stores SH coefficients for a single physical channel at a
 * runtime-configurable angular resolution (ℓ_max).
 */
struct ChannelDescriptor {
    /// Which physical channel this represents
    DescriptorChannel channel{DescriptorChannel::STERIC};

    /// Maximum spherical harmonic order for this channel
    int l_max{SH_L_MAX};

    /// SH expansion coefficients c_{ℓm}, length = (l_max+1)²
    std::vector<double> coeffs;

    // ====================================================================
    // Construction helpers
    // ====================================================================

    /**
     * Initialize with a given ℓ_max, zeroing all coefficients.
     */
    void init(DescriptorChannel ch, int lmax) {
        channel = ch;
        l_max = lmax;
        coeffs.assign(sh_num_coeffs(lmax), 0.0);
    }

    // ====================================================================
    // Evaluation
    // ====================================================================

    /**
     * Evaluate this channel's surface value at direction (θ, φ).
     */
    double evaluate(double theta, double phi) const {
        return evaluate_sh_expansion_dynamic(coeffs, theta, phi, l_max);
    }

    // ====================================================================
    // Metrics
    // ====================================================================

    /** Total L2 power. */
    double total_power() const { return sh_power_dynamic(coeffs); }

    /** Power per ℓ band. */
    std::vector<double> band_power() const {
        return sh_band_power_dynamic(coeffs, l_max);
    }

    /** Anisotropy ratio (0 = isotropic, ~1 = strongly anisotropic). */
    double anisotropy_ratio() const {
        return sh_anisotropy_ratio_dynamic(coeffs);
    }

    /** Number of SH coefficients. */
    int num_coeffs() const { return sh_num_coeffs(l_max); }

    /** Isotropic component: c_{0,0} / √(4π). */
    double isotropic_component() const {
        constexpr double inv_sqrt_4pi = 1.0 / std::sqrt(4.0 * 3.14159265358979323846);
        return coeffs.empty() ? 0.0 : coeffs[0] * inv_sqrt_4pi;
    }

    /**
     * Dominant ℓ band (highest power, excluding ℓ=0). Returns -1 if zero.
     */
    int dominant_band() const {
        auto bp = band_power();
        int best_l = -1;
        double best_p = 0.0;
        for (int l = 1; l <= l_max; ++l) {
            if (bp[l] > best_p) {
                best_p = bp[l];
                best_l = l;
            }
        }
        return best_l;
    }
};

/**
 * MultiChannelDescriptor — vector-valued anisotropic surface representation.
 *
 * Contains one ChannelDescriptor per physical channel (steric, electrostatic,
 * dispersion), each with independently configurable ℓ_max.
 *
 * The full orthonormal frame Q_B = {ê₁, ê₂, ê₃} from the inertia tensor
 * defines the coordinate system in which all channels are expressed.
 */
struct MultiChannelDescriptor {
    /// Per-channel descriptors
    ChannelDescriptor steric;
    ChannelDescriptor electrostatic;
    ChannelDescriptor dispersion;

    /// Probe radius used during sampling (Å)
    double probe_radius{};

    /// Number of sample points used
    int n_samples{};

    /// Local frame in which all channels are defined (full 3D orientation)
    InertiaFrame frame{};

    // ====================================================================
    // Construction
    // ====================================================================

    /**
     * Initialize all channels with specified ℓ_max values.
     */
    void init(int l_max_steric, int l_max_elec, int l_max_disp) {
        steric.init(DescriptorChannel::STERIC, l_max_steric);
        electrostatic.init(DescriptorChannel::ELECTROSTATIC, l_max_elec);
        dispersion.init(DescriptorChannel::DISPERSION, l_max_disp);
    }

    /**
     * Initialize all channels with the same ℓ_max.
     */
    void init(int l_max) {
        init(l_max, l_max, l_max);
    }

    // ====================================================================
    // Channel access
    // ====================================================================

    /** Access channel by enum. */
    ChannelDescriptor& channel(DescriptorChannel ch) {
        switch (ch) {
            case DescriptorChannel::STERIC:        return steric;
            case DescriptorChannel::ELECTROSTATIC: return electrostatic;
            case DescriptorChannel::DISPERSION:    return dispersion;
            default:                               return steric;
        }
    }

    const ChannelDescriptor& channel(DescriptorChannel ch) const {
        switch (ch) {
            case DescriptorChannel::STERIC:        return steric;
            case DescriptorChannel::ELECTROSTATIC: return electrostatic;
            case DescriptorChannel::DISPERSION:    return dispersion;
            default:                               return steric;
        }
    }

    /** Number of active channels. */
    static constexpr int num_channels() {
        return static_cast<int>(DescriptorChannel::NUM_CHANNELS);
    }

    // ====================================================================
    // Combined metrics
    // ====================================================================

    /**
     * Combined anisotropy: maximum anisotropy ratio across all channels.
     */
    double max_anisotropy() const {
        return std::max({steric.anisotropy_ratio(),
                         electrostatic.anisotropy_ratio(),
                         dispersion.anisotropy_ratio()});
    }

    /**
     * Total power summed across all channels.
     */
    double total_power() const {
        return steric.total_power() + electrostatic.total_power() + dispersion.total_power();
    }

    /**
     * Asphericity from the inertia frame (convenience).
     */
    double asphericity() const {
        return frame.asphericity;
    }

    /**
     * Suggest an appropriate ℓ_max based on the observed anisotropy
     * of a single-channel descriptor. Adaptive complexity selection.
     *
     * Returns a recommended ℓ_max:
     *   ratio < 0.1 → 2 (nearly isotropic)
     *   ratio < 0.3 → 4 (moderate anisotropy)
     *   ratio < 0.6 → 6 (strong anisotropy)
     *   ratio ≥ 0.6 → 8 (complex anisotropy)
     */
    static int suggest_l_max(double anisotropy_ratio) {
        if (anisotropy_ratio < 0.1) return 2;
        if (anisotropy_ratio < 0.3) return 4;
        if (anisotropy_ratio < 0.6) return 6;
        return 8;
    }
};

} // namespace coarse_grain
