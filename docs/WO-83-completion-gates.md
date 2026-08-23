# WO-83 Completion Gates

**Branch:** `feature/wizard-full-module-expansion`
**Area:** `atomistic/classify/`
**Scope:** WO-83A through WO-83C

---

## Gate 83A - Activation Audit Complete

```text
[x] atomistic/classify/fingerprints.cpp is confirmed active in build
[x] atomistic/classify/cluster.cpp is confirmed active in build
[x] atomistic/classify/vsepr.cpp build status is documented
[x] atomistic/classify/organic_candidate.cpp build status is documented
[x] organic_candidate.hpp prior status is documented
[x] organic_candidate.cpp prior status is documented
[x] vsepr.hpp creation status is documented
[x] vsepr.cpp creation status is documented
[x] duplicate classifier risks are identified
[x] ownership table is written
[x] module responsibility boundaries are documented
[x] local geometry ownership is assigned to VSEPR rather than organic classification
[x] docs/WO-83-classify-audit.md exists
[x] build wiring findings are recorded in the audit document
[x] classify ownership table is recorded in the audit document
[x] carry-forward risks are listed for later WO parts
```

Exit condition:

```text
The classify module has a documented ownership map, active build wiring is known,
and no new classifier is added without checking whether the behavior already
exists elsewhere.
```

---

## Gate 83B - VSEPR Module Active

```text
[x] atomistic/classify/vsepr.hpp exists
[x] atomistic/classify/vsepr.cpp exists
[x] tests/test_classify_vsepr.cpp exists
[x] VSEPR files are added to the atomistic build target
[x] classify_vsepr_sites() compiles
[x] VSEPRReport is produced from atomistic::State
[x] VSEPRSite stores center index and atomic number
[x] VSEPRSite stores bonded-domain count
[x] VSEPRSite stores lone-pair-domain count
[x] VSEPRSite stores electron-domain count
[x] VSEPRSite stores AXmEn label
[x] VSEPRSite stores molecular shape
[x] VSEPRSite stores electron-domain geometry
[x] VSEPRSite stores angle statistics
[x] VSEPRSite stores confidence score
[x] bonded-domain geometry works without lone-pair data
[x] geometry-only fallback works when lone-pair inference is unavailable
[x] electron-domain geometry and molecular geometry are stored separately
[x] local pairwise bond angles are computed
[x] mean / min / max / RMS angle statistics are computed
[x] linear-like geometry can be identified
[x] planar-like geometry can be identified
[x] tetrahedral-like geometry can be identified
[x] hypervalent-like coordination can be flagged
[x] output order is deterministic
[x] no hardcoded element arrays are introduced
[x] CO2  -> AX2 linear
[x] BF3  -> AX3 trigonal planar
[x] CH4  -> AX4 tetrahedral
[x] NH3  -> fallback local geometry case
[x] H2O  -> fallback local geometry case
[x] PCl5 -> AX5 trigonal bipyramidal
[x] SF6  -> AX6 octahedral
```

Exit condition:

```text
VSEPR/local geometry has a dedicated owner and is no longer at risk of being
duplicated inside organic classification.
```

---

## Gate 83C - OrganicCandidate Bridge Active

```text
[x] atomistic/classify/organic_candidate.hpp exists
[x] atomistic/classify/organic_candidate.cpp exists
[x] organic_candidate.cpp is added to the build target
[x] organic_candidate.hpp references VSEPR data cleanly
[x] OrganicCandidate consumes VSEPRReport
[x] OrganicCandidate stores VSEPRSite data
[x] OrganicCandidate stores linear site count
[x] OrganicCandidate stores planar site count
[x] OrganicCandidate stores tetrahedral site count
[x] OrganicCandidate derives geometry counts from VSEPR output
[x] OrganicCandidate does not reimplement local geometry classification
[x] local_geometry_strain_score exists
[x] local_geometry_strain_score receives VSEPR angle RMS deviation input
[x] strain score is normalized to a 0.0-1.0 range
[x] empty VSEPR reports return safe default strain values
[x] strained local geometries increase the score
[x] lipid_like_score exists
[x] lipid-like behavior is inferred from organic descriptors
[x] lipid-like behavior is not added as a flat enum label
[x] flexible-chain evidence contributes to lipid-like score
[x] ester or carboxylic-acid evidence contributes to lipid-like score
[x] polarity balance contributes to lipid-like score
[x] excessive heteroatom fraction can penalize lipid-like score
[x] OrganicCandidate owns organic descriptor aggregation
[x] OrganicCandidate owns chemistry-family inference
[x] VSEPR owns local geometry and AXmEn labels
[x] fingerprints owns topology signatures
[x] cluster owns deterministic grouping
[x] no added duplicate classifier ownership conflicts remain
```

