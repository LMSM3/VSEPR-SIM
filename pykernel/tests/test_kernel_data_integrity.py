"""
test_kernel_data_integrity.py — Kernel-level data pipe and integrity tests.

Validates:
  1. Kernel header parsing — correct element counts, non-zero data
  2. Data pipe consistency — Python-parsed values match expected reference points
  3. Cross-pipe integrity — cov radius < vdw radius, mass monotonicity
  4. Archive boundary — Z=1–110 fully populated, Z=111–118 excluded
  5. Symbol ↔ Z round-trip fidelity
  6. Public API correctness — functions return expected values for known elements

Anti-black-box: each test documents exactly which kernel source file and
property is being verified.
"""

from __future__ import annotations

import math
import pytest

from pykernel.element_data import (
    SYMBOLS, SYMBOL_TO_Z,
    COVALENT_RADII, VDW_RADII, CPK_COLORS, ATOMIC_MASSES,
    ARCHIVE_Z_MAX,
    cpk_color, covalent_radius, vdw_radius, atomic_mass, is_archived,
    DEFAULT_CPK_COLOR, DEFAULT_COV_RADIUS, DEFAULT_VDW_RADIUS, DEFAULT_ATOMIC_MASS,
)


# ============================================================================
# Section 1 — Kernel header parsing: array sizes and population
# ============================================================================

class TestKernelParsing:
    """Verify that each kernel data pipe was parsed correctly at import time."""

    def test_covalent_radii_array_size(self):
        """src/pot/covalent_radii.hpp must yield exactly 119 entries (Z=0..118)."""
        assert len(COVALENT_RADII) == 119

    def test_vdw_radii_array_size(self):
        """src/pot/vdw_radii.hpp must yield exactly 119 entries (Z=0..118)."""
        assert len(VDW_RADII) == 119

    def test_atomic_masses_array_size(self):
        """src/pot/atomic_masses.hpp must yield exactly 119 entries (Z=0..118)."""
        assert len(ATOMIC_MASSES) == 119

    def test_cpk_colors_array_size(self):
        """src/vis/renderer_base.cpp must yield exactly 119 CPK triplets (Z=0..118)."""
        assert len(CPK_COLORS) == 119

    def test_symbols_array_size(self):
        """SYMBOLS list must have 119 entries (Z=0 placeholder + 118 elements)."""
        assert len(SYMBOLS) == 119

    def test_z0_placeholder_is_empty(self):
        """Z=0 entries are placeholders / unused."""
        assert SYMBOLS[0] == ""
        assert COVALENT_RADII[0] == 0.0
        assert VDW_RADII[0] == 0.0
        assert ATOMIC_MASSES[0] == 0.0

    def test_all_archive_elements_nonzero_covalent(self):
        """Every element Z=1..110 must have a positive covalent radius."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            assert COVALENT_RADII[Z] > 0.0, f"Z={Z} ({SYMBOLS[Z]}) cov radius is {COVALENT_RADII[Z]}"

    def test_all_archive_elements_nonzero_vdw(self):
        """Every element Z=1..110 must have a positive VdW radius."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            assert VDW_RADII[Z] > 0.0, f"Z={Z} ({SYMBOLS[Z]}) vdw radius is {VDW_RADII[Z]}"

    def test_all_archive_elements_nonzero_mass(self):
        """Every element Z=1..110 must have a positive atomic mass."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            assert ATOMIC_MASSES[Z] > 0.0, f"Z={Z} ({SYMBOLS[Z]}) mass is {ATOMIC_MASSES[Z]}"

    def test_all_archive_elements_valid_cpk(self):
        """Every element Z=1..110 must have CPK color channels in [0, 1]."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            r, g, b = CPK_COLORS[Z]
            assert 0.0 <= r <= 1.0, f"Z={Z} red={r}"
            assert 0.0 <= g <= 1.0, f"Z={Z} green={g}"
            assert 0.0 <= b <= 1.0, f"Z={Z} blue={b}"


# ============================================================================
# Section 2 — Data pipe spot checks (known reference values)
# ============================================================================

