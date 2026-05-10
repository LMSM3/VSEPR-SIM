# VSIM Language Reference

> \*\*Maintenance rule:\*\* This document is updated with every change to the VSIM script surface.
> If a block, field, or default changes in the runtime, this document changes in the same commit.
> A runtime behavior not documented here is a bug in this document.

**Current language version:** v5.0.0-beta.9
**Format:** TOML-style flat blocks, max nesting depth 2 (`\[parent.child]`)
**File extension:** `.vsim`
**Validator:** `vsper validate <script.vsim>`

\---

## Table of Contents

1. [Document Conventions](#conventions)
2. [Block Overview](#block-overview)
3. [Resolution Order](#resolution-order)
4. [Internal Classifier Variables](#classifier-variables)
5. [Block: meta](#block-meta)
6. [Block: material](#block-material) _(beta-9 intent layer)_
7. [Block: system](#block-system)
8. [Block: cell](#block-cell)
9. [Block: boundary](#block-boundary)
10. [Block: pbc](#block-pbc)
11. [Block: run](#block-run)
12. [Block: environment](#block-environment) _(beta-9)_
13. [Block: excite.*](#block-excite) _(beta-9)_
14. [Block: observe](#block-observe) _(beta-9)_
15. [Block: analysis](#block-analysis)
16. [Block: visual](#block-visual)
17. [Block: output](#block-output)
18. [Block: override.particle](#block-override-particle) _(beta-9 Level 3)_
19. [Block: raw.object](#block-raw-object) _(beta-9 Level 4)_
20. [Built-in Functions](#builtin-functions)
21. [Global Special Variables](#global-special-variables)
22. [Common Defaults Table](#common-defaults-table)
23. [System Type Ladder](#system-type-ladder)
24. [Registry: structure](#registry-structure)
25. [Registry: material\_class](#registry-material-class)
26. [Registry: chemistry](#registry-chemistry)
27. [Registry: environment](#registry-environment)
28. [Registry: run\_mode](#registry-run-mode)
29. [Registry: solver](#registry-solver)
30. [Registry: forcefield](#registry-forcefield)
31. [Registry: observables](#registry-observables)
32. [Registry: export\_profile](#registry-export-profile)
33. [Registry: geometry\_source](#registry-geometry-source)
34. [Registry: radiation](#registry-radiation)
35. [Changelog](#changelog)

\---

## 1\. Document Conventions {#conventions}

### Field table format

Each field is documented as:

|Column|Meaning|
|-|-|
|Field|Key name as written in script|
|Type|Expected value type|
|Default|Value applied if field is absent|
|Default source|Why that default applies|
|Required|Whether null is an error|
|Status|`stable` / `partial` / `planned` / `confirm`|

### Type notation

|Notation|Meaning|
|-|-|
|`string`|Unquoted or quoted text|
|`float`|Floating-point number|
|`int`|Integer|
|`bool`|`true` or `false`|
|`enum(a,b,c)`|One of the listed values|
|`float3`|Three floats, comma-separated: `1.0, 2.0, 3.0`|
|`string\[]`|Comma-separated list of strings|
|`auto`|User requests runtime resolution|

### Default source notation

|Notation|Meaning|
|-|-|
|`universal`|Always applied regardless of context|
|`context:<type>`|Applied when `\_\_system\_type\_\_` matches|
|`derived:<field>`|Computed from another resolved field|
|`none`|No default; field is optional|
|`error`|No default; field is required|

\---

## 2\. Block Overview {#block-overview}

```
.vsim
 |
 +--> \[meta]      identifies the script
 |
 +--> \[system]    builds the starting state
 |
 +--> \[cell]      defines simulation box geometry
 |
 +--> \[boundary]  defines per-axis boundary conditions
 |
 +--> \[pbc]       defines periodic boundary runtime options
 |
 +--> \[run]       advances the simulation
 |
 +--> \[analysis]  measures system behavior
 |
 +--> \[visual]    emits frames and live display
 |
 +--> \[output]    writes files and reports
```

All blocks are optional. A script with only `\[system]` and `\[run]` is valid.
Blocks that are absent are filled entirely by the resolution ladder.
A script with no blocks at all is valid only if a source file provides all required fields.

\---

## 3\. Resolution Order {#resolution-order}

Every field resolves by walking this priority chain, stopping at the first hit:

```
1. explicit value        user wrote field = value
2. auto keyword          user wrote field = auto → runtime resolves and logs result
3. context default       \_\_system\_type\_\_ ladder provides a value → logged as \[DEFAULT]
4. universal default     always-valid fallback → logged as \[DEFAULT]
5. derived value         computed from another resolved field → logged as \[DERIVED]
6. null
     if required → error with explanation
     if optional → silently absent
```

**Logging behavior:**

```
\[INFERRED]  field = value    (auto was requested — runtime resolved)
\[DEFAULT]   field = value    (field was absent — context or universal default applied)
\[DERIVED]   field = value    (computed from resolved field X)
\[CONFLICT]  field            (explicit value conflicts with derived value — explicit wins, warning issued)
```

The user can always inspect what the runtime decided by reading the run log or the output report.

\---

## 4\. Internal Classifier Variables {#classifier-variables}

These variables are set by the runtime after reading `\[system]`. They are not user-facing. The user never writes them. Every context default in the ladder is a function of these flags.

|Variable|Type|Values|Set from|
|-|-|-|-|
|`\_\_system\_type\_\_`|enum|see ladder|lattice type, topology, boundary, density|
|`\_\_is\_periodic\_\_`|bool|true/false|boundary block or detected|
|`\_\_is\_charged\_\_`|bool|true/false|species charges|
|`\_\_is\_molecular\_\_`|bool|true/false|molecular topology present|
|`\_\_has\_dispersion\_\_`|bool|true/false|neutral species or explicit dispersion block|
|`\_\_has\_electrostatics\_\_`|bool|true/false|`\_\_is\_charged\_\_` = true|
|`\_\_boundary\_axes\_\_`|enum|xyz/xy/z/none|boundary block|
|`\_\_n\_species\_\_`|int|≥ 1|species list|
|`\_\_n\_atoms\_\_`|int|≥ 1|structure|
|`\_\_charge\_neutral\_\_`|bool|true/false|sum of species charges|
|`\_\_has\_framework\_\_`|bool|true/false|porous topology detected|

These are readable in `\[script]` and `\[post\_step]` blocks for conditional logic, but not writable.

\---

## 5\. Block: `\[meta]` {#block-meta}

Identifies the script. Passive — no derive logic. All fields optional.

```vsim
\[meta]
name        = my\_simulation
version     = 1.0.0
author      = 
description = 
```

|Field|Type|Default|Required|Status|
|-|-|-|-|-|
|`name`|string|filename without extension|no|stable|
|`version`|string|none|no|stable|
|`author`|string|none|no|stable|
|`description`|string|none|no|stable|

\---

## 6\. Block: `\[material]` — beta-9 intent layer {#block-material}

Primary entry point for minimal lab scripts. When present, the runtime passes it through the **registry resolution engine** before populating `\[system]`. Fields resolved from the registry are logged as `\[REGISTRY]`.

`\[material]` and `\[system]` are complementary. `\[material]` is the high-intent surface. `\[system]` is the low-level override layer.

**Authoring levels:**

|Level|Keys used|What happens|
|-|-|-|
|0 — casual|`formula` + `structure`|Alias resolved to prototype; registry expands full geometry|
|1 — prototype|`formula` + `prototype`|Deterministic key; registry fills space group, basis, generator|
|1 — crystallographic|`formula` + `space_group` + `basis`|Explicit Wyckoff positions; no registry geometry needed|
|2+|Add `cell`, `phase`, overrides|Registry fills what the user does not specify|

**Resolution hierarchy** (highest wins):

```
explicit basis + space_group  >  prototype  >  structure alias  >  registry
```

```vsim
\[material]
formula   = NaCl
structure = rocksalt
cell      = 4x4x4
```

|Field|Type|Default|Required|Status|
|-|-|-|-|-|
|`formula`|string|none|no|stable|
|`prototype`|string|none|no|stable|
|`structure`|string|none|no|stable|
|`space_group`|string|none|no|partial|
|`lattice`|string|none|no|partial|
|`basis`|string|none|no|partial|
|`cell`|string|none|no|partial|
|`phase`|enum|`auto`|no|planned|

`phase` values: `solid`, `liquid`, `gas`, `amorphous`

Selected alias examples (full table: `VSIM_REFERENCE.md`, Registry: structure section):

|`structure =`|Resolves to|
|-|-|
|`rocksalt`, `halite`, `nacl`|`B1_NaCl`|
|`diamond`, `silicon`, `germanium`|`A4_diamond`|
|`bcc` / `body_centered_cubic`|`A2_bcc`|
|`fcc` / `face_centered_cubic`|`A1_fcc`|
|`perovskite`|`ABO3_perovskite`|
|`corundum`, `alpha_alumina`|`D5_Al2O3_corundum`|
|`tetrahedral`|`geom_tetrahedral`|
|`bead_chain`|`bead_linear_chain`|
|`powder_bed`|`premacro_powder_bed`|

Unknown aliases pass through verbatim (forward-compatible).

**Status:** Schema and parser complete (WO-VSIM-03B). Registry resolution engine pending (WO-VSIM-03C, beta-9 active).

\---

## 7\. Block: `\[system]` {#block-system}

Builds the starting state. This is the most complex block. Most derive logic lives here.

```vsim
\[system]
type        = auto
formula     = CaF2
source      = positions.xyz
elements    = Ca, F
charge\_model = formal
```

### 6.1 System type

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`type`|enum / `auto`|`auto`|universal|no|partial|

Valid explicit values:

|Value|Meaning|
|-|-|
|`auto`|Runtime classifies from available inputs|
|`bulk\_crystal`|Periodic 3D crystal|
|`surface\_slab`|Periodic xy, open z|
|`molecule`|Non-periodic isolated molecule|
|`molecular\_crystal`|Periodic crystal with intact molecules|
|`liquid`|Periodic, disordered, high density|
|`gas`|Open boundary, low density|
|`porous\_framework`|Periodic with significant pore volume|
|`unknown`|Classification failed — only universal defaults apply|

When `type = auto`, the classifier runs the system type ladder (see section 17).
When classification succeeds, result is logged as `\[INFERRED]`.
When classification fails, `type = unknown` and a warning is issued.

### 6.2 Composition

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`formula`|string|derived from elements|derived|no|partial|
|`elements`|string\[]|read from source file|derived|if no source|stable|
|`charge\_model`|enum|`auto`|universal|no|partial|

`charge\_model` values:

|Value|Meaning|
|-|-|
|`auto`|Formal if ionic detected, none if neutral|
|`formal`|Integer oxidation state charges from element registry|
|`neutral`|All charges zero|
|`custom`|Per-species charges specified in `\[species.\*]` blocks|

When `charge\_model = formal` or `auto` resolves to formal, charges and masses are looked up from the internal element registry. Explicit `\[species.\*]` blocks override registry values.

### 6.3 Structure

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`source`|string (path)|none|none|if no generator|stable|
|`generator`|enum / `auto`|`auto`|universal|no|partial|
|`supercell`|int3 / `auto`|`auto`|derived:rcut|no|stable|
|`spacing`|float|none|none|if generator needs it|stable|
|`a0`|float|none|none|if generator needs it|stable|
|`fractional\_coords`|block|none|none|no|partial|

`generator` values (confirm against codebase):

```
fcc\_lattice         simple\_cubic        bcc\_lattice
fluorite\_lattice    zincblende\_lattice  wurtzite\_lattice
rutile\_lattice      quartz\_lattice      graphite\_ab\_lattice
fcc\_ionic\_lattice   corundum\_lattice    cif\_import
fragment            auto
```

When `generator = auto`, the runtime attempts to detect structure type from source positions.
When `supercell = auto`, the runtime selects the smallest supercell satisfying `rcut < L/2`.

### 6.4 Initial state

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`velocities`|enum|`none`|universal|no|confirm|
|`temperature\_init`|float|from `\[run]` temperature|derived|no|confirm|
|`relax\_before\_run`|bool|`false`|universal|no|confirm|

\---

## 8\. Block: `\[cell]` {#block-cell}

Defines simulation box geometry. Required if periodic boundaries are used and no source file provides cell metadata.

```vsim
\[cell]
type    = orthorhombic
lengths = 10.0, 10.0, 10.0
units   = angstrom
```

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`type`|enum|`orthorhombic`|universal|no|stable|
|`lengths`|float3|none|none|if periodic|stable|
|`units`|enum|`angstrom`|universal|no|stable|

`type` values:

|Value|Status|
|-|-|
|`orthorhombic`|stable|
|`triclinic`|reserved|

`units` values: `angstrom`, `nm` (reserved), `bohr` (reserved)

Cell metadata may also be provided in `.xyz` comment line:

```
64
cell="10.0 10.0 10.0" boundary="p p p" units="angstrom"
```

Script `\[cell]` block takes precedence over `.xyz` comment metadata.

\---

## 9\. Block: `\[boundary]` {#block-boundary}

Defines per-axis boundary conditions.

```vsim
\[boundary]
x = periodic
y = periodic
z = open
```

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`x`|enum|context|context:system\_type|no|stable|
|`y`|enum|context|context:system\_type|no|stable|
|`z`|enum|context|context:system\_type|no|stable|

Axis values:

|Value|Status|
|-|-|
|`periodic` / `p`|stable|
|`open` / `o`|stable|
|`reflective`|reserved|
|`absorbing`|reserved|

Compact form (sets all axes equally):

```vsim
\[boundary]
mode = periodic
axes = x,y,z
```

Context defaults by system type:

|System type|x|y|z|
|-|-|-|-|
|`bulk\_crystal`|periodic|periodic|periodic|
|`surface\_slab`|periodic|periodic|open|
|`molecule`|open|open|open|
|`molecular\_crystal`|periodic|periodic|periodic|
|`liquid`|periodic|periodic|periodic|
|`gas`|open|open|open|
|`porous\_framework`|periodic|periodic|periodic|

\---

## 10\. Block: `\[pbc]` {#block-pbc}

Runtime options for periodic boundary behavior. Only relevant when at least one axis is periodic.

```vsim
\[pbc]
minimum\_image        = true
wrap\_positions       = after\_step
track\_images         = true
unwrap\_for\_diffusion = true
```

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`minimum\_image`|bool|`true`|universal|no|stable|
|`wrap\_positions`|enum|`after\_step`|universal|no|stable|
|`track\_images`|bool|`true`|universal|no|stable|
|`unwrap\_for\_diffusion`|bool|`true`|universal|no|stable|

`wrap\_positions` values: `never`, `after\_step`, `after\_force`, `on\_export`

\---

## 11\. Block: `\[run]` {#block-run}

Controls simulation execution.

```vsim
\[run]
mode            = fire\_relax
steps           = 1000
dt              = 0.001
temperature     = 300.0
pressure        = 1.0
seed            = 42
integrator      = auto
force\_tolerance = 1e-6
```

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`mode`|enum|`static`|universal|no|stable|
|`steps`|int|`0`|universal|no|stable|
|`dt`|float|`0.001`|universal|no|stable|
|`temperature`|float|`300.0`|universal|no|stable|
|`pressure`|float|`1.0`|context:bulk\_crystal|no|stable|
|`seed`|int|`42`|universal|no|stable|
|`integrator`|enum / `auto`|`auto`|universal|no|confirm|
|`force\_tolerance`|float|`1e-6`|universal|no|stable|
|`save\_every`|int|`50`|universal|no|stable|

`mode` values:

|Value|Meaning|Status|
|-|-|-|
|`static`|Single-point energy, no dynamics|stable|
|`fire\_relax`|FIRE geometry relaxation|stable|
|`relax\_lattice\_c`|Relax c-axis only|partial|
|`md\_nve`|Molecular dynamics, NVE|confirm|
|`md\_nvt`|Molecular dynamics, NVT|confirm|
|`md\_npt`|Molecular dynamics, NPT|confirm|

`integrator` values (when `mode` requires dynamics):

|Value|Status|
|-|-|
|`auto`|Selected from mode|
|`fire`|stable|
|`verlet`|confirm|
|`leapfrog`|confirm|

`pressure` context defaults:

|System type|Default pressure|
|-|-|
|`bulk\_crystal`|1.0 bar|
|`surface\_slab`|1.0 bar|
|`liquid`|1.0 bar|
|`molecule`|irrelevant|
|`gas`|irrelevant|

\---

## 12\. Block: `\[environment]` — beta-9 {#block-environment}

Describes the physical environment surrounding the simulation cell. When present, overrides or extends `\[system]` environment assumptions. Fields resolved from the `environment` registry are logged as `\[REGISTRY]`.

```vsim
\[environment]
periodic    = true
temperature = 300
medium      = vacuum
```

|Field|Type|Default|Default source|Status|
|-|-|-|-|-|
|`periodic`|bool|`false`|universal|partial|
|`temperature`|float|`300.0` K|universal|stable|
|`pressure`|float|`0.0` GPa|universal|stable|
|`medium`|string|`vacuum`|universal|partial|
|`humidity`|float|`0.0`|universal|planned|
|`field_x` / `field_y` / `field_z`|float|`0.0` V/Å|universal|planned|

`medium` values: `vacuum`, `water`, `argon`, `helium`, `air`, `molten_salt`

**Status:** Schema and parser complete (WO-VSIM-03B). Runtime bridge pending (beta-9).

\---

## 13\. Block: `\[excite.*]` — beta-9 {#block-excite}

Named excitation subsection. Multiple `\[excite.*]` blocks may appear in one script, each keyed by excitation type. The subtype name after the dot (`laser`, `xray`, `electron_beam`, `thermal_spike`) becomes the registry key.

```vsim
\[excite.laser]
axis           = z
polarization   = x
intensity      = 1.0
pulse_width_fs = 100
```

|Field|Type|Default|Status|
|-|-|-|-|
|`axis`|enum|none|partial|
|`polarization`|string|none|partial|
|`intensity`|float|`1.0`|partial|
|`pulse_width_fs`|float|`100.0`|partial|
|`photon_energy_eV`|float|`0.0`|planned|
|`fluence`|float|`0.0`|planned|
|`profile`|enum|none|planned|

`profile` values: `gaussian`, `flat`, `sech2`

**Status:** Schema and parser complete (WO-VSIM-03B). Dispatch wiring pending (beta-9).

\---

## 14\. Block: `\[observe]` — beta-9 {#block-observe}

Declares which physical observables to measure during the run. Complements `\[analysis]` (which is post-run); `\[observe]` is for in-run metric collection.

```vsim
\[observe]
metrics = energy_map, interference, spectral_response
```

|Field|Type|Default|Status|
|-|-|-|-|
|`metrics`|string\[]|`[]`|partial|
|`output_format`|enum|`auto`|partial|
|`every_n_steps`|int|`1`|partial|

`output_format` values: `csv`, `json`, `svg`, `auto`

**Status:** Schema and parser complete (WO-VSIM-03B). Metric collection wiring pending (beta-9).

\---

## 15\. Block: `\[analysis]` {#block-analysis}

Declares which analyses to run after or during simulation. All fields default to `false` unless the system type makes them obviously relevant.

```vsim
\[analysis]
rdf               = true
coordination      = true
energy\_trend      = true
rmsd              = true
madelung\_energy   = true
```

|Field|Type|Default|Default source|Status|
|-|-|-|-|-|
|`rdf`|bool|`false`|none|stable|
|`rdf\_pairs`|string\[]|all pairs|derived|stable|
|`rdf\_rmax`|float|`L/2`|derived:cell|stable|
|`rdf\_bins`|int|`200`|universal|stable|
|`coordination`|bool|`false`|none|stable|
|`energy\_trend`|bool|`false`|none|stable|
|`rmsd`|bool|`false`|none|stable|
|`madelung\_energy`|bool|`false`|none|partial|
|`madelung\_constant`|bool|`false`|none|partial|
|`bond\_lengths`|bool|`false`|none|confirm|
|`bond\_angle`|bool|`false`|none|confirm|
|`diffusion`|bool|`false`|none|planned|
|`msd`|bool|`false`|none|planned|
|`fingerprint`|bool|`false`|none|partial|
|`topology`|bool|`false`|none|partial|
|`topology\_rings`|bool|`false`|none|partial|
|`pore\_topology`|bool|`false`|none|planned|
|`pore\_diameter`|bool|`false`|none|planned|
|`framework\_density`|bool|`false`|none|planned|
|`packing\_fraction`|bool|`false`|none|partial|
|`defect\_fraction`|bool|`false`|none|partial|
|`vacancy\_migration`|bool|`false`|none|partial|
|`charge\_neutrality`|bool|`true`|context:charged|stable|
|`kabsch\_rmsd`|bool|`false`|none|confirm|
|`anisotropy\_ratio`|bool|`false`|none|partial|
|`interlayer\_spacing`|bool|`false`|none|partial|
|`lattice\_constants`|bool|`false`|none|partial|
|`surface\_residence`|bool|`false`|none|partial|
|`interaction\_energy`|bool|`false`|none|partial|

### `\[expected]` sub-block

Optional sub-block for validation scripts. Fields are checked against analysis results. Any mismatch is reported as a validation failure, not a crash.

```vsim
\[expected]
coordination\_Ca       = 8
coordination\_Ca\_tol   = 0
madelung\_constant     = 5.03879
madelung\_constant\_tol = 0.001
charge\_sum            = 0.0
charge\_sum\_tol        = 1e-10
```

Convention: every expected value has a corresponding `\_tol` field. If `\_tol` is absent, exact match is required.

\---

## 16\. Block: `\[visual]` {#block-visual}

Controls frame emission and live display. Independent of correctness. Safe to omit entirely.

```vsim
\[visual]
enabled            = false
render\_interval    = 50
display\_fps        = 30
external\_renderer  = none
```

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`enabled`|bool|`false`|universal|no|partial|
|`render\_interval`|int|`50`|universal|no|partial|
|`display\_fps`|int|`30`|universal|no|partial|
|`external\_renderer`|string / `none`|`none`|universal|no|confirm|
|`show\_rdf`|bool|`false`|universal|no|confirm|
|`show\_energy`|bool|`false`|universal|no|confirm|
|`show\_coordination`|bool|`false`|universal|no|confirm|
|`gl\_window\_width`|int|`1280`|universal|no|partial|
|`gl\_window\_height`|int|`800`|universal|no|partial|
|`gl\_auto\_orbit`|bool|`false`|universal|no|partial|
|`overlay\_sequence`|string\[]|none|none|no|partial|

\---

## 17\. Block: `\[output]` {#block-output}

Controls file output. All fields default to `false` except `output\_dir`.

```vsim
\[output]
output\_dir          = output/
write\_xyz           = true
write\_xyzf          = false
write\_analysis\_json = true
write\_report\_md     = true
write\_metrics\_tsv   = false
write\_events\_json   = false
write\_pbc\_images    = false
```

|Field|Type|Default|Default source|Required|Status|
|-|-|-|-|-|-|
|`output\_dir`|string|`output/`|universal|no|stable|
|`write\_xyz`|bool|`false`|none|no|stable|
|`write\_xyzf`|bool|`false`|none|no|stable|
|`write\_xyzfull`|bool|`false`|none|no|stable|
|`write\_analysis\_json`|bool|`false`|none|no|stable|
|`write\_report\_md`|bool|`false`|none|no|stable|
|`write\_metrics\_tsv`|bool|`false`|none|no|stable|
|`write\_events\_json`|bool|`false`|none|no|partial|
|`write\_symbolic\_trace\_json`|bool|`false`|none|no|partial|
|`write\_pbc\_images\_tsv`|bool|`false`|none|no|partial|

Output file layout under `output\_dir`:

```
output/
  state/
    initial.xyz
    relaxed.xyz
    trajectory.xyzf
    pbc\_images.tsv
  analysis/
    metrics.json
    pbc\_validation.json
  report/
    report.md
```

\---

## 18\. Block: `\[\[override.particle\]\]` — beta-9 Level 3 {#block-override-particle}

Array-of-tables. Each block selectively mutates one particle before or during a run. Use for targeted initial condition overrides — not for building structures.

```vsim
\[\[override.particle\]\]
id       = 14
velocity = 0.0, 0.0, 3.0
charge   = -1.0
```

Multiple `\[\[override.particle\]\]` blocks are allowed. They are applied in order after structure generation.

|Field|Type|Default|Required|Status|
|-|-|-|-|-|
|`id`|int|none|yes|partial|
|`velocity`|float3|unset|no|partial|
|`position`|float3|unset|no|partial|
|`charge`|float|`0.0`|no|partial|
|`mass_scale`|float|`1.0`|no|planned|
|`fixed`|bool|`false`|no|planned|

**Status:** Schema and parser complete (WO-VSIM-03B). Runtime application pending (beta-9).

\---

## 19\. Block: `\[\[raw.object\]\]` — beta-9 Level 4 (debug / import only) {#block-raw-object}

Array-of-tables. Explicit particle injection. **Not the main experience.** Reserved for tests, importers, file bridges, and debugging. The double-bracket syntax and the name are intentional signals.

```vsim
\[\[raw.object\]\]
id       = debug_particle_001
species  = C
position = 0, 0, 0
velocity = 0, 0, 1
```

|Field|Type|Default|Required|Status|
|-|-|-|-|-|
|`id`|string|none|no|partial|
|`species`|string|none|yes|partial|
|`position`|float3|`0, 0, 0`|no|partial|
|`velocity`|float3|`0, 0, 0`|no|partial|
|`charge`|float|`0.0`|no|partial|
|`mass`|float|`0.0` (derive from species)|no|planned|
|`label`|string|none|no|planned|

`species` accepts element symbols (`C`, `Na`, `Fe`) or reserved particle tokens (`alpha`, `ghost`).

**Status:** Schema and parser complete (WO-VSIM-03B). Runtime injection pending (beta-9).

\---

## 20\. Built-in Functions {#builtin-functions}

Available in `\[script]` and `\[post\_step]` blocks.

### `particle.\*`

|Function|Signature|Returns|Status|
|-|-|-|-|
|`particle.position(id)`|`(int) → XYZVec3`|Wrapped position of particle|stable|

Particle IDs are 1-indexed. `particle.position(0)` raises an error.

### `pbc.\*`

All `pbc.\*` functions require at least one periodic axis. Calling them in an open system raises a configuration error.

|Function|Signature|Returns|Status|
|-|-|-|-|
|`pbc.wrap(r)`|`(XYZVec3) → XYZVec3`|Wrapped position|stable|
|`pbc.delta(ri, rj)`|`(XYZVec3, XYZVec3) → XYZVec3`|Min-image displacement|stable|
|`pbc.distance(ri, rj)`|`(XYZVec3, XYZVec3) → float`|Min-image distance|stable|
|`pbc.crossed\_boundary(id)`|`(int) → bool`|Crossed last step|stable|
|`pbc.image\_count(id)`|`(int) → Int3`|Cumulative image indices|stable|
|`pbc.unwrap(id)`|`(int) → XYZVec3`|Unwrapped trajectory position|stable|

`pbc.image\_count` returns an `Int3` with fields `.x`, `.y`, `.z`.
`pbc.crossed\_boundary` and `pbc.image\_count` and `pbc.unwrap` require `track\_images = true`.

### Constructors

|Function|Signature|Returns|Status|
|-|-|-|-|
|`xyzvec3(x, y, z)`|`(float, float, float) → XYZVec3`|XYZVec3 literal|stable|

### Assertions (test scripts only)

|Function|Signature|Status|
|-|-|-|
|`assert\_near(a, b, tol, msg)`|`(float, float, float, string)`|stable|
|`assert\_eq(a, b, msg)`|`(any, any, string)`|stable|
|`expect\_error(fn, msg\_substr) { ... }`|block form|stable|

\---

## 21\. Global Special Variables {#global-special-variables}

Read-only in scripts. Set by the runtime before script execution.

|Variable|Type|Meaning|
|-|-|-|
|`step.current`|int|Current simulation step|
|`step.total`|int|Total steps in run|
|`time.current`|float|Current simulation time (ps)|
|`cell.Lx`|float|Cell length x (Å)|
|`cell.Ly`|float|Cell length y (Å)|
|`cell.Lz`|float|Cell length z (Å)|
|`system.n\_atoms`|int|Total atom count|
|`system.n\_species`|int|Number of distinct species|
|`system.formula`|string|Chemical formula|
|`system.type`|string|Resolved system type|
|`energy.total`|float|Total energy (eV)|
|`energy.potential`|float|Potential energy (eV)|
|`energy.kinetic`|float|Kinetic energy (eV)|
|`temperature.current`|float|Instantaneous temperature (K)|
|`pressure.current`|float|Instantaneous pressure (bar)|

Internal classifier variables (read-only, for conditional logic):

|Variable|Type|
|-|-|
|`\_\_system\_type\_\_`|string|
|`\_\_is\_periodic\_\_`|bool|
|`\_\_is\_charged\_\_`|bool|
|`\_\_is\_molecular\_\_`|bool|
|`\_\_has\_dispersion\_\_`|bool|
|`\_\_has\_electrostatics\_\_`|bool|
|`\_\_charge\_neutral\_\_`|bool|
|`\_\_n\_atoms\_\_`|int|
|`\_\_n\_species\_\_`|int|

\---

## 22\. Common Defaults Table {#common-defaults-table}

Full list of fields that have universal or context defaults. Fields not listed here have no default and are either optional (absent = feature disabled) or required (absent = error).

|Field|Default|Source|
|-|-|-|
|`\[meta] name`|filename|universal|
|`\[system] type`|`auto`|universal|
|`\[system] charge\_model`|`auto`|universal|
|`\[system] generator`|`auto`|universal|
|`\[system] supercell`|`auto`|universal|
|`\[system] velocities`|`none`|universal|
|`\[cell] type`|`orthorhombic`|universal|
|`\[cell] units`|`angstrom`|universal|
|`\[boundary] x/y/z`|context|system type ladder|
|`\[pbc] minimum\_image`|`true`|universal|
|`\[pbc] wrap\_positions`|`after\_step`|universal|
|`\[pbc] track\_images`|`true`|universal|
|`\[pbc] unwrap\_for\_diffusion`|`true`|universal|
|`\[run] mode`|`static`|universal|
|`\[run] steps`|`0`|universal|
|`\[run] dt`|`0.001`|universal|
|`\[run] temperature`|`300.0` K|universal|
|`\[run] pressure`|`1.0` bar|context:bulk\_crystal/surface\_slab/liquid|
|`\[run] pressure`|irrelevant|context:gas/molecule|
|`\[run] seed`|`42`|universal|
|`\[run] force\_tolerance`|`1e-6`|universal|
|`\[run] save\_every`|`50`|universal|
|`\[run] integrator`|`auto`|universal|
|`\[analysis] charge\_neutrality`|`true`|context:charged|
|`\[analysis] rdf\_bins`|`200`|universal|
|`\[analysis] rdf\_rmax`|`L/2`|derived:cell|
|`\[output] output\_dir`|`output/`|universal|
|`\[visual] enabled`|`false`|universal|
|`\[visual] render\_interval`|`50`|universal|
|`\[visual] display\_fps`|`30`|universal|
|`\[visual] gl\_window\_width`|`1280`|universal|
|`\[visual] gl\_window\_height`|`800`|universal|

\---

## 23\. System Type Ladder {#system-type-ladder}

Evaluated top to bottom. First match wins. Sets `\_\_system\_type\_\_`.

```
has lattice generator
  AND boundary = periodic xyz (explicit or context)
  AND \_\_is\_molecular\_\_ = false
      → bulk\_crystal

has lattice generator
  AND boundary.xy = periodic
  AND boundary.z = open
      → surface\_slab

has molecular topology
  AND boundary = open
      → molecule

has molecular topology
  AND boundary = periodic xyz
      → molecular\_crystal

has framework topology
  AND pore\_volume > threshold
  AND boundary = periodic xyz
      → porous\_framework

no lattice
  AND density < threshold
  AND boundary = open
      → gas

no lattice
  AND density ≥ threshold
  AND boundary = periodic
      → liquid

nothing matched
      → unknown
      \[WARNING] System type could not be classified.
                Only universal defaults will apply.
                Specify \[system] type = <type> to override.
```

\---

\---

## Registry Design Rule {#registry-design-rule}

Each registry keyword resolves a bundle of defaults. The rule is simple:

> \*\*Explicit user value always wins. Registry provides defaults. No conflicts, no warnings, no negotiation.\*\*

Resolution order with registries inserted:

```
1. explicit value
2. auto keyword → runtime resolves
3. registry bundle defaults   ← new layer, from structure/material\_class/etc.
4. context defaults           (system type ladder)
5. universal defaults
6. derived values
7. null → error or silent absent
```

Multiple registries may be active at once. If two registries set the same field, the one listed first in the block wins. Explicit user value always overrides all of them.

Resolved registry fields are logged as `\[REGISTRY]`:

```
\[REGISTRY]  generator       = fluorite\_lattice   (from: structure = fluorite)
\[REGISTRY]  coordination\_Ca = 8                  (from: structure = fluorite)
\[REGISTRY]  electrostatics  = ewald              (from: material\_class = ionic\_crystal)
\[REGISTRY]  observables     = energy, rmsd, rdf  (from: export\_profile = research\_report)
```

\---

## 24\. Registry: `structure` {#registry-structure}

Set in `\[system]` or `\[material]` block.

```vsim
structure = "fluorite"
```

Resolves geometry prototype, space group, basis, site coordination, and default generator.

|Value|Prototype|Space group|Basis|Coord A|Coord B|Generator|
|-|-|-|-|-|-|-|
|`rocksalt`|NaCl|Fm-3m (225)|AB|6|6|`rocksalt\_lattice`|
|`fluorite`|CaF₂|Fm-3m (225)|AB₂|8|4|`fluorite\_lattice`|
|`antifluorite`|Na₂O|Fm-3m (225)|A₂B|4|8|`fluorite\_lattice`|
|`perovskite`|CaTiO₃|Pm-3m (221)|ABX₃|12/6|6|`perovskite\_lattice`|
|`bcc`|W|Im-3m (229)|A|8|—|`bcc\_lattice`|
|`fcc`|Cu|Fm-3m (225)|A|12|—|`fcc\_lattice`|
|`hcp`|Mg|P6₃/mmc (194)|A|12|—|`hcp\_lattice`|
|`diamond`|C|Fd-3m (227)|A|4|—|`diamond\_lattice`|
|`zincblende`|ZnS|F-43m (216)|AB|4|4|`zincblende\_lattice`|
|`wurtzite`|ZnS|P6₃mc (186)|AB|4|4|`wurtzite\_lattice`|
|`rutile`|TiO₂|P4₂/mnm (136)|AB₂|6|3|`rutile\_lattice`|
|`anatase`|TiO₂|I4₁/amd (141)|AB₂|6|3|`anatase\_lattice`|
|`corundum`|Al₂O₃|R-3c (167)|A₂B₃|6|4|`corundum\_lattice`|
|`spinel`|MgAl₂O₄|Fd-3m (227)|AB₂X₄|4/6|4|`spinel\_lattice`|
|`graphite`|C|P6₃/mmc (194)|A|3|—|`graphite\_ab\_lattice`|
|`graphene`|C|P6/mmm (191)|A|3|—|`graphene\_lattice`|
|`quartz`|SiO₂|P3₁21 (152)|AB₂|4|2|`quartz\_lattice`|
|`cristobalite`|SiO₂|Fd-3m (227)|AB₂|4|2|`cristobalite\_lattice`|
|`zeolite`|SiO₂ framework|varies|AB₂|4|2|`cif\_import`|
|`mfi`|ZSM-5|Pnma (62)|AB₂|4|2|`cif\_import`|
|`nacl\_structure`|NaCl|Fm-3m (225)|AB|6|6|`rocksalt\_lattice`|
|`cscl\_structure`|CsCl|Pm-3m (221)|AB|8|8|`cscl\_lattice`|

Also resolves:

```
symmetry        = cubic / tetragonal / hexagonal / trigonal / orthorhombic
material\_class  → ionic\_crystal / metal / covalent / framework (suggested, not forced)
```

\---

## 25\. Registry: `material\_class` {#registry-material-class}

Set in `\[system]` or `\[material]` block.

```vsim
material\_class = "ionic\_crystal"
```

Resolves force model assumptions, charge handling, default analysis bundle, and known failure modes.

|Value|Force model|Charge handling|Default electrostatics|Default analysis|
|-|-|-|-|-|
|`ionic\_crystal`|Buckingham / LJ + Coulomb|formal charges|ewald|coordination, rdf, madelung|
|`metal`|EAM / LJ|neutral|none|rdf, coordination, diffusion|
|`ceramic`|Buckingham + Coulomb|formal|ewald|coordination, rdf, bond\_angle|
|`covalent\_solid`|Tersoff / SW|neutral|none|rdf, bond\_lengths, topology|
|`molecular\_crystal`|LJ + intramolecular|partial or neutral|ewald or none|rdf, packing, fingerprint|
|`polymer`|harmonic bond + LJ|partial|none|rmsd, rdf, packing|
|`powder`|granular contact|neutral|none|packing, coordination|
|`molten\_salt`|LJ + Coulomb|formal|ewald|rdf, diffusion, coordination|
|`oxide\_fuel`|Buckingham + Coulomb|formal|ewald|defects, diffusion, rdf|
|`framework\_material`|LJ / UFF|partial or neutral|ewald or none|pore\_topology, rdf, packing|
|`radiation\_damaged\_crystal`|LJ + Coulomb|formal|ewald|defects, vacancy\_migration, rdf|
|`noble\_gas\_solid`|LJ|neutral|none|rdf, coordination, cohesive\_energy|
|`layered\_material`|LJ + dispersion|neutral|none|interlayer\_spacing, rdf, anisotropy|

Also resolves:

```
\_\_is\_charged\_\_        = true / false
\_\_has\_dispersion\_\_    = true / false
tail\_correction       = true (for LJ-based classes)
charge\_neutrality     = true (for charged classes)
```

\---

## 26\. Registry: `chemistry` {#registry-chemistry}

Set in `\[chemistry]` (canonical) or `\[system]` (legacy alias) blocks.

```vsim
[chemistry]
chemistry              = "oxidation"
heat                   = -1         # auto-derive from environment.temperature
reaction_events        = true
track_species_state    = true
min_score_threshold    = 0.25
max_reactions_per_step = 8
```

Resolves reaction rules, species state changes, event logging behavior, and surface interaction defaults.
Chemistry evaluation is **ambient** — reactions are assessed after every simulation step whenever two or more
molecule species are present. The `[chemistry]` block controls rule selection and gating; it is **not** a mode
that must be explicitly activated.

|Value|Reaction type|Species changes|Event logging|Surface interaction|
|-|-|-|-|-|
|`oxidation`|O₂ uptake, oxide growth|O incorporation, metal → oxide|oxide\_layer\_events|surface\_residence|
|`hydration`|H₂O uptake|OH/H₂O incorporation|hydration\_events|surface\_residence|
|`corrosion`|dissolution + oxide|surface removal, oxide growth|corrosion\_events|surface\_dissolution|
|`pyrolysis`|thermal decomposition|bond breaking, fragment release|decomposition\_events|none|
|`reduction`|O removal|O loss, metal recovery|reduction\_events|surface\_residence|
|`hydrolysis`|water bond breaking|H/OH insertion|hydrolysis\_events|surface\_residence|
|`polymerization`|monomer chain growth|monomer → chain|polymerization\_events|none|
|`salt\_exchange`|ion swap|cation/anion substitution|exchange\_events|surface\_dissolution|
|`fluorination`|electrophilic F addition|F incorporation|fluorination\_events|surface\_residence|
|`isomer\_scan`|conformer / isomer search|bond-order reassignment|isomer\_events|none|
|`none`|no template family active|no changes|none|none|

**Heat gate** — the `heat` integer `h ∈ [0, 999]` controls activation energy thresholds:
- `heat = -1` (default): auto-derived from `environment.temperature` via `T/3000 × 999`
- `heat = 0`: cold, no thermally activated reactions
- `heat = 999`: extreme, all templates open

**Observe metrics** — these metric names in `[observe]` are evaluated from `KernelEventLog`:

| Metric | Meaning |
|---|---|
| `"reaction_events"` | Count of `Reaction` events |
| `"chemical_state"` | Count of `ChemicalState` events |
| `"exothermic_count"` | Count of events where `delta_E < 0` |
| `"avg_delta_E"` | Mean reaction energy (kcal/mol) |

Also resolves:

```
event\_registry     = true
reaction\_events    = true
track\_species\_state = true
```

\---

## 27\. Registry: `environment` {#registry-environment}

Set in `\[system]` or `\[environment]` block.

```vsim
environment = "vacuum"
```

Resolves background medium, pressure, thermal coupling, drag/damping, and transport assumptions.

|Value|Background|Pressure|Thermostat|Drag|Notes|
|-|-|-|-|-|-|
|`vacuum`|none|0.0|none|none|No external coupling|
|`air`|N₂/O₂ implicit|1.0 bar|implicit|light|Oxidizing, 300 K default|
|`water`|H₂O implicit|1.0 bar|langevin|medium|Dielectric screening active|
|`molten\_salt`|ionic melt|1.0 bar|langevin|heavy|High temperature default|
|`helium`|He implicit|1.0 bar|langevin|light|Inert, cryogenic capable|
|`argon`|Ar implicit|1.0 bar|langevin|light|Inert carrier gas|
|`high\_temperature`|none|1.0 bar|none|none|T > 1000 K default|
|`radiation\_field`|none|1.0 bar|none|none|Enables radiation events|
|`pressure\_gradient`|none|gradient|none|none|Requires pressure\_lo / pressure\_hi|

Also resolves:

```
temperature    (if not set: vacuum→300K, molten\_salt→900K, high\_temperature→1500K)
thermostat     (if not set: vacuum/radiation→none, water/He/Ar→langevin)
damping        (environment-specific)
dielectric     (water→78.4, vacuum→1.0, others→1.0)
```

\---

## 28\. Registry: `run\_mode` {#registry-run-mode}

Set in `\[run]` block as `mode`.

```vsim
mode = "relax"
```

Resolves solver, step count defaults, convergence gate, default exports, and analysis pipeline.

|Value|Solver|Default steps|Convergence gate|Default analysis|Default exports|
|-|-|-|-|-|-|
|`relax`|FIRE|2000|force\_tolerance|energy\_trend, rmsd|xyz, analysis\_json|
|`formation`|FIRE|5000|force\_tolerance|energy\_trend, rmsd, rdf|xyz, xyzf, analysis\_json|
|`diffusion`|velocity\_verlet|50000|msd\_convergence|diffusion, msd, rdf|xyzf, analysis\_json|
|`packing`|FIRE|3000|packing\_fraction|packing, rdf, coordination|xyz, analysis\_json|
|`surface\_contact`|FIRE|5000|force\_tolerance|surface\_residence, rdf|xyz, xyzf, analysis\_json|
|`thermal\_ramp`|langevin|20000|temperature\_reached|energy\_trend, rdf|xyzf, analysis\_json|
|`field\_response`|velocity\_verlet|10000|field\_convergence|energy\_trend, rmsd|xyzf, analysis\_json|
|`radiation\_damage`|velocity\_verlet|10000|damage\_saturation|defects, vacancy\_migration, rdf|xyzf, events\_json|
|`reaction\_scan`|reaction\_event|10000|reaction\_complete|reaction\_events, rdf|events\_json, analysis\_json|
|`static`|none|0|none|energy\_trend|xyz, analysis\_json|

Also resolves:

```
save\_every      (relax→100, diffusion→50, radiation\_damage→10)
dt              (relax→0.001, diffusion→0.001, thermal\_ramp→0.002)
```

\---

## 29\. Registry: `solver` {#registry-solver}

Set in `\[run]` block. Usually resolved from `mode`; override only when needed.

```vsim
solver = "FIRE"
```

|Value|Integrator|Timestep default|Stability guard|Allowed with|
|-|-|-|-|-|
|`FIRE`|FIRE|0.001 ps|force\_max check|relax, formation, packing|
|`velocity\_verlet`|Velocity Verlet|0.001 ps|energy drift check|diffusion, field\_response, radiation\_damage|
|`langevin`|Langevin|0.002 ps|temperature check|thermal\_ramp, NVT runs|
|`bead\_dynamics`|overdamped|0.005 ps|displacement check|polymer, soft matter|
|`monte\_carlo`|MC moves|N/A (moves not steps)|acceptance rate check|packing, reaction\_scan|
|`reaction\_event`|event-driven|variable|event rate check|reaction\_scan|
|`hybrid\_md\_dem`|MD + DEM|0.001 ps|overlap check|powder, granular|

\---

## 30\. Registry: `forcefield` {#registry-forcefield}

Set in `\[system]` or `\[forcefield]` block.

```vsim
forcefield = "Lennard\_Jones"
```

|Value|Energy terms|Parameter source|Charge handling|Cutoff default|Gradient|
|-|-|-|-|-|-|
|`UFF`|bond + angle + LJ|UFF 1992 table|partial (Gasteiger)|12.0 Å|yes|
|`UFX`|UFF extended|UFX table|partial|12.0 Å|yes|
|`Lennard\_Jones`|LJ 12-6|per-species ε/σ|none|2.5σ|yes|
|`Buckingham`|exp-6 + Coulomb|per-pair A/B/C|formal|10.0 Å|yes|
|`Coulomb\_Ewald`|Ewald electrostatics|species charges|formal or partial|ewald\_rcut|yes|
|`harmonic\_bond`|bond + angle|per-bond k/r₀|none|N/A|yes|
|`granular\_contact`|Hertz contact|per-species E/ν|none|particle radius|yes|
|`reactive\_placeholder`|stub|none|none|none|no|
|`dispersion\_D3`|D3(BJ) dispersion|D3 parameter table|none|12.0 Å|yes|

Also resolves:

```
tail\_correction    = true   (LJ-based forcefields)
ewald\_alpha        = 0.3    (Coulomb\_Ewald, Buckingham)
ewald\_kmax         = 8      (Coulomb\_Ewald, Buckingham)
minimum\_image      = true   (all periodic forcefields)
```

\---

## 31\. Registry: `observables` {#registry-observables}

Set in `\[analysis]` block as a list.

```vsim
observables = \["energy", "rdf", "diffusion"]
```

Each observable activates a named bundle of tracked fields, analysis functions, and required trajectory format.

|Value|Tracked fields|Analysis functions|Required trajectory|
|-|-|-|-|
|`energy`|E\_total, E\_pot, E\_kin|energy\_trend, drift\_check|none|
|`rmsd`|per-atom displacement|rmsd vs reference|xyz initial|
|`rdf`|pair distances|rdf all pairs|none|
|`diffusion`|unwrapped positions|msd, diffusion\_coefficient|xyzf + image counts|
|`packing`|volume, overlap|packing\_fraction, void\_fraction|xyz|
|`defects`|vacancy/interstitial count|defect\_fraction, formation\_energy|xyz reference|
|`surface`|surface atom flags|surface\_residence, dissolution\_rate|xyzf|
|`coordination`|per-atom CN|coordination\_number, CN\_distribution|none|
|`spectral\_response`|velocity autocorrelation|vibrational\_spectrum|xyzf velocities|
|`radiation\_damage`|PKA events, displaced atoms|damage\_cascade, dpa\_count|xyzf + events|
|`topology`|bond graph, ring counts|ring\_statistics, network\_analysis|xyz|
|`fingerprint`|structure fingerprint vector|polymorph\_distance, similarity|xyz|
|`madelung`|electrostatic energy|madelung\_constant, madelung\_energy|none|
|`thermodynamics`|T, P, H, G estimates|equation\_of\_state|xyzf|

\---

## 32\. Registry: `export\_profile` {#registry-export-profile}

Set in `\[output]` block as `profile`.

```vsim
profile = "research\_report"
```

Resolves the full set of output files activated.

|Value|xyz|xyzf|xyzfull|analysis\_json|metrics\_tsv|report\_md|events\_json|manifest|dashboard\_svg|
|-|-|-|-|-|-|-|-|-|-|
|`minimal`|✓|||||||||
|`debug`|✓|✓|✓|✓|✓||✓|||
|`research\_report`|✓|✓||✓|✓|✓||||
|`publication`|✓|✓||✓|✓|✓||✓||
|`dashboard`|✓|||✓|✓||||✓|
|`golden\_test`|✓|||✓|✓|✓||✓||
|`teaching\_demo`|✓|✓||✓||✓|||✓|

Also resolves:

```
output\_dir     = output/<script\_name>/   (all profiles)
write\_report\_md = true                   (research\_report, publication, golden\_test, teaching\_demo)
```

Individual `write\_\*` fields override profile settings. Profile is the floor, not the ceiling.

\---

## 33\. Registry: `geometry\_source` {#registry-geometry-source}

Set in `\[system]` block.

```vsim
geometry\_source = "xyz"
```

Resolves the loader, coordinate truth source, and import validation behavior.

|Value|Loader|Coordinate truth|Validates|Notes|
|-|-|-|-|-|
|`generated`|internal generator|generator output|species, cell|Default when `structure =` is set|
|`xyz`|xyz file reader|`.xyz` file|atom count, species|Uses `source =` path|
|`xyzfull`|xyzfull reader|`.xyzfull` file|full state|Restores complete simulation state|
|`step`|STEP/AP214 reader|CAD geometry|mesh validity|Confirm: STEP reader status|
|`stl`|STL mesh reader|mesh surface|watertight|Confirm: STL reader status|
|`script`|inline coordinates|`\[structure.fractional\_coords]` block|none|Manual Wyckoff input|
|`database`|internal database|known prototype|space group|Uses `structure =` as key|

\---

## 34\. Registry: `radiation` {#registry-radiation}

Set in `\[system]` or `\[radiation]` block.

```vsim
radiation = "neutron\_flux"
```

Resolves event source, energy deposition model, damage model, daughter species, and radiation observables.

|Value|Event source|Energy deposition|Damage model|Daughter species|Observables|
|-|-|-|-|-|-|
|`none`|none|none|none|none|none|
|`alpha\_decay`|PKA from He nucleus|nuclear stopping|displacement cascade|He + daughter nucleus|defects, radiation\_damage|
|`beta\_decay`|electron emission|electronic stopping|ionization track|electron + neutrino (implicit)|defects|
|`gamma\_field`|photon field|electronic stopping|radiolysis products|species-dependent|defects, spectral\_response|
|`neutron\_flux`|PKA from n capture|nuclear stopping|displacement cascade|daughter isotope|defects, radiation\_damage, dpa|
|`fission\_fragment`|heavy ion PKA|nuclear + electronic|large cascade|two fission products|defects, radiation\_damage, dpa|
|`radiolysis`|field-driven bond breaking|electronic|molecular fragmentation|OH, H, e⁻aq (implicit)|defects, surface|

Also resolves:

```
event\_registry          = true
track\_displaced\_atoms   = true
damage\_threshold        = 25.0 eV    (default displacement energy)
observables            += radiation\_damage, defects
```

\---

## 35\. Changelog {#changelog}

|Version|Change|
|-|-|
|v5.0.0-beta.8|Initial language reference. Covers \[meta], \[system], \[cell], \[boundary], \[pbc], \[run], \[analysis], \[visual], \[output]. PBC built-in functions stable.|
|v5.0.0-beta.8|Added registry system: structure, material\_class, chemistry, environment, run\_mode, solver, forcefield, observables, export\_profile, geometry\_source, radiation. Registry rule: explicit user value always wins.|
|v5.0.0-beta.9|Added \[material] block (§6) — intent-based authoring Layer 0–1. Schema and parser complete (WO-VSIM-03B). Registry resolution engine pending (WO-VSIM-03C).|
|v5.0.0-beta.9|Added \[environment] block (§12) — Level 2 physical environment override. Schema and parser complete.|
|v5.0.0-beta.9|Added \[excite.*] block (§13) — named excitation subsection. Schema and parser complete.|
|v5.0.0-beta.9|Added \[observe] block (§14) — in-run metric collection. Schema and parser complete.|
|v5.0.0-beta.9|Added \[\[override.particle\]\] block (§18) — Level 3 selective particle mutation. Schema and parser complete.|
|v5.0.0-beta.9|Added \[\[raw.object\]\] block (§19) — Level 4 explicit particle injection (debug/import only). Schema and parser complete.|
|v5.0.0-beta.9|Expanded Registry: structure (§24) alias map to ~70 canonical entries across 8 groups: ionic/salts, elemental metals, covalent/semiconductor, oxides/ceramics, molecular geometry, polymers/organics, porous/framework, bead/premacro.|
|v5.0.0-beta.9|Beta-8 declared closed. Beta-9 theme: Registry Resolution and Minimal Lab Script Layer (WO-VSEPR-SIM-58A).|

> \*\*Reminder:\*\* When you change a block, field, default, or built-in function in the VSIM runtime or parser, add a row to this changelog and update the affected section. If the change is breaking, mark the field status as `changed` and add a migration note.


