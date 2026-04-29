# level3/ — Level 3 Beading: Macro-DM Preparation

**Status:** Preparation infrastructure (feeds macro-DM pipeline)

## Purpose

Level 3 Beading is the **third and coarsest resolution tier** in the
VSEPR-SIM multiscale hierarchy. A Level 3 Bead represents a *domain* —
a spatial cluster of Level 1/2 beads — and carries:

- Aggregate structural state (mean η, mean ρ, mean P₂ of member beads)
- QM descriptor bundle from `qm/`
- Macro-DM precursor payload from `macro_precursor.hpp`
- Direct handoff record for continuum/FEA export

## Scale hierarchy

```
Atomistic state         (Å scale, full atom positions)
       ↓  map_reaction_to_beads()
Level 1 Bead            (per-atom-group, ~1–5 atoms/bead)
       ↓  CG stepper (6+9 FIRE)
Level 2 Bead            (converged, environment-responsive)
       ↓  level3/aggregate_to_l3()
Level 3 Bead            (domain aggregate, macro-DM ready)
       ↓  macro_precursor.hpp
MacroPrecursorState     (rigidity*, ductility*, transport*...)
       ↓  (future) continuum export / FEA handoff
```

## Domain formation rule

L3 domains are formed by spatial clustering of converged L2 beads:
- Cluster radius: `r_domain` (default 15 Å)
- Minimum members: 4 beads per domain
- Cluster algorithm: greedy nearest-neighbour from seed bead

## Macro-DM handoff fields

Each `Level3Bead` produces a `Level3HandoffRecord` that feeds the
macro-dynamics model with:
- Effective mass density
- Effective charge density
- Structural order parameters (η̄, ρ̄, P̄₂)
- QM descriptors (φ̄, χ̄, ᾱ, q_eff)
- MacroPrecursorState channels

## Files

| File | Purpose |
|------|---------|
| `level3_bead.hpp` | `Level3Bead` struct, domain aggregation |
| `level3_handoff.hpp` | `Level3HandoffRecord` — macro-DM payload |
| `level3_builder.hpp` | `aggregate_to_l3()` — L2→L3 aggregation |