class TestPipeSpotChecks:
    """Verify that parsed kernel data matches known reference values.

    These are hand-verified values from the original literature.
    Tolerance is ±0.01 for radii, ±0.1 for masses.
    """

    # -- Covalent radii (Pyykkö & Atsumi 2009) --

    def test_cov_hydrogen(self):
        assert math.isclose(COVALENT_RADII[1], 0.32, abs_tol=0.01)

    def test_cov_carbon(self):
        assert math.isclose(COVALENT_RADII[6], 0.75, abs_tol=0.01)

    def test_cov_nitrogen(self):
        assert math.isclose(COVALENT_RADII[7], 0.71, abs_tol=0.01)

    def test_cov_oxygen(self):
        assert math.isclose(COVALENT_RADII[8], 0.63, abs_tol=0.01)

    def test_cov_iron(self):
        assert math.isclose(COVALENT_RADII[26], 1.16, abs_tol=0.01)

    def test_cov_gold(self):
        assert math.isclose(COVALENT_RADII[79], 1.24, abs_tol=0.01)

    # -- VdW radii (Bondi 1964 / Alvarez 2013) --

    def test_vdw_hydrogen(self):
        assert math.isclose(VDW_RADII[1], 1.20, abs_tol=0.01)

    def test_vdw_carbon(self):
        assert math.isclose(VDW_RADII[6], 1.70, abs_tol=0.01)

    def test_vdw_nitrogen(self):
        assert math.isclose(VDW_RADII[7], 1.55, abs_tol=0.01)

    def test_vdw_oxygen(self):
        assert math.isclose(VDW_RADII[8], 1.52, abs_tol=0.01)

    def test_vdw_sulfur(self):
        assert math.isclose(VDW_RADII[16], 1.80, abs_tol=0.01)

    def test_vdw_cesium(self):
        assert math.isclose(VDW_RADII[55], 3.43, abs_tol=0.01)

    # -- Atomic masses (IUPAC 2021) --

    def test_mass_hydrogen(self):
        assert math.isclose(ATOMIC_MASSES[1], 1.008, abs_tol=0.01)

    def test_mass_carbon(self):
        assert math.isclose(ATOMIC_MASSES[6], 12.011, abs_tol=0.01)

    def test_mass_nitrogen(self):
        assert math.isclose(ATOMIC_MASSES[7], 14.007, abs_tol=0.01)

    def test_mass_oxygen(self):
        assert math.isclose(ATOMIC_MASSES[8], 15.999, abs_tol=0.01)

    def test_mass_iron(self):
        assert math.isclose(ATOMIC_MASSES[26], 55.845, abs_tol=0.1)

    def test_mass_gold(self):
        assert math.isclose(ATOMIC_MASSES[79], 196.97, abs_tol=0.1)

    def test_mass_uranium(self):
        assert math.isclose(ATOMIC_MASSES[92], 238.03, abs_tol=0.1)

    def test_mass_darmstadtium(self):
        """Z=110 (Ds) — last archived element."""
        assert math.isclose(ATOMIC_MASSES[110], 281.0, abs_tol=1.0)

    # -- CPK colors (Jmol reference) --

    def test_cpk_hydrogen_white(self):
        r, g, b = CPK_COLORS[1]
        assert r == 1.0 and g == 1.0 and b == 1.0

    def test_cpk_carbon_gray(self):
        r, g, b = CPK_COLORS[6]
        assert math.isclose(r, 0.30, abs_tol=0.01)
        assert math.isclose(g, 0.30, abs_tol=0.01)

    def test_cpk_oxygen_red(self):
        r, g, b = CPK_COLORS[8]
        assert r > 0.9  # Dominant red
        assert g < 0.1
        assert b < 0.1

    def test_cpk_gold_yellow(self):
        r, g, b = CPK_COLORS[79]
        assert r == 1.0
        assert math.isclose(g, 0.82, abs_tol=0.01)


# ============================================================================
# Section 3 — Cross-pipe integrity constraints
# ============================================================================

class TestCrossPipeIntegrity:
    """Physical consistency checks across different data pipes."""

    def test_covalent_less_than_vdw_for_archive(self):
        """Covalent radius must be strictly less than VdW radius for Z=1..110."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            assert COVALENT_RADII[Z] < VDW_RADII[Z], (
                f"Z={Z} ({SYMBOLS[Z]}): cov={COVALENT_RADII[Z]} >= vdw={VDW_RADII[Z]}"
            )

    def test_mass_general_increase(self):
        """Atomic mass generally increases with Z.

        A few inversions are physically real (Ar/K, Co/Ni, Te/I, Th/Pa,
        U/Np, Pu/Am).  We allow up to 6 inversions across the archive.
        """
        inversions = 0
        max_mass = 0.0
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            if ATOMIC_MASSES[Z] < max_mass:
                inversions += 1
            else:
                max_mass = ATOMIC_MASSES[Z]
        assert inversions <= 6, f"Too many mass inversions: {inversions}"

    def test_hydrogen_lightest(self):
        """Hydrogen (Z=1) must have the smallest atomic mass."""
        h_mass = ATOMIC_MASSES[1]
        for Z in range(2, ARCHIVE_Z_MAX + 1):
            assert ATOMIC_MASSES[Z] > h_mass, (
                f"Z={Z} ({SYMBOLS[Z]}) mass {ATOMIC_MASSES[Z]} <= H mass {h_mass}"
            )

    def test_covalent_radius_range(self):
        """Covalent radii should fall within physically reasonable bounds (0.2–2.5 Å)."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            r = COVALENT_RADII[Z]
            assert 0.2 <= r <= 2.5, f"Z={Z} ({SYMBOLS[Z]}): cov={r} out of range"

    def test_vdw_radius_range(self):
        """VdW radii should fall within physically reasonable bounds (1.0–4.0 Å)."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            r = VDW_RADII[Z]
            assert 1.0 <= r <= 4.0, f"Z={Z} ({SYMBOLS[Z]}): vdw={r} out of range"

    def test_mass_positive_range(self):
        """Atomic masses should be between 1.0 and 300.0 Da for Z=1..110."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            m = ATOMIC_MASSES[Z]
            assert 1.0 <= m <= 300.0, f"Z={Z} ({SYMBOLS[Z]}): mass={m} out of range"


