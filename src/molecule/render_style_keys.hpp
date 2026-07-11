#pragma once
// ============================================================================
// render_style_keys.hpp — Glass Module: External Colour/Style Key System
// ============================================================================
// No hardcoded colours.  The prerender system emits style keys; a palette
// mapper (cloneable from Python or set per-project) resolves them to RGB.
// ============================================================================

#include <cstdint>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// RenderStyleKey — emitted per atom/bond instance for external palette lookup.
// -----------------------------------------------------------------------
struct RenderStyleKey {
    uint32_t atom_type{};       // atomic number (Z)
    uint32_t flags{};           // selection, highlight, confidence, etc.
};

// -----------------------------------------------------------------------
// Style flag bits (combinable)
// -----------------------------------------------------------------------
namespace StyleFlags {
    constexpr uint32_t None       = 0;
    constexpr uint32_t Selected   = 1u << 0;
    constexpr uint32_t Highlighted= 1u << 1;
    constexpr uint32_t Ghost      = 1u << 2;     // transparent/faded
    constexpr uint32_t Ring       = 1u << 3;     // belongs to a ring system
    constexpr uint32_t Terminal   = 1u << 4;     // leaf/terminal atom
    constexpr uint32_t Aromatic   = 1u << 5;     // aromatic bond/atom
}

// -----------------------------------------------------------------------
// BondStyleKey — per bond instance
// -----------------------------------------------------------------------
struct BondStyleKey {
    uint32_t bond_order{};      // 1,2,3,4 (aromatic)
    uint32_t flags{};
};

} // namespace glass
} // namespace vsepr
