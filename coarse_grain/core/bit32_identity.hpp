#pragma once
/**
 * bit32_identity.hpp — 32-Bit Packed Particle Identity Word
 *
 * Encodes the full §0 Identity–State Decomposition vector
 *
 *   I_i = [ Z_i, A_i, Q_i, Σ_i, Λ_i, Θ_i ]^T
 *
 * into a single 32-bit unsigned integer:
 *
 *   Bit layout (MSB → LSB):
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │ 31 ─────── 24 │ 23 ────── 16 │ 15 ──────── 8 │ 7 6 5 │ 4 3 │ 2 1 0 │
 *   │   Z_i [8]     │   A_i [8]    │   Q_i [8]     │ Σ[3]  │ Λ[2]│ Θ[3]  │
 *   └─────────────────────────────────────────────────────────────────┘
 *
 *   Field      Bits   Range / encoding
 *   ────────── ────── ──────────────────────────────────────────────────
 *   Z_i         8     Atomic number  0–118  (0 = unassigned)
 *   A_i         8     Mass bucket    0–255  (0 = natural abundance avg)
 *   Q_i         8     Charge word    signed 8-bit fixed-point × 0.25 e
 *                     range –32.0 e … +31.75 e  (sufficient for ions)
 *   Σ_i         3     StructuralRole 0–4    (5 values; 3 bits ≥ ceiling)
 *   Λ_i         2     StabilityClass 0–3    (4 values; 2 bits exact)
 *   Θ_i         3     Provenance tag 0–7    (8 provenance buckets)
 *   ────────── ────── ──────────────────────────────────────────────────
 *   Total      32 bits  ← one uint32_t, cache-line friendly
 *
 * Design notes:
 *   - The word is immutable at CG time: pack once, read many times.
 *   - Q_i uses s8.2 fixed-point so fractional charges (e.g. +1.5 e)
 *     round-trip without floating-point in the hot path.
 *   - Θ_i stores a 3-bit provenance tag, not the full hash (which
 *     lives in Bead::parent_atom_indices).  Tag values:
 *       0 = virgin/untracked
 *       1 = mapped from atomistic
 *       2 = relaxed (FIRE converged)
 *       3 = CG-only (no atomistic parent)
 *       4 = reaction product
 *       5 = crystal seed
 *       6 = foreign import
 *       7 = mutated/hypothesis
 *
 * Anti-black-box: every bit field has a named accessor.
 * Deterministic:  pack(unpack(w)) == w for all valid inputs.
 *
 * Reference: docs/section_32bit_hourglass_lookglass.tex §1
 *            docs/section0_identity_state_decomposition.tex §0.1
 */

#include "coarse_grain/core/bead.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace coarse_grain {

// ============================================================================
// Provenance Tag (Θ_i 3-bit encoding)
// ============================================================================

enum class ProvenanceTag : uint8_t {
    Virgin          = 0,  // Untracked origin
    MappedAtomistic = 1,  // Mapped from atomistic parent group
    FIRERelaxed     = 2,  // FIRE-converged minimum
    CGOnly          = 3,  // No atomistic parent
    ReactionProduct = 4,  // Produced by reaction engine
    CrystalSeed     = 5,  // Seeded from crystal lattice
    ForeignImport   = 6,  // Imported from external data
    Hypothesis      = 7   // Mutated / hypothetical structure
};

inline const char* provenance_tag_name(ProvenanceTag t) {
    switch (t) {
        case ProvenanceTag::Virgin:          return "Virgin";
        case ProvenanceTag::MappedAtomistic: return "Mapped-Atomistic";
        case ProvenanceTag::FIRERelaxed:     return "FIRE-Relaxed";
        case ProvenanceTag::CGOnly:          return "CG-Only";
        case ProvenanceTag::ReactionProduct: return "Reaction-Product";
        case ProvenanceTag::CrystalSeed:     return "Crystal-Seed";
        case ProvenanceTag::ForeignImport:   return "Foreign-Import";
        case ProvenanceTag::Hypothesis:      return "Hypothesis";
        default:                             return "Unknown";
    }
}

// ============================================================================
// Bit-field masks and shifts
// ============================================================================

namespace detail {
    static constexpr uint32_t SHIFT_Z   = 24u;
    static constexpr uint32_t SHIFT_A   = 16u;
    static constexpr uint32_t SHIFT_Q   = 8u;
    static constexpr uint32_t SHIFT_SIG = 5u;
    static constexpr uint32_t SHIFT_LAM = 3u;
    static constexpr uint32_t SHIFT_TH  = 0u;

    static constexpr uint32_t MASK_Z    = 0xFF000000u;
    static constexpr uint32_t MASK_A    = 0x00FF0000u;
    static constexpr uint32_t MASK_Q    = 0x0000FF00u;
    static constexpr uint32_t MASK_SIG  = 0x000000E0u;  // bits 7:5
    static constexpr uint32_t MASK_LAM  = 0x00000018u;  // bits 4:3
    static constexpr uint32_t MASK_TH   = 0x00000007u;  // bits 2:0
} // namespace detail

// ============================================================================
// Identity32 — the packed word + typed accessors
// ============================================================================

/**
 * Identity32 — 32-bit packed particle identity word.
 *
 * Immutable after construction. All six §0 identity fields are
 * recoverable via named accessors without any external state.
 */
struct Identity32 {
    uint32_t word{0u};

    // ── default ──────────────────────────────────────────────────────────────
    Identity32() = default;
    explicit Identity32(uint32_t raw) : word(raw) {}

    // ── field accessors ───────────────────────────────────────────────────────

    /// Z_i — atomic number (0–118).
    uint8_t Z() const {
        return static_cast<uint8_t>((word & detail::MASK_Z) >> detail::SHIFT_Z);
    }

