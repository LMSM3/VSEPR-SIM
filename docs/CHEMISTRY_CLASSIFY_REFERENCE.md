# Chemistry Classify Reference

**Module:** `vsepr classify`  
**Subsystem:** VSEPR Analysis · Organic Classification · ChemPlus Reaction Bridge  
**Introduced:** Day 84 development arc  
**Last updated:** 2026-07-03

---

## Overview

`vsepr classify <script.vsim>` is an analysis preview command. It accepts a
`.vsim` script file and prints a multi-section report covering:

1. **[VSEPR]** — per-atom geometry sites, electron-domain topology, and
   shape classification using VSEPR rules.
2. **[OrganicCandidate]** — molecular family inference, functional-group
   detection, and property scores for the formula declared in the script.
3. **[ChemPlus]** *(optional)* — reaction classification, energy parsing, and
   VSEPR geometry linking when the script contains a `[chem_plus]` section.

No simulation is executed. The command is a fast pre-flight check.

---

## Invoking the command

```
vsepr classify path/to/script.vsim
```

Output is written to stdout with ANSI colour when connected to a terminal.

---

## Script structure

A minimal classify-ready script requires a `[project]` block and a `formula`
key (either directly under `[formula]` or anywhere in the flat key space):

```toml
[project]
name = my_molecule

[formula]
formula = NH3
```

### Adding a ChemPlus reaction block

Include a `[chem_plus]` section to enable reaction classification output:

```toml
[project]
name = combustion_demo

[formula]
formula = CH4

[chem_plus]
reaction    = CH4 + 2O2 -> CO2 + 2H2O + 891 kJ
vsepr_link  = true
```

#### Supported `[chem_plus]` keys

| Key | Type | Description |
|-----|------|-------------|
| `reaction` | string | Canonical reaction string, e.g. `"CH4 + 2O2 -> CO2 + 2H2O + 891 kJ"` |
| `preset` | string | Named preset library: `"999"` (8 general reactions) or `"998"` |
| `vsepr_link` | bool | When `true`, resolves a VSEPR geometry tag for the first product |
| `vsepr_link` | bool | When `true`, resolves a VSEPR geometry tag for the first product |
| `class_override` | string | Force reaction class: `combustion`, `decomposition`, `acid-base`, `synthesis`, `general` |
| `energy_kj` | float | Override parsed energy value (kJ/mol) |

Either `reaction` or `preset` must be set for the `[ChemPlus]` block to appear
in the output. Both can coexist; `reaction` is evaluated first.

---

## Output sections

### [VSEPR]

Reports the number of geometry sites found and per-site detail:

```
[VSEPR]
vsepr_report:
  sites:              5
  tetrahedral_count:  1
  ...
  site[0]  Z=6  AX4  shape=tetrahedral  conf=0.10  rms_dev=40.53deg
  site[1]  Z=1  AX1  shape=linear  conf=1.00  rms_dev=0.00deg
```

Field | Meaning
------|---------
`Z`   | Atomic number of the site centre
`AX…` | VSEPR notation: A = centre, X = bonded domains, E = lone pairs
`shape` | Resolved geometry name
`conf`  | Confidence score 0–1 (see Note below)
`rms_dev` | Root-mean-square deviation from ideal bond angles (degrees)

