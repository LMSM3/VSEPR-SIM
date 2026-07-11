# Bug Report — Chemistry Classify Module

**Component:** `vsepr classify` — VSEPR, OrganicCandidate, ChemPlus  
**Reported:** 2026-07-03  
**Branch:** `day84t-chemplus-declarative-vsepr`  
**Severity legend:** Critical · High · Medium · Low · Info

---

## Bug #1 — `formula` field displays `Z<n>` instead of element symbol for non-C/H atoms

**Severity:** Medium  
**Component:** `atomistic/classify/organic_candidate.cpp` — `build_formula()`  
**Reproducible:** Always

### Description

The `[OrganicCandidate]` section of `vsepr classify` output shows element
symbols as `Z<atomic_number>` for any element that is not carbon (Z=6) or
hydrogen (Z=1). For example:

- `H2O` → `formula: H2Z8` (should be `H2O`)
- `NaCl` → `formula: Z11Z17` (should be `ClNa`)
- `CO2` → `formula: CZ82` (should be `CO2`)

### Root cause

In `atomistic/classify/organic_candidate.cpp`, the `build_formula()` function
maps only C and H to their symbols. All other elements are appended using the
raw atomic-number format `"Z" + std::to_string(z)`:

```cpp
// Line ~95 in organic_candidate.cpp
for (auto& [z, cnt] : others) {
	ss << "Z" << z;          // BUG: should look up element symbol
	if (cnt > 1) ss << cnt;
}
```

### Expected behaviour

The formula field should display standard Hill-order element symbols:
- C and H first (C, then H), then all other elements alphabetically by symbol.
- Non-C/H elements should resolve their two-letter IUPAC symbol from the
  element table (e.g. Z=8 → `O`, Z=11 → `Na`, Z=17 → `Cl`).

### Affected files

- `atomistic/classify/organic_candidate.cpp` — `build_formula()` (line ~83)

### Suggested fix

Replace the `"Z" << z` output with a symbol lookup. A minimal static table
covering Z=1–36 is sufficient for the classify preview path. The element
lookup can be the same `element_Z()` reverse-table already present in
`src/cli/cmd_classify.cpp` (or extracted to a shared header).

### Workaround

None — the output is always wrong for non-C/H formulas. The VSEPR and
ChemPlus sections are unaffected.

---

## Bug #2 — Hydrogen combustion classified as `general` instead of `combustion`

**Severity:** Medium  
**Component:** `include/vsim/vsim_document.hpp` — `ChemPlusSection::classify()`;  
  also `include/vsim/chemplus_declarative.hpp` and `scripts/demo_wo84t_chemplus_declarative.py`  
**Reproducible:** Always with reactions of the form `xH2 + O2 -> yH2O + Z kJ`

### Description

Reactions that combust hydrogen (producing H2O but not CO2) are classified as
`general` instead of `combustion`:

```
2H2 + O2 -> 2H2O + 572 kJ
  class=general   ← wrong, should be class=combustion
```

This affects `vsepr classify` output, the `evaluate()` API, and all three
Day-84 demo scripts.

### Root cause

The combustion detection rule in `ChemPlusSection::classify()` requires **both**:
- `o2` present on the LHS, **and**
- `co2` present on the RHS.

```cpp
// vsim_document.hpp ~line 1798
if (lhs.find("o2") != std::string::npos && rhs.find("co2") != std::string::npos)
	return ReactionClass::combustion;
```

Hydrogen combustion produces `H2O`, not `CO2`, so the rule never fires.
The same logic is duplicated in `chemplus_declarative.hpp` and
`demo_wo84t_chemplus_declarative.py`.

### Expected behaviour

Any reaction where the LHS contains an oxidiser (`o2`) and the RHS contains a
combustion product (`co2` **or** `h2o`) should be classified as `combustion`.

### Affected files

1. `include/vsim/vsim_document.hpp` — `ChemPlusSection::classify()` (~line 1798)
2. `include/vsim/chemplus_declarative.hpp` — `classify_reaction()` internal helper
3. `scripts/demo_wo84t_chemplus_declarative.py` — `classify_reaction()` Python mirror (~line 41–46)

All three must be updated together to keep the C++ and Python logic consistent.

