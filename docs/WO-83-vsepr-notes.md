# WO-83 VSEPR Notes

**File:** `docs/WO-83-vsepr-notes.md`
**Branch:** `feature/wizard-full-module-expansion`
**Status:** Active (WO-83B complete, D–U partial)

---

## What was added

### `atomistic/classify/vsepr.hpp`
- `VSEPRElectronGeometry` — linear through octahedral + expanded coordination
- `VSEPRMolecularShape` — atom, linear, bent, trigonal planar/pyramidal, tetrahedral, see-saw, T-shaped, trigonal bipyramidal, square planar/pyramidal, octahedral, irregular, expanded coordination
- `VSEPRAngleStats` — min/max/mean angle in degrees + RMS deviation
- `VSEPRSite` — per-atom site record: AX label, shapes, geometry, lone-pair count, hybridization flags, confidence
- `VSEPRReport` — full site list + summary counts (linear, planar, tetrahedral, bent, pyramidal, hypervalent)
- `VSEPROptions` — neighbor cutoff, tolerance settings, fallback flags
- `HybridizationHint` — sp, sp2, sp3, sp3d, sp3d2 (WO-83I, geometry-inferred)
- `format_vsepr_report()` — plain-text formatter, deterministic (WO-83D)
- `hybridization_hint()` — geometry-derived hint from VSEPRSite (WO-83I)

### `atomistic/classify/vsepr.cpp`
- `classify_vsepr_sites()` — per-atom local geometry classification
- `infer_lone_pairs_from_element()` — element-based lone-pair count for H, B, C, N, O, F, Cl, Br, I, P, S
- `infer_lone_pairs_from_geometry()` — geometry fallback when element not in table
- Fallback confidence penalty: 25% reduction when geometry fallback used (WO-83E)
- `angle_deg()` — uses `norm()`/`dot()` from `core/math_vec3.hpp`, safe for zero vectors
- `confidence_from_rms()` — 1.0 - rms/45.0, clamped

### `atomistic/classify/providers.hpp`
- `BondOrderProvider` (WO-83F) — functional callback, per-bond, nullable
- `LonePairProvider` (WO-83G) — functional callback, per-atom, nullable
- `FormalChargeProvider` (WO-83H) — functional callback, per-atom, nullable
- `RingProvider` (WO-83J) — per-atom ring sizes + total count, nullable
- `ProviderSet` — aggregate of all four

---

## Ownership rules

| Concept | Owner |
|---|---|
| Local bond angles | VSEPR (`vsepr.cpp`) |
| AX-mEn labels | VSEPR |
| Electron domain geometry | VSEPR |
| Molecular shape | VSEPR |
| Hybridization hint | VSEPR (geometry-derived) |
| Lone-pair inference | VSEPR (element table + geometry fallback) |
| Ring detection | NOT VSEPR — deferred to WO-84 RingProvider |
| Aromaticity | NOT VSEPR — deferred to WO-84 |
| Bond order | NOT VSEPR — via BondOrderProvider (WO-83F) |
| Formal charge | NOT VSEPR — via FormalChargeProvider (WO-83H) |

---

## Known limitations

1. **Xe, Kr, Rn lone pairs** — `infer_lone_pairs_from_element()` has no case for noble gas hypervalent species (XeF2, XeF4, XeF6). These fall through to geometry-only fallback, which typically misses lone-pair domains. **Fix target: WO-84 LonePairProvider.**

2. **Transition metals** — No d-orbital lone-pair inference. Classification falls through to fallback. AX labels are geometric only.

3. **Bond-order-dependent shapes** — Square planar vs. tetrahedral distinction for d8 metals requires bond order context. **Fix target: WO-84 BondOrderProvider.**

4. **Lone-pair fallback confidence** — The 25% penalty (WO-83E) is conservative. Actual confidence depends on quality of positions and neighbor detection. Long-term: weight by position quality metric.

---

## Future work (WO-84+)

- Wire `LonePairProvider` into `classify_vsepr_sites()` options
- Wire `BondOrderProvider` into shape disambiguation
- Wire `FormalChargeProvider` into neutral vs. zwitterionic site handling
- Expose `VSEPROptions::providers` field of type `ProviderSet`
- Add JSON-lite export path for VSEPR report (WO-83P stabilization)
