#pragma once
/**
 * seed_hash.hpp — Deterministic Seed Block S_i and Hash Provenance Block H_i
 *
 * Implements the formal definitions:
 *
 *   S_i = (seed_32, seed_64, seed_geom, seed_chem)
 *
 *   H_i = (H_32^tag, H_256^content, H_562^provenance)
 *
 * Every major database record carries both blocks to guarantee:
 *   - reproducible generation (seed)
 *   - identity and integrity verification (hash)
 *   - lineage tracing across atomistic, bead, and macro-scale projections
 *
 * Part of the Day 40C Database Architecture Demonstration Packet.
 */

#include <cstdint>
#include <string>
#include <array>
#include <functional>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace database {

// ============================================================================
// §6.2  Seed Block  S_i = (seed_32, seed_64, seed_geom, seed_chem)
// ============================================================================

struct SeedBlock {
    uint32_t    seed_32   = 0;          ///< Primary 32-bit seed
    uint64_t    seed_64   = 0;          ///< Extended 64-bit seed
    std::string seed_geom;              ///< Geometry seed label (e.g. "ring_chain_offset_v1")
    std::string seed_chem;              ///< Chemical seed label  (e.g. "hybrid_decay_response_v1")

    bool is_set() const {
        return seed_32 != 0 || seed_64 != 0
            || !seed_geom.empty() || !seed_chem.empty();
    }

    std::string summary() const {
        std::ostringstream os;
        os << "seed32=" << seed_32
           << " seed64=" << seed_64;
        if (!seed_geom.empty()) os << " geom=" << seed_geom;
        if (!seed_chem.empty()) os << " chem=" << seed_chem;
        return os.str();
    }
};

// ============================================================================
// §6.3  Hash Block  H_i = (H_32^tag, H_256^content, H_562^provenance)
// ============================================================================

/// 32-byte (256-bit) content hash — SHA-256 or equivalent
using Hash256 = std::array<uint8_t, 32>;

/// 71-byte (562-bit) provenance hash — custom deep lineage signature
/// Stored as 71 bytes (562 bits, last byte uses only 2 bits)
using Hash562 = std::array<uint8_t, 71>;

struct HashBlock {
    uint32_t tag_32    = 0;             ///< H_32^tag   — quick human-friendly tag
    Hash256  content   = {};            ///< H_256^content — exact record fingerprint
    Hash562  provenance = {};           ///< H_562^provenance — deep lineage signature

    bool has_content_hash() const {
        for (auto b : content) if (b != 0) return true;
        return false;
    }

    bool has_provenance_hash() const {
        for (auto b : provenance) if (b != 0) return true;
        return false;
    }

    /// Hex string of the content hash
    std::string content_hex() const {
        std::ostringstream os;
        for (auto b : content)
            os << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(b);
        return os.str();
    }

    /// Hex string of the provenance hash
    std::string provenance_hex() const {
        std::ostringstream os;
        for (auto b : provenance)
            os << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(b);
        return os.str();
    }

    std::string summary() const {
        std::ostringstream os;
        os << "tag=0x" << std::hex << tag_32;
        if (has_content_hash())
            os << " h256=" << content_hex().substr(0, 16) << "...";
        if (has_provenance_hash())
            os << " h562=" << provenance_hex().substr(0, 16) << "...";
        return os.str();
    }
};

// ============================================================================
// Lightweight content hash (FNV-1a 32-bit for tag, 64-bit for seed extension)
// ============================================================================

inline uint32_t fnv1a_32(const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

inline uint64_t fnv1a_64(const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x00000100000001B3ULL;
    }
    return h;
}

/// Compute a 32-bit tag from a string key
inline uint32_t compute_tag(const std::string& key) {
    return fnv1a_32(key.data(), key.size());
}

/// Fill content hash from raw bytes (truncated FNV cascade — placeholder
/// until a real SHA-256 implementation is wired in)
inline Hash256 compute_content_hash(const void* data, size_t len) {
    Hash256 h = {};
    // Fill 32 bytes with cascaded FNV-1a segments
    for (size_t seg = 0; seg < 32; seg += 8) {
        // Vary the initial state per segment
        uint64_t v = fnv1a_64(data, len) ^ (seg * 0x9E3779B97F4A7C15ULL);
        const auto* src = reinterpret_cast<const uint8_t*>(&v);
        for (size_t b = 0; b < 8 && (seg + b) < 32; ++b)
            h[seg + b] = src[b];
    }
    return h;
}

/// Fill provenance hash from raw bytes (cascade — placeholder)
inline Hash562 compute_provenance_hash(const void* data, size_t len) {
    Hash562 h = {};
    for (size_t seg = 0; seg < 71; seg += 8) {
        uint64_t v = fnv1a_64(data, len) ^ (seg * 0x517CC1B727220A95ULL);
        const auto* src = reinterpret_cast<const uint8_t*>(&v);
        for (size_t b = 0; b < 8 && (seg + b) < 71; ++b)
            h[seg + b] = src[b];
    }
    return h;
}

/// Verify a content hash matches
inline bool verify_content(const Hash256& expected,
                           const void* data, size_t len) {
    Hash256 actual = compute_content_hash(data, len);
    return actual == expected;
}

} // namespace database
} // namespace vsepr