# ============================================================================
# Section 4 — Archive boundary enforcement
# ============================================================================

class TestArchiveBoundary:
    """Verify the Z=1–110 archive boundary is correct."""

    def test_archive_z_max_is_110(self):
        assert ARCHIVE_Z_MAX == 110

    def test_darmstadtium_is_archived(self):
        assert is_archived("Ds")

    def test_roentgenium_not_archived(self):
        """Z=111 (Rg) — first excluded element."""
        assert not is_archived("Rg")

    def test_oganesson_not_archived(self):
        """Z=118 (Og) — last element, excluded."""
        assert not is_archived("Og")

    @pytest.mark.parametrize("sym", ["Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"])
    def test_last_8_excluded(self, sym):
        """Elements Z=111–118 must all be outside the archive."""
        assert not is_archived(sym)

    @pytest.mark.parametrize("sym,Z", [
        ("H", 1), ("He", 2), ("C", 6), ("Fe", 26),
        ("Au", 79), ("U", 92), ("Ds", 110),
    ])
    def test_key_elements_archived(self, sym, Z):
        assert is_archived(sym)

    def test_all_archive_symbols_nonempty(self):
        """Every symbol string for Z=1..110 must be non-empty."""
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            assert SYMBOLS[Z] != "", f"Z={Z} has empty symbol"


# ============================================================================
# Section 5 — Symbol ↔ Z round-trip fidelity
# ============================================================================

class TestSymbolRoundTrip:
    """Verify SYMBOLS and SYMBOL_TO_Z are consistent."""

    def test_symbol_to_z_back_to_symbol(self):
        """For every non-empty symbol, Z = SYMBOL_TO_Z[sym] and SYMBOLS[Z] == sym."""
        for Z in range(1, len(SYMBOLS)):
            sym = SYMBOLS[Z]
            if sym:
                assert SYMBOL_TO_Z[sym] == Z, f"SYMBOL_TO_Z[{sym}] = {SYMBOL_TO_Z.get(sym)} != {Z}"

    def test_z_to_symbol_back_to_z(self):
        """For every mapped symbol, round-tripping through SYMBOL_TO_Z works."""
        for sym, Z in SYMBOL_TO_Z.items():
            assert SYMBOLS[Z] == sym, f"SYMBOLS[{Z}] = {SYMBOLS[Z]} != {sym}"

    def test_no_duplicate_symbols(self):
        """No two atomic numbers should share a symbol."""
        seen = {}
        for Z in range(1, len(SYMBOLS)):
            sym = SYMBOLS[Z]
            if sym:
                assert sym not in seen, f"Duplicate symbol {sym}: Z={seen[sym]} and Z={Z}"
                seen[sym] = Z

    def test_symbol_format(self):
        """All symbols should be 1–2 characters, first letter uppercase."""
        for Z in range(1, len(SYMBOLS)):
            sym = SYMBOLS[Z]
            if sym:
                assert 1 <= len(sym) <= 2, f"Z={Z} symbol {sym!r} wrong length"
                assert sym[0].isupper(), f"Z={Z} symbol {sym!r} not capitalized"

    def test_symbol_to_z_count(self):
        """SYMBOL_TO_Z should have exactly 118 entries."""
        assert len(SYMBOL_TO_Z) == 118


# ============================================================================
# Section 6 — Public API correctness
# ============================================================================