    /// A_i — mass bucket (0 = natural average; 1–255 = specific mass).
    uint8_t A() const {
        return static_cast<uint8_t>((word & detail::MASK_A) >> detail::SHIFT_A);
    }

    /**
     * Q_i — charge participation as s8.2 fixed-point.
     *
     * Returns the decoded floating-point charge in units of e.
     * Resolution: 0.25 e per LSB.  Range: –32.0 … +31.75 e.
     */
    double Q() const {
        auto raw = static_cast<int8_t>(
            static_cast<uint8_t>((word & detail::MASK_Q) >> detail::SHIFT_Q));
        return static_cast<double>(raw) * 0.25;
    }

    /// Q_i raw signed byte (for serialisation without FP).
    int8_t Q_raw() const {
        return static_cast<int8_t>(
            static_cast<uint8_t>((word & detail::MASK_Q) >> detail::SHIFT_Q));
    }

    /// Σ_i — structural role (3-bit, values 0–4).
    StructuralRole sigma() const {
        auto v = static_cast<uint8_t>((word & detail::MASK_SIG) >> detail::SHIFT_SIG);
        return static_cast<StructuralRole>(v <= 4u ? v : 4u);
    }

    /// Λ_i — stability class (2-bit, values 0–3).
    StabilityClass lambda() const {
        auto v = static_cast<uint8_t>((word & detail::MASK_LAM) >> detail::SHIFT_LAM);
        return static_cast<StabilityClass>(v);
    }

    /// Θ_i — provenance tag (3-bit, values 0–7).
    ProvenanceTag theta() const {
        auto v = static_cast<uint8_t>((word & detail::MASK_TH) >> detail::SHIFT_TH);
        return static_cast<ProvenanceTag>(v);
    }

    // ── comparison / equality ─────────────────────────────────────────────────
    bool operator==(const Identity32& o) const { return word == o.word; }
    bool operator!=(const Identity32& o) const { return word != o.word; }

    // ── human-readable dump ───────────────────────────────────────────────────
    std::string to_string() const {
        return std::string("Identity32{"
            " Z=")  + std::to_string(Z())
            + " A=" + std::to_string(A())
            + " Q=" + std::to_string(Q())
            + " Σ=" + structural_role_name(sigma())
            + " Λ=" + stability_class_name(lambda())
            + " Θ=" + provenance_tag_name(theta())
            + " }";
    }
};

// ============================================================================
// Pack / unpack functions
// ============================================================================

/**
 * pack_identity — encode all six §0 fields into a 32-bit word.
 *
 * @param Z      Atomic number (0–118).  Clamped to 8 bits.
 * @param A      Mass bucket (0–255).    Clamped to 8 bits.
 * @param Q_fp   Charge in units of e.  Encoded as s8.2 fixed-point.
 *               Out-of-range values are clamped to [–32.0, +31.75].
 * @param sigma  StructuralRole Σ_i (0–4).
 * @param lambda StabilityClass Λ_i (0–3).
 * @param theta  ProvenanceTag  Θ_i (0–7).
 */
inline Identity32 pack_identity(
    uint8_t        Z,
    uint8_t        A,
    double         Q_fp,
    StructuralRole sigma,
    StabilityClass lambda,
    ProvenanceTag  theta)
{
    // Encode Q as s8.2 fixed-point, clamped
    double q_clamped = Q_fp < -32.0 ? -32.0 : (Q_fp > 31.75 ? 31.75 : Q_fp);
    auto   q_raw     = static_cast<int8_t>(q_clamped / 0.25 + (q_clamped >= 0 ? 0.5 : -0.5));

    uint32_t sig_v = static_cast<uint8_t>(sigma) & 0x07u;
    uint32_t lam_v = static_cast<uint8_t>(lambda) & 0x03u;
    uint32_t th_v  = static_cast<uint8_t>(theta)  & 0x07u;

    uint32_t w =
        (static_cast<uint32_t>(Z)                     << detail::SHIFT_Z)
      | (static_cast<uint32_t>(A)                     << detail::SHIFT_A)
      | (static_cast<uint32_t>(static_cast<uint8_t>(q_raw)) << detail::SHIFT_Q)
      | (sig_v                                         << detail::SHIFT_SIG)
      | (lam_v                                         << detail::SHIFT_LAM)
      | (th_v                                          << detail::SHIFT_TH);

    return Identity32{w};
}

/**
 * pack_from_bead — convenience overload that reads fields directly from
 * a Bead and its Z-value.
 *
 * @param bead  The Bead to pack (carries Σ_i, Λ_i, charge).
 * @param Z     Atomic number of the dominant atom (or representative Z).
 * @param A     Mass bucket (pass 0 for natural average).
 * @param theta Provenance tag for this packing context.
 */
inline Identity32 pack_from_bead(
    const Bead&    bead,
    uint8_t        Z,
    uint8_t        A          = 0,
    ProvenanceTag  theta      = ProvenanceTag::MappedAtomistic)
{
    return pack_identity(Z, A, bead.charge,
                         bead.structural_role,
                         bead.stability_class,
                         theta);
}

// ============================================================================
// Round-trip assertion (debug utility)
// ============================================================================

/**
 * identity32_roundtrip_ok — verify that pack(unpack(w)) == w.
 *
 * Returns true if the word is self-consistent.  Only the 3 reserved
 * values of Σ (5–7) will fail — all valid inputs pass.
 */
inline bool identity32_roundtrip_ok(const Identity32& id) {
    Identity32 repacked = pack_identity(
        id.Z(), id.A(), id.Q(),
        id.sigma(), id.lambda(), id.theta());
    return repacked == id;
}

} // namespace coarse_grain
