#pragma once
/**
 * organic_class.hpp  —  WO-75C: Organic Class Descriptors
 * =========================================================
 *
 * Defines the organic class layer that biological objects resolve into.
 * Each class carries an explicit bonding-character tag.
 * No geometry is implied at this layer.
 *
 * Resolution chain:
 *   [bio/nongeom object]  →  C⃗_plant  →  [OrganicClass]_{organic}
 *
 * Classes:
 *   Cellulose  organic/polymer      σ_net · H-bond network
 *   Lignin     organic/ar/polymer   σ_net · conjugated π̃  (aromatic)
 *   Pectin     organic/gel          σ · H · ±charged sites
 *
 * VSEPR-SIM  |  WO-75C  |  v5.13.5
 */

#include <cstdint>
#include <string_view>

namespace vsepr {
namespace bio {

// ============================================================================
// BondingCharacter — bitmask flags (may be combined with |)
// ============================================================================

enum class BondingCharacter : uint8_t {
	None        = 0,
	SigmaNet    = 1 << 0,   // σ_net — extended covalent σ framework
	HBond       = 1 << 1,   // H-bond network (hydroxyl / amide donors)
	ArPi        = 1 << 2,   // aromatic conjugated π̃ (lignin phenyl rings)
	ChargedSite = 1 << 3,   // ±charged ionisable sites (pectin carboxylate)
};

inline BondingCharacter operator|(BondingCharacter a, BondingCharacter b) {
	return static_cast<BondingCharacter>(
		static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool has_flag(BondingCharacter flags, BondingCharacter bit) {
	return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(bit)) != 0;
}

// ============================================================================
// OrganicClass — enumeration of plant organic polymer classes
// ============================================================================

enum class OrganicClass : uint8_t {
	Unknown   = 0,
	Cellulose = 1,   // structural polysaccharide
	Lignin    = 2,   // aromatic polymer network
	Pectin    = 3,   // galacturonic-acid gel polymer
};

// ============================================================================
// OrganicClassDescriptor — all metadata for one class
// ============================================================================

struct OrganicClassDescriptor {
	OrganicClass      id;
	std::string_view  name;          // e.g. "cellulose"
	std::string_view  domain;        // e.g. "organic/polymer"
	BondingCharacter  bonding;       // combined bonding flags
	std::string_view  bonding_note;  // human-readable bonding summary
};

// ============================================================================
// organic_class_descriptor() — authoritative lookup
// ============================================================================

inline constexpr OrganicClassDescriptor organic_class_descriptor(OrganicClass cls) {
	switch (cls) {
		case OrganicClass::Cellulose:
			return {
				OrganicClass::Cellulose,
				"cellulose",
				"organic/polymer",
				BondingCharacter::SigmaNet | BondingCharacter::HBond,
				"sigma_net · H-bond network"
			};
		case OrganicClass::Lignin:
			return {
				OrganicClass::Lignin,
				"lignin",
				"organic/ar/polymer",
				BondingCharacter::SigmaNet | BondingCharacter::ArPi,
				"sigma_net · conjugated pi (aromatic)"
			};
		case OrganicClass::Pectin:
			return {
				OrganicClass::Pectin,
				"pectin",
				"organic/gel",
				BondingCharacter::SigmaNet | BondingCharacter::HBond | BondingCharacter::ChargedSite,
				"sigma · H-bond · +/- charged sites"
			};
		default:
			return {
				OrganicClass::Unknown,
				"unknown",
				"organic/unknown",
				BondingCharacter::None,
				"none"
			};
	}
}

// Convenience: look up by name string
inline OrganicClass organic_class_from_name(std::string_view name) {
	if (name == "cellulose") return OrganicClass::Cellulose;
	if (name == "lignin")    return OrganicClass::Lignin;
	if (name == "pectin")    return OrganicClass::Pectin;
	return OrganicClass::Unknown;
}

} // namespace bio
} // namespace vsepr
