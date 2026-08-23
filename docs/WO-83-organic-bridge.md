# WO-83 OrganicCandidate Bridge Notes

**File:** `docs/WO-83-organic-bridge.md`
**Branch:** `feature/wizard-full-module-expansion`
**Status:** Active (WO-83C complete, N/M/O improvements in this session)

---

## Design rule

> OrganicCandidate uses VSEPR as input but does NOT own local geometry.

All sp/sp2/sp3 counts come from `classify_vsepr_sites()` → VSEPRReport.
OrganicCandidate never calls angle math directly.

---

## What was added

### `atomistic/classify/organic_candidate.hpp`
- `FamilySource` enum: `Inferred | Manual | Default` (WO-83M)
- `family_source` field on `OrganicCandidate` struct
- `lipid_like_score` field (WO-83N): composite 0–1 descriptor
- `compute_lipid_like_score()` method declaration
- `format_organic_candidate()` free function declaration (WO-83Q)

### `atomistic/classify/organic_candidate.cpp`
- `to_string(FamilySource)` — deterministic name for source tag
- `classify()` sets `family_source = Inferred` when families were detected, `Default` when empty
- `compute_lipid_like_score()` (WO-83N): five evidence components:
  1. High C-fraction among heavy atoms (hydrocarbon chain evidence)
  2. High rotatable-bond density (flexible chain evidence)
  3. Small O count, no N (ester/carboxylic evidence)
  4. Moderate heteroatom fraction 2–20% (polarity balance)
  5. Excessive heteroatom penalty (>35%) + halogen penalty
- `format_organic_candidate()` (WO-83Q): plain-text formatter for report layer
- `strain_score` normalization: divided by 45°×site_count (unchanged from WO-83C)

---

## Ownership rules

| Concept | Owner |
|---|---|
| sp/sp2/sp3 counts | OrganicCandidate (via VSEPR) |
| Strain score | OrganicCandidate (via VSEPR rms_deviation) |
| Lipid-like score | OrganicCandidate (composite descriptor) |
| Ring count | Placeholder (0) — deferred to RingProvider (WO-84) |
| Aromatic ring count | Placeholder (0) — deferred to bond-order + ring detection |
| Rotatable bond count | OrganicCandidate (graph-based, conservative when no bond-order) |
| Family source marker | OrganicCandidate (FamilySource enum) |
| Partial charge / polarity | OrganicCandidate (from State.Q) |
| Donor/acceptor map | OrganicCandidate (heuristic from element + adjacency) |

---

## Known limitations

1. **Ring count = 0** — SSSR not implemented. Ring detection is deferred to WO-84 RingProvider interface. No false ring counts emitted.

2. **Aromatic detection = 0** — Requires ring membership + alternating bond-order data. Both deferred to WO-84. OrganicFamily::AROMATIC is never assigned without explicit ring+bond evidence.

3. **Rotatable bonds** — Current implementation counts single bonds to non-terminal non-H atoms in the bond graph. Ring exclusion requires RingProvider. Conservative: over-counts in ring molecules.

4. **Lipid-like score** — Based on formula-level evidence (C-fraction, heteroatom count). Does not use 3D conformation. Long flexible chain in 3D is not distinguished from compact formula.

5. **Polarity score** — Requires non-zero State.Q charges. If charges are absent (Q = 0 for all atoms), polarity = 0.0 regardless of true dipole moment.

---

## FamilySource usage

```
c.family_source = FamilySource::Default  // no families identified
c.family_source = FamilySource::Inferred // derived from descriptors
c.family_source = FamilySource::Manual   // set by caller, not yet exposed
```

The `Manual` path is reserved for future caller-override via script metadata
(e.g. `[material] family = "alkane"` explicit override in a .vsim file).

---

## Future work (WO-84+)

- Wire `RingProvider` into `count_rings()` to replace placeholder
- Wire `BondOrderProvider` into rotatable bond ring exclusion
- Implement aromaticity check: ring of size 4n+2 with alternating double bonds
- Add lipid subtype: phospholipid, glycolipid, sterol (requires ring + functional group data)
- Wire `FormalChargeProvider` into polarity and reactive site computation
- Expose `OrganicClassifier::classify_with_providers(state, ProviderSet)` overload
