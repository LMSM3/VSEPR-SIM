# WO-VSEPR-SIM-58A — Beta-8 Closeout / Beta-9 Promotion

**Date:** 2025-07-14  
**Status:** CLOSED  
**Branch:** `v5.0.0-beta.7-step-attempt`

---

## Decision

Close beta-8 and freeze its defaults. Beta-9 will expand them.

---

## Beta-8 Closeout Summary

**Status: CLOSED / RUNTIME FOUNDATION COMPLETE**

### Beta-8 owns

- PBC runtime behavior
- FIRE/PBC compatibility
- Ewald ionic support
- runtime export flushing
- dashboard/report artifacts
- STEP sidecar export
- render cadence control
- basic language/runtime bridge (VSIM schema + parser foundation, WO-VSIM-03B schema/parser complete)

### Beta-8 does not own

| Item | Deferred to |
|---|---|
| Registry resolution engine | beta-9 |
| Minimal-input lab syntax expansion | beta-9 |
| Structure/material/run/environment registries | beta-9 |
| Large alias databases (alias map done; resolution engine deferred) | beta-9 |
| Installation / packaging / consumer-grade launch flow | **beta-10** |

> Installation code is explicitly deferred: installation / operating method / user-facing runtime packaging → beta-10.
> Trying to install a language before the language resolves correctly would be peak human software behavior.

---

## Beta-9 Promotion

### Theme

**v5.0.0-beta.9 — Registry Resolution and Minimal Lab Script Layer**

### Purpose

Take the language reference resolution chain:

```
explicit value
→ auto keyword
→ registry bundle defaults
→ context defaults
→ universal defaults
→ derived values
```

and make it real in runtime.

Beta-9 is where:

```toml
structure = "rocksalt"
```

actually expands into:

```
prototype    = "B1_NaCl"
space_group  = "Fm-3m"
basis        = "Na:0,0,0; Cl:0.5,0.5,0.5"
generator    = "ionic_rocksalt"
coordination = 6
default_charge_model = "formal"
```

logged as `[REGISTRY] material.prototype ← B1_NaCl (from alias rocksalt)`.

### Beta-9 success criteria

- `structure = "rocksalt"` resolves fully through the registry layer
- Registry-resolved fields are logged as `[REGISTRY]`
- A Level 0 `.vsim` script (`[material]` + `[run]`) runs to completion and exports xyz
- All WO-VSIM-03B sections parse and resolve correctly end-to-end
- WO-VSIM-03B and WO-VSIM-03C gate tables closed

### Promoted work orders

| WO | Title | State entering beta-9 |
|---|---|---|
| WO-VSIM-03B | Kill Explicit Object Authoring | Schema/parser/alias map complete; registry wiring pending |
| WO-VSIM-03C | Registry Resolution Engine | Not started |

---

## Gate Table

| Gate | Item | Status |
|---|---|---:|
| 58A-1 | Beta-8 runtime scope reviewed | ✅ |
| 58A-2 | Beta-8 declared closed for new feature intake | ✅ |
| 58A-3 | Installation code explicitly deferred to beta-10 | ✅ |
| 58A-4 | Registry/minimal-input work removed from beta-8 scope | ✅ |
| 58A-5 | Beta-9 theme defined | ✅ |
| 58A-6 | WO-VSIM-03B promoted into beta-9 | ✅ |
| 58A-7 | WO-VSIM-03C promoted into beta-9 | ✅ |
| 58A-8 | Beta-9 success criteria defined | ✅ |

---

## Files changed

| File | Change |
|---|---|
| `STAGE.md` | Version header → beta-9; beta-8 declared CLOSED; WO-58A gate table inserted; beta-9 stack section started; WO-VSIM-03B status block added; permanent-rules updated |
| `VSIM_REFERENCE.md` | Planned-features entry clarified; beta-9 owned items table added |
| `docs/wo/WO_58A.md` | This file |
