# WO-66O — Organic/Peptide Scale Diagnostics
<!-- v5.1.4 | branch: v5.0.0-main -->

## Summary

Adds a [diagnostics.organic] section providing scale-aware diagnostics
for simulations whose domain is peptide or small_molecule.

## Motivation

Peptide and small-molecule runs lack structural validation.  Without
backbone planarity checks, Ramachandran audits, H-bond detection, and
chirality monitoring, silent stereocentre inversions and unphysical
backbone geometries go unreported.

## Syntax

`sim
[diagnostics.organic]
enabled              = true
run_on_peptide       = true
sample_every_n_steps = 100
violation_severity   = warn

peptide.check_phi_psi   = true
peptide.check_chirality = true
peptide.check_hbonds    = true
peptide.compute_rg      = true

small_molecule.check_bond_lengths   = true
small_molecule.check_aromatic_planarity = true
`

## Implementation

| File | Change |
|---|---|
| include/vsim/diagnostics/organic_diagnostics.hpp | PeptideChainDiagnostics, SmallMoleculeDiagnostics, OrganicScaleSection, OrganicDiagnosticResult |
| include/vsim/vsim_document.hpp | VsimDocument::organic_diagnostics |
| src/vsim/vsim_parser.cpp | pply_diagnostics_organic_key |

## Diagnostic codes

VSIM-D010 — Ramachandran outlier  
VSIM-D011 — omega planarity violation  
VSIM-D012 — chirality inversion (L→D)  
VSIM-D013 — H-bond distance/angle violation  
VSIM-D014 — aromatic ring non-planarity  
VSIM-D015 — formal charge inconsistency  
VSIM-D019 — section enabled but domain is neither peptide nor small_molecule  