class TestPublicAPI:
    """Verify the public lookup functions return correct values."""

    def test_cpk_color_known_element(self):
        c = cpk_color("C")
        assert isinstance(c, tuple) and len(c) == 3
        assert math.isclose(c[0], 0.30, abs_tol=0.01)

    def test_cpk_color_unknown_element(self):
        c = cpk_color("Xx")
        assert c == DEFAULT_CPK_COLOR

    def test_covalent_radius_known(self):
        r = covalent_radius("O")
        assert math.isclose(r, 0.63, abs_tol=0.01)

    def test_covalent_radius_unknown(self):
        r = covalent_radius("Zz")
        assert r == DEFAULT_COV_RADIUS

    def test_vdw_radius_known(self):
        r = vdw_radius("N")
        assert math.isclose(r, 1.55, abs_tol=0.01)

    def test_vdw_radius_unknown(self):
        r = vdw_radius("Zz")
        assert r == DEFAULT_VDW_RADIUS

    def test_atomic_mass_known(self):
        m = atomic_mass("Fe")
        assert math.isclose(m, 55.845, abs_tol=0.1)

    def test_atomic_mass_carbon(self):
        m = atomic_mass("C")
        assert math.isclose(m, 12.011, abs_tol=0.01)

    def test_atomic_mass_unknown(self):
        m = atomic_mass("Zz")
        assert m == DEFAULT_ATOMIC_MASS

    def test_is_archived_true(self):
        assert is_archived("H")
        assert is_archived("Ds")
        assert is_archived("Fe")

    def test_is_archived_false(self):
        assert not is_archived("Og")
        assert not is_archived("Zz")
        assert not is_archived("")


# ============================================================================
# Section 7 — Pipe-to-pipe cross-consistency for specific elements
# ============================================================================

class TestPipeCrossConsistency:
    """Verify that the same element looks up consistently across all pipes."""

    @pytest.mark.parametrize("sym,Z", [
        ("H", 1), ("C", 6), ("N", 7), ("O", 8), ("Fe", 26),
        ("Cu", 29), ("Ag", 47), ("Au", 79), ("U", 92),
    ])
    def test_api_matches_raw_array(self, sym, Z):
        """Public API functions must return the same value as raw array indexing."""
        assert cpk_color(sym) == CPK_COLORS[Z]
        assert covalent_radius(sym) == COVALENT_RADII[Z]
        assert vdw_radius(sym) == VDW_RADII[Z]
        assert atomic_mass(sym) == ATOMIC_MASSES[Z]

    @pytest.mark.parametrize("sym", [
        "H", "Li", "Be", "B", "C", "N", "O", "F", "Ne",
        "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar",
        "K", "Ca", "Fe", "Cu", "Zn", "Br", "Kr",
        "Ag", "I", "Xe", "Au", "Pb", "U", "Ds",
    ])
    def test_all_four_pipes_populated(self, sym):
        """For commonly-used elements, all 4 data pipes must return positive values."""
        assert covalent_radius(sym) > 0.0
        assert vdw_radius(sym) > 0.0
        assert atomic_mass(sym) > 0.0
        r, g, b = cpk_color(sym)
        assert 0.0 <= r <= 1.0
        assert 0.0 <= g <= 1.0
        assert 0.0 <= b <= 1.0


# ============================================================================
# Section 8 — Data pipe completeness audit
# ============================================================================

class TestDataPipeCompleteness:
    """Full sweep: every Z in the archive range has data in all 4 pipes."""

    def test_full_archive_sweep(self):
        """All 110 archive elements must have non-zero values in every pipe."""
        failures = []
        for Z in range(1, ARCHIVE_Z_MAX + 1):
            sym = SYMBOLS[Z]
            if COVALENT_RADII[Z] <= 0.0:
                failures.append(f"Z={Z} ({sym}) cov=0")
            if VDW_RADII[Z] <= 0.0:
                failures.append(f"Z={Z} ({sym}) vdw=0")
            if ATOMIC_MASSES[Z] <= 0.0:
                failures.append(f"Z={Z} ({sym}) mass=0")
            r, g, b = CPK_COLORS[Z]
            if not (0.0 <= r <= 1.0 and 0.0 <= g <= 1.0 and 0.0 <= b <= 1.0):
                failures.append(f"Z={Z} ({sym}) cpk=({r},{g},{b}) invalid")
        assert failures == [], f"Archive pipe failures: {failures}"

    def test_beyond_archive_still_has_kernel_data(self):
        """Z=111–118 still exist in raw kernel arrays (just not archived).

        This verifies the kernel data extends beyond the archive boundary
        and the Python parser didn't truncate it.
        """
        for Z in range(111, 119):
            assert COVALENT_RADII[Z] > 0.0, f"Z={Z} cov missing from kernel"
            assert VDW_RADII[Z] > 0.0, f"Z={Z} vdw missing from kernel"
            assert ATOMIC_MASSES[Z] > 0.0, f"Z={Z} mass missing from kernel"