> **Note on confidence scores in classify preview:** The preview builds an
> idealised scaffold geometry from the formula string rather than using a
> relaxed structure. For molecules with lone pairs (e.g. H2O, NH3) this
> produces higher RMS deviations and therefore lower confidence scores than a
> real simulation would give. This is expected behaviour in preview mode.
> See [Bug #3](#bug-3--zero-confidence-on-preview-geometry-for-lone-pair-molecules)
> for the formal tracking note.

### [OrganicCandidate]

Reports the molecular family and property scores:

```
[OrganicCandidate]
organic_candidate:
  formula:         CH4
  primary_family:  alkane  [inferred]
  ...
```

Field | Meaning
------|---------
`formula` | Reconstructed formula from the state (Hill order: C, H, then others by symbol)
`primary_family` | Top-ranked organic family
`sp_count` / `sp2_count` / `sp3_count` | Hybridization site counts
`strain_score` | Estimated geometric strain (0–1)
`decomp_risk` | Estimated decomposition risk (0–1)

> **Note on non-C/non-H formulas:** Organic family classification is designed
> for carbon-based molecules. For inorganic or salt formulas (e.g. `NaCl`,
> `H2O`) the family inference will produce approximate results. The `formula`
> field should display element symbols correctly; see
> [Bug #1](#bug-1--formula-field-displays-zn-instead-of-element-symbol-for-non-ch-atoms).

### [ChemPlus]

Shown only when `[chem_plus]` is present and active in the script:

```
[ChemPlus]
+ CH4 + 2O2 -> CO2 + 2H2O + 891 kJ
	class=combustion  mode=exothermic  energy=891 kJ  vsepr=AX2
```

Field | Meaning
------|---------
`class` | Reaction class: `combustion`, `decomposition`, `acid-base`, `synthesis`, `general`
`mode` | Energy direction: `exothermic`, `endothermic`, `unknown`
`energy` | Parsed energy in kJ (omitted when not present in reaction string)
`vsepr` | VSEPR tag for the first product (only shown when `vsepr_link = true`)

---

## Reaction classification rules

The classifier uses lexical pattern matching on the lowercased reaction string:

| Class | Rule |
|-------|------|
| `combustion` | LHS contains `o2` AND RHS contains `co2` |
| `combustion` | LHS contains `o2` AND RHS contains `h2o` (hydrogen combustion) — *see Bug #2* |
| `decomposition` | LHS has no `+` but RHS has `+` |
| `acid-base` | Reaction contains a recognised acid (`hcl`, `h2so4`, `hno3`) AND a recognised base (`naoh`, `koh`, `ca(oh)2`) |
| `synthesis` | Only via `class_override = synthesis` (not auto-detected) |
| `general` | No other rule matched |

> **Synthesis auto-detection is not yet implemented.** Use `class_override`
> to force `synthesis` for Haber-process and similar reactions.

---

## Preset reaction libraries

| Preset tag | Reactions | Content |
|-----------|-----------|---------|
| `999` | 8 | Common combustion and general reactions: CH4, C2H6, C3H8, 2H2+O2, P2O5+H2O, N2+H2, SO3+H2O, C+O2 |
| `998` | 8 | Alternative preset library (content varies by build) |

---

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | Preview completed successfully |
| `1` | Input file not found or formula could not be parsed |

---

## Examples

### Minimal formula preview

```toml
# minimal.vsim
[project]
name = minimal_h2o

[formula]
formula = H2O
```

```
vsepr classify minimal.vsim
```

### Combustion with VSEPR linkage

```toml
# methane_combustion.vsim
[project]
name = methane_combustion

[formula]
formula = CH4

[chem_plus]
reaction   = CH4 + 2O2 -> CO2 + 2H2O + 891 kJ
vsepr_link = true
```

### Preset 999 survey

```toml
# preset_survey.vsim
[project]
name = preset_survey

[formula]
formula = CO2

[chem_plus]
preset     = 999
vsepr_link = true
```

---

## Known limitations and bugs

See [BUG_REPORT_CHEMISTRY_CLASSIFY.md](BUG_REPORT_CHEMISTRY_CLASSIFY.md) for
the full issue list. Summary:

| ID | Severity | Component | Summary |
|----|----------|-----------|---------|
| Bug #1 | Medium | OrganicCandidate | `formula` field shows `Z8`, `Z11Z17` instead of element symbols for non-C/H atoms |
| Bug #2 | Medium | ChemPlus classifier | Hydrogen combustion (`2H2 + O2 → H2O`) classified as `general` instead of `combustion` |
| Bug #3 | Low | VSEPR confidence | Preview scaffold geometry gives `conf=0.00` for lone-pair molecules (H2O, NH3) |

---

## Related files

| File | Purpose |
|------|---------|
| `src/cli/cmd_classify.cpp` | CLI entry point and [ChemPlus] block rendering |
| `include/vsim/chemplus_declarative.hpp` | ChemPlusSection struct, `evaluate()`, and classify logic |
| `include/vsim/vsim_document.hpp` | `ChemPlusSection` schema definition and `classify()` method |
| `atomistic/classify/vsepr.cpp` | VSEPR site geometry and confidence computation |
| `atomistic/classify/organic_candidate.cpp` | OrganicCandidate family inference and `build_formula()` |
| `tests/test_chemplus_cli.cpp` | Group 92 unit tests for ChemPlus CLI integration |
| `scripts/demo_wo84u_chemplus_cli.py` | Validation script (15/15 PASS, PNG export) |
