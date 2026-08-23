/**
 * xbit.hpp  -  XBIT (Extended Binary Identity Tag) struct and documentation
 *
 * WO-66Q  |  Non-Molecular Objects + XBIT Documentation
 * v5.1.4  |  v5.0.0-main
 *
 * XBIT is a compact, serialisable identity tag that can be attached to any
 * simulation entity — particle, object, surface, geometry, bridge, or batch
 * member.  It is designed to be:
 *
 *   - Deterministic: the same input → the same XBIT every run.
 *   - Compact: fits in 32 bytes.
 *   - Hierarchical: encodes scale tier, object kind, and lineage.
 *   - Diffable: two XBITs can be compared for equivalence, ancestry, and
 *     divergence point.
 *
 * Structure (256 bits / 32 bytes):
 *
 *   Bits 255–224  (32 bits)  tier_tag    — scale tier (atomistic, molecular,
 *                                          coarse, premacro, macro, object)
 *   Bits 223–192  (32 bits)  kind_tag    — object kind from ConstructorObjectKind
 *                                          or particle species Z
 *   Bits 191–128  (64 bits)  lineage_id  — parent / formation lineage hash
 *   Bits 127–64   (64 bits)  instance_id — deterministic instance hash
 *   Bits  63–32   (32 bits)  batch_tag   — batch index + batch base hash
 *   Bits  31–0    (32 bits)  checksum    — CRC-32 of the preceding 28 bytes
 *
 * Intended usage in .vsim:
 *   [objects]
 *   system.surface.wall = WallSurface(xbit = auto)
 *   dem.pipe_packing    = DEMBridge(from = system.surface.wall, xbit = auto)
 *
 * "xbit = auto" instructs the runtime to compute the XBIT deterministically
 * from the ObjectPath and constructor arguments.
 * "xbit = <hex32>" allows a user-supplied override (testing / replay).
 *
 * Serialisation:
 *   - Binary: 32-byte little-endian blob
 *   - Hex string: 64-char lowercase hex (no prefix, no separators)
 *   - XBIT notation: "XBIT:<hex64>"
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace vsim {

// ============================================================================
// XbitTier  —  scale-tier tag embedded in bits 255–224
// ============================================================================

enum class XbitTier : uint32_t {
	Unknown       = 0x00000000,
	Subatomic     = 0x10000000,
	Atomistic     = 0x20000000,
	Molecular     = 0x30000000,
	CoarseBead    = 0x40000000,
	Premacro      = 0x50000000,
	Macro         = 0x60000000,
	Object        = 0x70000000,  // non-molecular constructor objects
	Bridge        = 0x80000000,  // bridge objects
	Crystal       = 0x90000000,  // crystal/PBC module
	Environment   = 0xA0000000,  // ambient/environment objects
};

// ============================================================================
// Xbit  —  256-bit identity tag (32 bytes)
// ============================================================================

struct Xbit {
	// Raw storage: 32 bytes, little-endian layout
	std::array<uint8_t, 32> data = {};

	// Accessor helpers for semantic fields
	uint32_t tier_tag()     const noexcept;
	uint32_t kind_tag()     const noexcept;
	uint64_t lineage_id()   const noexcept;
	uint64_t instance_id()  const noexcept;
	uint32_t batch_tag()    const noexcept;
	uint32_t checksum()     const noexcept;

	// Mutators
	void set_tier_tag    (uint32_t v) noexcept;
	void set_kind_tag    (uint32_t v) noexcept;
	void set_lineage_id  (uint64_t v) noexcept;
	void set_instance_id (uint64_t v) noexcept;
	void set_batch_tag   (uint32_t v) noexcept;
	void recompute_checksum()          noexcept;

	// Comparison
	bool operator==(const Xbit& o) const noexcept { return data == o.data; }
	bool operator!=(const Xbit& o) const noexcept { return data != o.data; }
	bool is_null()                 const noexcept;

	// Serialisation
	std::string to_hex()    const;           // 64-char lowercase hex
	std::string to_string() const;           // "XBIT:<hex64>"

	static Xbit from_hex(const std::string& hex);  // parse "XBIT:..." or raw 64-char hex
	static Xbit null() noexcept { return {}; }
};

// ============================================================================
// Xbit field layout constants (byte offsets, little-endian)
// ============================================================================

namespace xbit_layout {
	// Field byte offsets (0 = LSB in the data array)
	inline constexpr int CHECKSUM_OFFSET    =  0;  // bytes  0– 3
	inline constexpr int BATCH_TAG_OFFSET   =  4;  // bytes  4– 7
	inline constexpr int INSTANCE_ID_OFFSET =  8;  // bytes  8–15
	inline constexpr int LINEAGE_ID_OFFSET  = 16;  // bytes 16–23
	inline constexpr int KIND_TAG_OFFSET    = 24;  // bytes 24–27
	inline constexpr int TIER_TAG_OFFSET    = 28;  // bytes 28–31
}

// ============================================================================
// XbitAccessor inlines
// ============================================================================

namespace xbit_detail {
	inline uint32_t load32(const uint8_t* p) noexcept {
		return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
			   ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
	}
	inline uint64_t load64(const uint8_t* p) noexcept {
		return (uint64_t)load32(p) | ((uint64_t)load32(p+4)<<32);
	}
	inline void store32(uint8_t* p, uint32_t v) noexcept {
		p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
		p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
	}
	inline void store64(uint8_t* p, uint64_t v) noexcept {
		store32(p,(uint32_t)v); store32(p+4,(uint32_t)(v>>32));
	}
}

inline uint32_t Xbit::tier_tag()    const noexcept { return xbit_detail::load32(data.data() + xbit_layout::TIER_TAG_OFFSET); }
inline uint32_t Xbit::kind_tag()    const noexcept { return xbit_detail::load32(data.data() + xbit_layout::KIND_TAG_OFFSET); }
inline uint64_t Xbit::lineage_id()  const noexcept { return xbit_detail::load64(data.data() + xbit_layout::LINEAGE_ID_OFFSET); }
inline uint64_t Xbit::instance_id() const noexcept { return xbit_detail::load64(data.data() + xbit_layout::INSTANCE_ID_OFFSET); }
inline uint32_t Xbit::batch_tag()   const noexcept { return xbit_detail::load32(data.data() + xbit_layout::BATCH_TAG_OFFSET); }
inline uint32_t Xbit::checksum()    const noexcept { return xbit_detail::load32(data.data() + xbit_layout::CHECKSUM_OFFSET); }
inline bool     Xbit::is_null()     const noexcept {
	for (auto b : data) {
		if (b) return false;
	}
	return true;
}

inline void Xbit::set_tier_tag   (uint32_t v) noexcept { xbit_detail::store32(data.data() + xbit_layout::TIER_TAG_OFFSET, v); }
inline void Xbit::set_kind_tag   (uint32_t v) noexcept { xbit_detail::store32(data.data() + xbit_layout::KIND_TAG_OFFSET, v); }
inline void Xbit::set_lineage_id (uint64_t v) noexcept { xbit_detail::store64(data.data() + xbit_layout::LINEAGE_ID_OFFSET, v); }
inline void Xbit::set_instance_id(uint64_t v) noexcept { xbit_detail::store64(data.data() + xbit_layout::INSTANCE_ID_OFFSET, v); }
inline void Xbit::set_batch_tag  (uint32_t v) noexcept { xbit_detail::store32(data.data() + xbit_layout::BATCH_TAG_OFFSET, v); }

} // namespace vsim
