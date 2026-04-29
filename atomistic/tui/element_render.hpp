#pragma once
/**
 * element_render.hpp -- Element glyph, colour, and draw-priority table
 * =====================================================================
 * VSEPR-SIM
 *
 * Centralises every rendering decision that is derived from atomic number Z.
 * All functions are deterministic: same Z \u2192 same glyph, colour, priority.
 *
 * Priority system
 * ---------------
 * CRITICAL  -- selected atom, decay event, collision warning, high-force atom
 * HIGH      -- heavy/actinide/lanthanide series
 * NORMAL    -- standard main-group and transition metals
 * LOW       -- hydrogen, background grid placeholders
 *
 * ASCII mode:  single printable character per cell
 * UTF-8 mode:  geometric symbols for richer visual distinction
 *              (enabled by defining VSEPR_TUI_UTF8 before including)
 */

#include "crystal_tui.hpp"   // Colour
#include <cstdint>
#include <string>

namespace atomistic {
namespace tui {

// ============================================================================
// Draw priority
// ============================================================================

enum class DrawPriority : uint8_t {
	LOW      = 0,
	NORMAL   = 1,
	HIGH     = 2,
	CRITICAL = 3
};

// ============================================================================
// Glyph selection (ASCII)
// ============================================================================

inline char glyph_for_element(int Z) {
	switch (Z) {
		// Period 1
		case 1:  return '.';   // H
		case 2:  return 'e';   // He

		// Period 2
		case 3:  return 'L';   // Li
		case 4:  return 'B';   // Be
		case 5:  return 'b';   // B
		case 6:  return 'C';   // C
		case 7:  return 'N';   // N
		case 8:  return 'O';   // O
		case 9:  return 'F';   // F
		case 10: return 'e';   // Ne

		// Period 3
		case 11: return 'a';   // Na
		case 12: return 'g';   // Mg
		case 13: return 'l';   // Al
		case 14: return 'i';   // Si
		case 15: return 'P';   // P
		case 16: return 'S';   // S
		case 17: return 'l';   // Cl
		case 18: return 'r';   // Ar

		// Period 4 d-block and main group
		case 19: return 'K';   // K
		case 20: return 'a';   // Ca
		case 26: return 'e';   // Fe
		case 27: return 'o';   // Co
		case 28: return 'i';   // Ni
		case 29: return 'u';   // Cu
		case 30: return 'n';   // Zn
		case 32: return 'e';   // Ge
		case 35: return 'r';   // Br

		// Period 5
		case 47: return 'g';   // Ag
		case 48: return 'd';   // Cd
		case 50: return 'n';   // Sn
		case 53: return 'I';   // I

		// Period 6
		case 55: return 's';   // Cs
		case 56: return 'a';   // Ba
		case 74: return 'W';   // W  (tungsten)
		case 76: return 'S';   // Os (osmium)
		case 78: return 't';   // Pt
		case 79: return 'u';   // Au
		case 80: return 'g';   // Hg
		case 82: return 'b';   // Pb
		case 83: return 'i';   // Bi

		// Actinides (Z 89-103)
		case 90: return 'T';   // Th
		case 92: return 'U';   // U
		case 93: return 'p';   // Np
		case 94: return 'P';   // Pu
		case 95: return 'm';   // Am
		case 96: return 'C';   // Cm

		default: return '?';
	}
}

// ============================================================================
// Element label (up to 2-char symbol for sidebars / tables)
// ============================================================================

inline std::string label_for_element(int Z) {
	switch (Z) {
		case 1:  return "H";   case 2:  return "He";
		case 3:  return "Li";  case 4:  return "Be";
		case 5:  return "B";   case 6:  return "C";
		case 7:  return "N";   case 8:  return "O";
		case 9:  return "F";   case 10: return "Ne";
		case 11: return "Na";  case 12: return "Mg";
		case 13: return "Al";  case 14: return "Si";
		case 15: return "P";   case 16: return "S";
		case 17: return "Cl";  case 18: return "Ar";
		case 19: return "K";   case 20: return "Ca";
		case 22: return "Ti";  case 24: return "Cr";
		case 25: return "Mn";  case 26: return "Fe";
		case 27: return "Co";  case 28: return "Ni";
		case 29: return "Cu";  case 30: return "Zn";
		case 32: return "Ge";  case 33: return "As";
		case 34: return "Se";  case 35: return "Br";
		case 36: return "Kr";
		case 47: return "Ag";  case 48: return "Cd";
		case 50: return "Sn";  case 51: return "Sb";
		case 52: return "Te";  case 53: return "I";
		case 54: return "Xe";
		case 55: return "Cs";  case 56: return "Ba";
		case 74: return "W";   case 78: return "Pt";
		case 79: return "Au";  case 80: return "Hg";
		case 76: return "Os";
		case 82: return "Pb";  case 83: return "Bi";
		case 84: return "Po";
		case 88: return "Ra";  case 89: return "Ac";
		case 90: return "Th";  case 91: return "Pa";
		case 92: return "U";   case 93: return "Np";
		case 94: return "Pu";  case 95: return "Am";
		case 96: return "Cm";  case 98: return "Cf";
		case 99: return "Es";
		default: {
			// Synthesise from Z for unlisted elements
			return "Z" + std::to_string(Z);
		}
	}
}

// ============================================================================
// Colour per element (by block / period)
// ============================================================================

inline Colour colour_for_element(int Z) {
	if (Z <= 0)   return {140, 140, 140};  // unknown
	if (Z == 1)   return {255, 255, 255};  // H: white
	if (Z == 2)   return {180, 220, 255};  // He: pale blue
	if (Z <= 10)  return {100, 200, 255};  // p-block period 2: cyan
	if (Z <= 18)  return {100, 230, 100};  // p-block period 3: green
	if (Z <= 36)  return {255, 200, 80};   // d-block period 4: gold
	if (Z <= 54)  return {200, 100, 255};  // heavier p-block: violet
	if (Z <= 71)  return {255, 160, 80};   // lanthanides: warm orange
	if (Z <= 86)  return {255, 100, 60};   // d-block period 6: orange-red
	if (Z <= 103) return {160, 60, 255};   // actinides: deep violet
	return {180, 180, 180};                // superheavy: grey
}

// ============================================================================
// Draw priority per element
// ============================================================================

inline DrawPriority draw_priority_for_element(int Z) {
	if (Z <= 0)    return DrawPriority::LOW;
	if (Z == 1)    return DrawPriority::LOW;      // H: background-ish
	if (Z <= 18)   return DrawPriority::NORMAL;
	if (Z <= 86)   return DrawPriority::NORMAL;
	if (Z <= 103)  return DrawPriority::HIGH;     // actinides prominent
	return DrawPriority::NORMAL;
}

// ============================================================================
// Numeric sort key (for z-ordering multiple atoms in same cell)
// Returns higher values for atoms that should be drawn on top.
// ============================================================================

inline int render_sort_key(int Z, DrawPriority priority_override = DrawPriority::NORMAL) {
	return static_cast<int>(priority_override) * 1000 + Z;
}

} // namespace tui
} // namespace atomistic
