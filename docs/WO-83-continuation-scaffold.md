# WO-83 Continuation Scaffold

**Branch:** `feature/wizard-full-module-expansion`
**Area:** `atomistic/classify/`
**Scope:** Scaffold for WO-83 Part D through Part W.

---

## Scaffold Boundary

This document is scaffolding only. It defines placeholders, gates, and expected
artifacts for future WO-83 parts. It does not claim that Part D through Part W
features are implemented.

No new classifier behavior should be treated as complete until it has:

- source changes
- a real `.vsim` acceptance script
- optional developer smoke/regression tests
- documentation of what the script proves
- passing verification output

---

## Continuation Table

| Part | Status | Intended area | Required acceptance artifact | Notes |
|---|---|---|---|---|
| WO-83D | Implemented | Lone-pair inference | `examples/classify_acceptance/lone_pair_nh3.vsim`, `examples/classify_acceptance/lone_pair_h2o.vsim` | Neutral element-based inference added; formal charge and bond-order remain future work. |
| WO-83E | Scaffold | Formal charge support | `examples/classify_acceptance/formal_charge_probe.vsim` | Must not mutate atomistic truth state silently. |
| WO-83F | Scaffold | Bond-order-aware VSEPR | `examples/classify_acceptance/bond_order_probe.vsim` | Should extend VSEPR, not OrganicCandidate. |
| WO-83G | Scaffold | Ring detection ownership | `examples/classify_acceptance/ring_detection_probe.vsim` | Owner decision before implementation. |
| WO-83H | Scaffold | Aromaticity ownership | `examples/classify_acceptance/aromaticity_probe.vsim` | Likely organic/topology bridge, not VSEPR. |
| WO-83I | Scaffold | Rotatable bond logic | `examples/classify_acceptance/rotatable_bond_probe.vsim` | Replace placeholder count with topology-aware logic. |
| WO-83J | Scaffold | Lipid subtype inference | `examples/classify_acceptance/lipid_subtype_probe.vsim` | Infer subtype from descriptors, not a flat label. |
| WO-83K | Scaffold | VSEPR report formatter | `examples/classify_acceptance/vsepr_report_probe.vsim` | Formatter only; no classifier ownership changes. |
| WO-83L | Scaffold | OrganicCandidate report formatter | `examples/classify_acceptance/organic_report_probe.vsim` | Report derived descriptors and bridge inputs. |
| WO-83M | Scaffold | Report/export integration | `examples/classify_acceptance/classify_export_probe.vsim` | Export must report path, format, and status. |
| WO-83N | Scaffold | Geometry fallback regression | `examples/classify_acceptance/geometry_fallback_probe.vsim` | Lock NH3/H2O fallback behavior. |
| WO-83O | Scaffold | Bug reproduction harness | `examples/classify_acceptance/classify_bug_repro.vsim` | Use when a geometry assignment fails. |
| WO-83P | Scaffold | Descriptor support matrix | `examples/classify_acceptance/descriptor_matrix_probe.vsim` | Source-backed, partial, design-only. |
| WO-83Q | Scaffold | Audit manifest hook | `examples/classify_acceptance/audit_manifest_probe.vsim` | Record derived classifier outputs. |
| WO-83R | Scaffold | Example molecule fixtures | `examples/classify_acceptance/molecule_fixture_probe.vsim` | Keep fixtures deterministic. |
| WO-83S | Scaffold | CLI/demo route | `examples/classify_acceptance/classify_cli_probe.vsim` | Do not add public commands until support is clear. |
| WO-83T | Scaffold | Documentation sync | `examples/classify_acceptance/docs_sync_probe.vsim` | Sync docs with source status only. |
| WO-83U | Scaffold | Validation sweep | `examples/classify_acceptance/run_all_classify_acceptance.ps1` | Run classify label and targeted VSIM examples. |
| WO-83V | Scaffold | Carry-forward audit | `examples/classify_acceptance/carry_forward_probe.vsim` | Move unresolved items into next WO. |
| WO-83W | Scaffold | Freeze/handoff | `examples/classify_acceptance/classify_handoff_probe.vsim` | Final handoff once source, docs, and scripts match. |

---

## Part Template

```text
## WO-83X - Title

Status: Scaffold
Owner:
Source files:
Docs:
Acceptance artifact:

Goal:

Non-goals:

Implementation notes:

Validation:

Carry-forward:
```

---

## Acceptance Artifact Requirements

Every future part should add or update at least one real `.vsim` script that:

- exercises the new or fixed behavior
- requests the relevant classifier outputs through `[analysis.classify]` and `[observe]`
- writes enough exported data to prove pass/fail status
- documents what the script proves

C++ tests and Python demos may accompany the script as developer smoke tests,
but they do not replace runtime acceptance.

Use `scripts/demo_classify_scaffold.py` only as a lightweight developer demo
pattern.
