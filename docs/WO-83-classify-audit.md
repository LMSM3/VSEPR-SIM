# WO-83 Classify Activation Audit

**Branch:** `feature/wizard-full-module-expansion`
**Area:** `atomistic/classify/`
**Scope:** WO-83A through WO-83C classify module coding push.

---

## Build Wiring Findings

| Unit | Build status | Notes |
|---|---|---|
| `atomistic/classify/fingerprints.cpp` | Active | Compiled directly into the `atomistic` static library by `atomistic/CMakeLists.txt`. |
| `atomistic/classify/cluster.cpp` | Active | Compiled directly into the `atomistic` static library by `atomistic/CMakeLists.txt`. |
| `atomistic/classify/organic_candidate.hpp` | Created in WO-83C | No prior file was found in the source tree before this push. |
| `atomistic/classify/organic_candidate.cpp` | Created in WO-83C | Bridge logic is now a translation unit in the `atomistic` target. |
| `atomistic/classify/vsepr.hpp` | Created in WO-83B | Dedicated local VSEPR geometry API. |
| `atomistic/classify/vsepr.cpp` | Created in WO-83B | Dedicated local shape implementation; not owned by organic classification. |

---

## Ownership Table

| Property | Existing owner | Active? | Tested? | Extend or create? |
|---|---|---|---|---|
| local geometry | No dedicated owner before WO-83B | Yes, via `classify/vsepr.*` | Yes, `test_classify_vsepr` | Create dedicated VSEPR module |
| VSEPR AXmEn | No dedicated owner before WO-83B | Yes, via `VSEPRSite::ax_label` | Yes, CO2/BF3/CH4/NH3/H2O/PCl5/SF6 cases | Create in VSEPR module |
| sp/sp2/sp3 | No dedicated owner before WO-83C | Partial, derived from VSEPR site flags in `OrganicCandidate` | Yes, bridge smoke test | Derive from VSEPR output |
| rings | No active classify owner found | No | No | Create later only if topology/fingerprint owner is reconciled |
| aromaticity | No active classify owner found | No | No | Create later as organic/topology bridge, not VSEPR |
| rotatable bonds | No active classify owner found | Partial placeholder in `OrganicCandidate` | Compile/bridge only | Extend organic descriptor logic later |
| lipid-like score | No active classify owner found | Partial inferred score in `OrganicCandidate` | Compile/bridge only | Infer from descriptors, do not add a dumb family enum |
| strain score | No active classify owner found | Yes, VSEPR angle RMS feeds organic candidate score | Yes, bridge smoke test | Create bridge from VSEPR report |

---

## Duplicate Classifier Risk

- VSEPR owns local AXmEn and local molecular/electron geometry.
- OrganicCandidate owns organic descriptor aggregation and chemistry-family inference.
- Fingerprints own topology signatures and graph-level structural comparison.
- Cluster owns grouping and deterministic threshold assignment.
- Lipid-like behavior is inferred from existing organic descriptors and is not a standalone family label.
- No organic code should reimplement local geometry if `classify_vsepr_sites()` can provide it.

---

## WO-83 Completion Snapshot

```text
[x] classify audit exists
[x] vsepr.hpp added
[x] vsepr.cpp added
[x] test_classify_vsepr.cpp added
[x] build files updated
[x] VSEPR report compiles
[x] geometry-only classification works
[x] organic candidate bridge compiles
[x] strain score receives VSEPR angle deviation input
[x] lipid-like score is inferred from existing organic descriptors
[x] no duplicate classifier ownership conflicts remain in the added code
```

See `docs/WO-83-completion-gates.md` for the gate-by-gate acceptance record.

---

## Carry-Forward Gates

These are not blockers for WO-83A through WO-83C, but should become future work.

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

WO-83D implements the neutral element-based subset and is documented in
`docs/WO-83D-lone-pair-inference.md`.

---

## Standing Acceptance Artifact Rule

From Part D through Part W, every part must end with at least one real `.vsim`
acceptance script. C++ tests and Python demos are developer smoke/regression
artifacts only.

Part D through Part W are scaffold-only for now. The continuation scaffold lives
in `docs/WO-83-continuation-scaffold.md`. The current acceptance scripts live in
the VSIM documentation workspace under `examples/classify_acceptance/`, with a
lightweight developer demo template at `scripts/demo_classify_scaffold.py`.

Minimum behavior:

```text
[ ] run the relevant VSIM scenario
[ ] request classifier outputs explicitly
[ ] write detailed analysis/report artifacts
[ ] prove pass/fail status from runtime output
[ ] document what the script proves and what output fields are required
```
