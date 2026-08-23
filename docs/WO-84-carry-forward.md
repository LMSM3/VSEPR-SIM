# WO-84 Carry-Forward

**File:** `docs/WO-84-carry-forward.md`
**Branch:** `feature/wizard-full-module-expansion`
**Prepared by:** WO-83Z

---

## Theme: Bond Order, Lone Pairs, Ring Ownership, and 3D Chemistry

WO-84 picks up exactly where WO-83 left the scaffolding.
The providers are defined. The stubs are in place.
WO-84 implements the chemistry behind them.

---

## Carry-forward items (from WO-83Z checklist)

| Item | File to touch | Notes |
|---|---|---|
| Real lone-pair inference from element/species/bond data | `vsepr.cpp`, `providers.hpp` | Wire `LonePairProvider` into `classify_vsepr_sites()` via `VSEPROptions::providers` |
| Formal charge support | `organic_candidate.cpp`, `providers.hpp` | `FormalChargeProvider` affects reactive site and polarity computation |
| Bond-order-aware VSEPR | `vsepr.cpp` | Square planar vs. tetrahedral disambiguation for d8 metals; aromatic bond detection |
| Ring detection owner | `atomistic/classify/ring_detector.hpp` (new) | Implement SSSR or delegating bridge; feed to `RingProvider` |
| Aromaticity detection | `atomistic/classify/organic_candidate.cpp` | Requires ring membership + bond order; 4n+2 Hückel check |
| Rotatable bond (provider-aware) | `organic_candidate.cpp` | Ring exclusion via `RingProvider`; terminal bond exclusion |
| Lipid subtype inference | `organic_candidate.cpp` | phospholipid, sterol, glycolipid subtypes from functional group + ring data |
| VSEPR report export stabilization | `vsepr.cpp`, `include/cli/cmd_classify.hpp` | JSON-lite format option; tie to report layer |
| OrganicCandidate report export | `organic_candidate.cpp` | JSON-lite format option for pipeline report engine |
| Defect-induced local strain analysis | new bridge file | Defect occupancy → local strain; keep VSEPR and DefectFingerprint separate |

---

## Recommended WO-84 opening sequence

### WO-84A: Provider wiring
Wire all four `ProviderSet` providers into `VSEPROptions` and `OrganicClassifier`.
Gate each enhancement on `provider.available()` — zero behavioral change when absent.

### WO-84B: LonePairProvider implementation stub
Write a concrete `ElementLonePairProvider` that extends the current element table
to cover: Xe, Kr, Rn, transition metals (using group-based heuristics).
Attach to `VSEPROptions::providers.lone_pair` in the classify pipeline.

### WO-84C: RingProvider + ring_detector.hpp
Implement SSSR (smallest set of smallest rings) or a delegating interface.
Wire into `OrganicClassifier::count_rings()` and rotatable bond exclusion.

### WO-84D: BondOrderProvider + aromatic gate
With ring data and bond-order data both available, implement:
- Hückel aromaticity check (4n+2 rule) for `OrganicFamily::AROMATIC`
- Bond-order-dependent VSEPR disambiguation (square planar vs. tetrahedral)

### WO-84E–Z: Polish, export, known-bug closure
Close the known bugs documented in WO-83-vsepr-notes.md and WO-83-organic-bridge.md.
Expand JSON-lite export. Wire into Markdown report engine.

---

## 3D / Visual carry-forward (per WO-83 spec)

| Item | Notes |
|---|---|
| VSEPR site overlay in 3D viewer | Color-code sites by molecular shape in GL viewer |
| Hybridization hint labels in viewer | Show sp/sp2/sp3 label near each heavy atom |
| `vsepr classify` → visual bond geometry | Feed VSEPRReport to visualizer via `--viz` flag |
| Bond-order visualization | Color bonds by order (single/double/triple/aromatic) once BondOrderProvider is live |
| Reactive site markers | Show Fukui / reactive site markers as overlaid spheres |

---

## Design rules that must not change

1. VSEPR owns local geometry. OrganicCandidate never calls angle math.
2. Provider interfaces are null-safe. No crash when provider is absent.
3. Ring detection does not live inside VSEPR.
4. Aromaticity is not assigned without ring membership + bond pattern evidence.
5. `classify_vsepr_sites()` must remain deterministic (same input → same output).
6. WO-83C (full fanout execution) remains SKIPPED until expansion preview, candidate scoring, and selected execution are stable.
