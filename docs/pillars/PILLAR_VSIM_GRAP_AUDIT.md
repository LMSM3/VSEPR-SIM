# Three Metallic Pillars — VSIM Grammar Gap Audit

Date: Day 89 continuation  
Toolchain: VSEPR-SIM v5.14.1, release build `build/vsepr.exe`  
Script candidates: `scripts/pillars/cold_iron_path.vsim`, `quicksilver_path.vsim`, `star_gas_path.vsim`

## Summary

The proposed pillar scripts exercise a rich reactive-simulation contract (surface kinetics, coordination chemistry, gas-phase ignition). The current VSIM parser recognizes only a subset of the required sections and grammar constructs. This document records exactly what parses today and what must be added or simplified.

## Parser grammar behaviour observed

- Scalar key/value pairs parse reliably.
- A single-line bracketed list parses: `species = ["H2", "O2", "H2O"]`.
- Multi-line bracketed lists fail; each unquoted item is re-read as a key on the next line.
- Colons inside unquoted list items confuse the comma-splitter: `"Fe:lattice"` must be quoted.
- `[[section]]` array-of-tables is **not supported**. Repeated sub-sections collapse unless handled explicitly (e.g., `[object.surface]` for surfaces, `[[simulation.molecule]]` not recognized).
- Inline tables like `options = { ... }` are **not supported**.
- Unknown sections are preserved in `raw_sections` but not otherwise interpreted.

## Section dispatch map (from `src/vsim/vsim_parser.cpp`)

| Section in proposed script | Current parser support | Notes |
|---|---|---|
| `[project]` | ✅ supported | name, version, seed_base, determinism, description, author parse |
| `[pillar]` | ❌ not dispatched | Unknown → raw_sections only |
| `[cell]` | ✅ supported | type, lx, ly, lz, units parse |
| `[boundary]` | ✅ supported | x/y/z tokens parse |
| `[pbc]` | ✅ supported | known keys parse |
| `[material]` | ✅ supported | basic keys parse |
| `[[simulation.molecule]]` | ⚠️ not as array-of-tables | Treated as `[simulation]`, molecule overrides may not stack |
| `[environment]` | ✅ supported | temperature, pressure, medium, gravity parse |
| `[surface]` | ⚠️ different meaning | Routed to surface analysis applier, not a surface-reaction state model |
| `[species.registry]` | ❌ unknown | raw_sections only |
| `[reaction.network]` | ❌ unknown | raw_sections only |
| `[[reaction.channel]]` | ❌ unsupported | Array-of-tables; parser cannot represent it |
| `[reaction.passivation]` | ❌ unknown | raw_sections only |
| `[[reaction.stage]]` | ❌ unsupported | Array-of-tables |
| `[metal_center]` | ❌ unknown | raw_sections only |
| `[descriptor.electronic]` | ❌ unknown | raw_sections only |
| `[catalysis]` | ❌ unknown | raw_sections only |
| `[excite.thermal_spike]` / `[excite]` | ✅ `excite` exists | Keys must be added to `apply_excite_key` |
| `[thermodynamics]` | ❌ unknown | raw_sections only |
| `[ignition]` | ❌ unknown | raw_sections only |
| `[simulation]` | ✅ supported | common keys parse |
| `[chemistry]` | ✅ supported | chemistry, reaction_events, event_registry parse |
| `[run]` | ✅ supported | mode, max_steps, dt_fs, temperature_K, converge, output_level parse |
| `[observe]` | ✅ supported | metrics list, every_n_steps, output_format parse |
| `[audit.conservation]` | ❌ unknown | raw_sections only |
| `[audit.state_rules]` | ❌ unknown | raw_sections only |
| `[audit.acceptance]` | ❌ unknown | raw_sections only |
| `[export]` | ✅ supported | output dir / flags parse |
| `[export.visual]` | ✅ supported | SVG/figure flags parse |
| `[visual]` | ✅ supported | output_type, render_interval, proxy flags parse |
| `[visual.workspace]` | ⚠️ limited | `show` directives with inline tables fail; top-level keys parse |
| `[suite]` / `[suite.limits]` / `[suite.smoke]` | ✅ supported | groups, ordering, timeout, fail_fast, etc. |

## Consequence for the three-pillar implementation

The scripts cannot run unmodified in v5.14.1. Two strategies are available:

1. **Big-bang parser extension**: add appliers for `[pillar]`, `[species.registry]`, `[reaction.network]`, `[reaction.channel]`, `[reaction.passivation]`, `[reaction.stage]`, `[metal_center]`, `[catalysis]`, `[thermodynamics]`, `[ignition]`, extend list/array-of-table grammar, and wire a new reactive kernel. This is weeks of work.

2. **Scaffold now, grammar later**: implement the scientific model as a standalone C++ module with its own minimal configuration file (or a very small `[chem_plus]`-style handler), and incrementally migrate toward the full VSIM contract as parser support grows.

## Recommendation

Use strategy 2. The first milestone `PILLAR-01: Cold Iron Oxidation Cycle` should:

- Live in `src/pillars/cold_iron/` and `apps/pillar_cold_iron_demo.cpp`.
- Read a simplified VSIM-like spec or command-line config.
- Validate that the model itself can simulate Fe oxidation/passivation/reduction with strict conservation.
- Produce CSV/JSON event logs, species history, and a Markdown report.
- Add CTest coverage for conservation.

Once the model works, we extend the VSIM parser to accept `[pillar]` and `[reaction.channel]` blocks and dispatch them to the same implementation. This avoids building a cathedral of unsupported syntax before the science is proven.
