# WO-66P — Universal Translation + Crystal/PBC vsim Module Compression
<!-- v5.1.4 | branch: v5.0.0-main -->

## Summary

Compresses the existing [pbc] / [cell] flat-key schema into a single
CrystalModule(...) functional constructor that can be declared in
[objects].  Adds universal translation targets (xyz, CELL comment block,
CIF summary, JSON manifest).

## Motivation

Legacy [cell] and [pbc] sections spread crystal parameters across two
sections with overlapping keys.  A single constructor call is cleaner,
compositional, and enables programmatic generation of crystal variants.

## Syntax

### Functional form (preferred)

`sim
[objects]
system.crystal = CrystalModule(
    lattice   = fcc,
    a         = 3.52,
    species   = [Ni],
    supercell = [4, 4, 4],
    relax     = true
)
`

### Legacy compatibility (still supported)

`sim
[cell]
lattice = fcc
a       = 3.52
nx      = 4
ny      = 4
nz      = 4
species = Ni

[pbc]
pbc_x = true
pbc_y = true
pbc_z = true
`

Both forms populate VsimDocument::crystal (a CrystalConstructorSection).

## Translation targets

| Field | Output |
|---|---|
| 	ranslate_to_xyz = true | CELL comment in xyz header |
| 	ranslate_to_cif_summary = true | CIF summary block |
| 	ranslate_to_json = true | JSON crystal manifest |

## Implementation

| File | Change |
|---|---|
| include/vsim/crystal/crystal_constructor.hpp | LatticeType, CrystalConstructorSection |
| include/vsim/vsim_document.hpp | VsimDocument::crystal |
| src/vsim/vsim_parser.cpp | pply_crystal_constructor_key (handles objects.crystal, crystal, cell, pbc) |

## Acceptance criteria

- [x] CrystalConstructorSection covers all Bravais types, supercell, PBC, relax, defects, disorder
- [x] Legacy [cell] / [pbc] keys route to pply_crystal_constructor_key
- [x] Translation-target flags present and parseable
- [x] Build clean

## Diagnostic codes

VSIM-C010 — lattice type unknown  
VSIM-C011 — species list empty  
VSIM-C012 — lattice parameters inconsistent for declared type  
VSIM-C013 — custom lattice vectors supplied but not all three provided  