### Suggested fix

Extend the combustion rule to also match `h2o` on the RHS:

```cpp
// C++ fix (vsim_document.hpp and chemplus_declarative.hpp)
bool rhs_has_combustion_product =
	rhs.find("co2") != std::string::npos ||
	rhs.find("h2o") != std::string::npos;
if (lhs.find("o2") != std::string::npos && rhs_has_combustion_product)
	return ReactionClass::combustion;
```

```python
# Python fix (demo_wo84t_chemplus_declarative.py)
if "o2" in lhs and ("co2" in rhs or "h2o" in rhs):
	return "combustion"
```

### Test impact

After fix:
- `2H2 + O2 -> 2H2O + 572 kJ` → `combustion` ✓
- `C + O2 -> CO2 + 394 kJ` → `combustion` ✓ (unchanged)
- `CaCO3 -> CaO + CO2` → `decomposition` ✓ (unchanged — no LHS `o2`)

The Group 92 test `test_cli_vsepr_link_h2o` currently passes because it tests
the `vsepr_tag` (AX2E2 for H2O) rather than the reaction class. Add an
additional assertion:
```cpp
check(res.reactions[0].reaction_class == "combustion", "vsepr_link H2O: class=combustion");
```

### Workaround

Use `class_override = combustion` in the `[chem_plus]` section to force
correct classification for hydrogen combustion reactions.

---

## Bug #3 — Zero confidence score on preview geometry for lone-pair molecules

**Severity:** Low  
**Component:** `atomistic/classify/vsepr.cpp` — `confidence_from_rms()`  
**Reproducible:** Always when `vsepr classify` is run on formulas with lone-pair centres (H2O, NH3, SO2, etc.)

### Description

The VSEPR centre site for lone-pair molecules reports `conf=0.00` in classify
preview mode:

```
site[0]  Z=8  AX2E2  shape=bent  conf=0.00  rms_dev=61.50deg
```

The confidence score is computed as `1.0 - (rms_deg / 45.0)`, clamped to
`[0, 1]`. The preview path builds an idealised scaffold geometry where ligands
are placed on a sphere at 1.5 Å without lone-pair repulsion. This produces
`rms_dev > 45°` for bent/pyramidal molecules, which clamps confidence to zero.

### Root cause

```cpp
// vsepr.cpp line ~140
static double confidence_from_rms(double rms_deg) {
	return std::clamp(1.0 - (rms_deg / 45.0), 0.0, 1.0);
}
```

The 45° normalisation constant is calibrated for relaxed simulation geometries.
Preview scaffold geometries for lone-pair molecules will always exceed this
threshold.

### Expected behaviour

In preview mode the confidence score should either:
- **Option A:** Be suppressed / not shown when running from a scaffold geometry
  (add a `is_preview` flag to the state or site), **or**
- **Option B:** Use a wider normalisation window (e.g. 90°) when the state
  has zero simulation steps (scaffold-only).

A `conf=0.00` value is technically correct by the formula, but is misleading
to users because it suggests the VSEPR classification is wrong when in fact the
shape (`bent`, `AX2E2`) is correctly identified.

### Affected files

- `atomistic/classify/vsepr.cpp` — `confidence_from_rms()` (line ~140)
- `src/cli/cmd_classify.cpp` — could annotate the output with `(preview geometry)` note

### Workaround

The `shape` and `AX…` fields remain correct. Users should read the shape field
rather than relying on `conf` in classify preview output. The classification
pipeline (`OrganicCandidate`, `ChemPlus`) is unaffected.

---

## Summary table

| # | Severity | File(s) | Short description | Status |
|---|----------|---------|-------------------|--------|
| 1 | Medium | `organic_candidate.cpp` | `formula` shows `Z<n>` instead of element symbol | Open |
| 2 | Medium | `vsim_document.hpp`, `chemplus_declarative.hpp`, `demo_wo84t_chemplus_declarative.py` | Hydrogen combustion classified as `general` | Open |
| 3 | Low | `vsepr.cpp` | `conf=0.00` for lone-pair molecules in preview geometry | Open |

---

*Report generated from manual `vsepr classify` output review on branch*
*`day84t-chemplus-declarative-vsepr`, 2026-07-03.*