Exit condition:

```text
OrganicCandidate uses local geometry as input, but does not own or reimplement
local geometry classification.
```

---

## Final Ownership State

```text
fingerprints.hpp / fingerprints.cpp
    Own topology fingerprints, graph signatures, RDF/CN descriptors.

cluster.hpp / cluster.cpp
    Own deterministic threshold grouping and cluster IDs.

vsepr.hpp / vsepr.cpp
    Own local geometry, AXmEn labels, electron-domain geometry,
    molecular geometry, angle stats, and VSEPR site reports.

organic_candidate.hpp / organic_candidate.cpp
    Own organic descriptor aggregation, organic-family inference,
    VSEPR bridge, strain scoring, and lipid-like inferred behavior.
```

---

## Final Acceptance Gate

```text
[x] activation audit exists
[x] build wiring is documented
[x] classify ownership table exists
[x] duplicate classifier risks are documented
[x] VSEPR module is active
[x] VSEPR tests pass
[x] OrganicCandidate bridge is active
[x] strain score receives VSEPR geometry input
[x] lipid-like score is inferred
[x] no added duplicate classifier ownership conflicts remain
[x] docs reflect the new module boundaries
[x] real VSIM acceptance scripts exist in examples/classify_acceptance
[x] source `vsepr` CLI target builds
[ ] source `vsepr` CLI accepts `.vsim` validate/run/audit flow
[ ] VSIM runtime accepts analysis.classify requests
[ ] VSIM runtime emits detailed vsepr_sites output
[ ] VSIM runtime emits detailed organic_candidate output when requested
[ ] unsupported classifier metrics warn instead of being silently dropped
```

Source-level exit condition:

```text
The source module has separate owners for topology, clustering, local geometry,
and organic descriptor inference, with VSEPR feeding organic classification
rather than being buried inside it.
```

Runtime progression condition:

```text
Schedule progression is blocked until real VSIM scripts can prove classifier
behavior through detailed runtime output. If output detail is missing, the
report/export layer must be fixed before continuing.

Current check:

The built `vsepr` CLI exposes `vsepr <SPEC> <ACTION>`. It does not yet accept
the documented `.vsim` validate/run/audit flow; passing a `.vsim` script path
falls into formula parsing before classifier output can be tested.
```

---

## Carry-Forward Gates

These are not blockers for WO-83A through WO-83C.

```text
[x] neutral element-based lone-pair inference for current State data
[ ] formal charge support
[ ] bond-order-aware VSEPR
[ ] species / valence-state metadata support
[ ] ring detection owner selected
[ ] aromaticity detection owner selected
[ ] rotatable bond logic implemented beyond placeholder level
[ ] lipid subtype inference: fatty acid / triglyceride / phospholipid / sterol
[ ] report export for VSEPR and OrganicCandidate summaries
[ ] VSEPR report formatter
[ ] OrganicCandidate report formatter
[ ] regression script for geometry-only fallback
[ ] bug reproduction script for any failed geometry assignment
```

WO-83D documents the completed neutral element-based subset in
`docs/WO-83D-lone-pair-inference.md`. Formal charge, bond order, and richer
species metadata remain carry-forward work.

---

## Standing Acceptance Artifact Rule

From Part D through Part W, every part must end with at least one real `.vsim`
acceptance script. C++ tests and Python demos are developer smoke/regression
checks only.

Current Part D through Part W content is scaffold-only. Do not mark any future
part complete until its source changes, VSIM acceptance script, documentation,
and verification output exist. See `docs/WO-83-continuation-scaffold.md`.

Acceptable forms:

```text
examples/classify_acceptance/lone_pair_nh3.vsim
examples/classify_acceptance/lone_pair_h2o.vsim
tests/test_classify_lone_pair_inference.cpp
scripts/demo_classify_scaffold.py
```

Minimum behavior:

```text
[ ] run the relevant VSIM scenario
[ ] request classifier outputs explicitly
[ ] write detailed analysis/report artifacts
[ ] prove pass/fail status from runtime output
[ ] document what the script proves and what output fields are required
```
