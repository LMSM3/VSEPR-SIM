# Green Loom Companion Grammar Audit

[W] GREEN_LOOM_COMPANIONS
[D] Independent
[I] Reuse existing pillar grammar and established visualization/open controls
[V] Initial audit complete; validation follows script creation
[P] Evidence: `docs/green_energy/DESIGN.md` and `scripts/green_energy/`
[N] Dedicated porous-capture runtime kernel is the remaining functional extension

## Decision

Use the existing parser-first pillar grammar without a new schema extension. The scenarios deliberately reuse the grammar accepted by the Green Loom import and Three Metallic Pillars work.

| Section / construct | Parser status | Usage |
|---|---|---|
| `[pillar]` | Structured pillar section | Scenario identity and authority declarations |
| `[species.registry]` | Structured list section | Colonated gas/site/state identifiers |
| `[reaction.network]` | Structured reaction network | Capture/release event policy |
| `[[reaction.channel]]` | Structured array-of-tables | Adsorption, competition, and regeneration channels |
| `[[reaction.stage]]` | Structured array-of-tables | Capture/purge/regenerate/cool schedule |
| `[audit.conservation]` / `[audit.acceptance]` | Structured audit sections | Acceptance conditions and residual policy |
| `[export]` / `[export.visual]` | Structured export sections | XYZF/XYZFull, dashboard, SVG, HTML output |
| `[visual]`, `[visual.workspace]`, `show ...` | Existing visual configuration | Dense, domain-specific output requests |
| `[open]`, `[open.advanced]` | Structured end-of-run presentation | Explicit viewer/replay summon and analysis overlays |
| `[fibre]`, `[ree_sites]`, `[transport]`, `[cycle]`, `[optimization]` | Raw declarative sections | Preserved for a future porous-capture kernel |

## Syntax checks

- Multi-line string lists are required for the species registry, metrics, and acceptance requirements.
- Quoted colonated species tokens, such as `"CO2:gas"` and `"site:Ce_vacancy"`, are supported by the pillar parser changes.
- Reaction channels and stages use double-bracket arrays-of-tables.
- The scripts use only parser-supported scalar/list forms for structured fields.

## Runtime limitation

Validation confirms grammar and core VSIM constraints, but domain-specific pore transport, adsorption, and regeneration keys are not executed by a dedicated runtime kernel. The visual outputs are fully declared; their populated scientific content depends on that future kernel.
