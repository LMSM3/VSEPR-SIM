/**
 * xyza_dynamic.cpp
 * ================
 * VSEPR-SIM  |  WO-VSIM-66A  |  .xyza Dynamic State Gate
 *
 * Non-template, translation-unit-level definitions for the xyza_dynamic layer.
 * All template / inline logic lives in include/vsim/io/xyza_dynamic.hpp.
 *
 * This file exists to:
 *   1. Anchor the unit-conversion constants as linkable symbols when callers
 *      need to take their address (unusual, but defensive).
 *   2. Provide a clear compilation unit for future non-inline additions
 *      (e.g. a validated formation-engine binding).
 *   3. Serve as the registered source in CMakeLists.txt so the 66A test
 *      target has a concrete object to link.
 *
 * Column layout confirmed (WO-66A scope audit):
 *   sym  x  y  z  q  vx  vy  vz  fx  fy  fz  e
 *   col:  1  2  3  4   5   6   7   8   9  10  11
 *         ^--always--^  ^---------- extended --------^
 *
 * The fixed 11-column extended line (including sym) is the upper bound.
 * Fewer columns are legal; missing trailing columns are zero-filled by the
 * reader (ZeroFillPolicy::Warn by default).
 *
 * Unit system used on disk (.xyza file):
 *   position    Å  (angstrom)
 *   charge      e  (elementary charge)
 *   velocity    Å/fs
 *   force       kcal/(mol·Å)   [written to file in this unit]
 *   energy      kcal/mol       [written to file in this unit]
 *
 * Unit system inside XyzaAtomRecord (in-memory):
 *   force       eV/Å
 *   energy      eV
 *
 * The conversion constants below document the mapping.
 */

#include "../../include/vsim/io/xyza_dynamic.hpp"

namespace vsepr {
namespace io {

// --------------------------------------------------------------------------
// Unit conversion constants — linkable definitions
// (The inline constexpr in the header already gives them internal linkage;
//  these extern definitions allow external linkage if ever needed.)
// --------------------------------------------------------------------------

// 1 kcal/mol = 0.043364104 eV  (NIST 2022 CODATA)
// 1 eV       = 23.060541945 kcal/mol
//
// These agree with the values used by the VSEPR-SIM energy engine.

// (No additional non-inline code required at this stage.)

} // namespace io
} // namespace vsepr
