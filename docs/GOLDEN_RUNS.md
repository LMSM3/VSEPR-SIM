# Golden Runs

A Golden Run (`GR`) is a deterministic, auditable simulation scenario. A solver stopping condition alone is not a Golden Run result. The final status is `GOLDEN_RUN_PASSED` only after numerical, physical, conservation, restart, and render-stream gates all pass.

## GR-1 — Gas-to-Liquid Relaxation

GR-1 evolves 64 argon-like Lennard-Jones particles from a dispersed gas configuration to a persistent liquid cluster. It is a headless reduced-unit reference scenario: the rendering stream is generated from authoritative state samples and cannot mutate simulation state.

### Scope and units

GR-1 is a **reduced-unit Lennard-Jones reference**, not a prediction for laboratory argon or a reactive chemistry model. It uses `sigma = 1`, `epsilon = 1`, and unit particle mass. Temperature, pressure, density, time, energy, distance, and velocity are therefore reported in internally consistent reduced units unless a field explicitly says otherwise. This keeps the scenario suitable for deterministic regression testing while avoiding an unsupported conversion to SI units.

The model uses periodic boundaries, a shifted-force Lennard-Jones 12-6 cutoff, velocity-Verlet integration, deterministic velocity rescaling, and an explicitly declared isotropic volume-compression schedule. The compression schedule deliberately drives a dilute gas state toward a capture density; it is part of the resolved input, not a renderer or post-processing effect.

### Authoritative input

The resolved deck is [`examples/golden_runs/GR-1-gas-to-liquid.gr1`](../examples/golden_runs/GR-1-gas-to-liquid.gr1). It declares:

- particle count, species, molecular identity, seed, deterministic coordinate recipe, and deterministic initial velocities;
- periodic volume constraint, temperatures, timestep, integrator, thermostat/ensemble, and Lennard-Jones model parameters;
- simulation, sampling, checkpoint, and 60-fps render cadence;
- numerical, conservation, phase-classification, restart, and hold thresholds.

All coordinates and velocities actually used at step zero are retained in the first `trajectory.jsonl` record. The deck and resulting manifest are both required to reconstruct a run.

### Execute

```text
gr1-condensation --input examples/golden_runs/GR-1-gas-to-liquid.gr1 --output out/GR-1-gas-to-liquid
```

The target exits nonzero unless the full Golden Run gate passes.

The runner reads only values declared by the deck. Unknown descriptive fields are retained in the deck for audit context but do not alter the solver. Each resulting `manifest.json` records the operational values actually resolved by the runner, including the selected technique and final compression fraction.

### Determinism and restart behavior

Initial coordinate jitter is generated from `random_seed`; initial velocities are derived deterministically from the same seed and their center-of-mass velocity is removed. There is no stochastic thermostat update after initialization. A restart serializes the step index, box state, render-frame index, particle positions, and velocities. The replay comparison reconstructs state from the serialized checkpoint representation, not an in-memory object copy, then checks final positions and velocities against the uninterrupted execution within the declared tolerances.

The trajectory and final-state hashes are FNV-1a-64 regression identifiers. They are useful for same-build deterministic comparisons and artifact tracking, but they must not be treated as cryptographic signatures or cross-platform floating-point guarantees.

### Output contract

| Artifact | Purpose |
| --- | --- |
| `manifest.json` | Immutable run identity, resolved configuration, model units, phase thresholds, provenance identity, hashes, solver status, and final result. |
| `trajectory.jsonl` | Authoritative sampled positions, velocities, and cluster IDs. |
| `diagnostics.csv` | Energy, temperature, pressure/virial proxy, local density, phase indicator, cluster fraction, dispersion, diffusion proxy, RDF-change proxy, and classification history. |
| `render_60fps.jsonl` | Camera-independent world coordinates, particle radius, species, cluster IDs, phase state, diagnostics, timestamp, and frame number. Playback interpolates only this stream and never feeds a value back into the solver. |
| `checkpoint.gr1state` | Deterministic particle state at the declared checkpoint: timestep, box/volume state, render-frame index, positions, and velocities. Velocity-Verlet has no additional integration history; the deterministic velocity-rescale thermostat and the post-initialization RNG have no mutable state. |
| `restart_comparison.json` | Declared position/velocity tolerances and checkpoint-versus-uninterrupted result. |
| `conservation_report.json` | Mass, species balance, center-of-mass momentum, and NVT/compression energy-budget diagnostics. |
| `phase_classification.json` | Final multi-signal phase classification and hold requirement. |

`diagnostics.csv` has one row per deterministic sample and includes the following fields:

| Field group | Meaning |
| --- | --- |
| `potential`, `kinetic`, `total` | Instantaneous reduced-unit energy terms. In NVT compression, total energy is a budget diagnostic rather than a strict NVE invariant. |
| `temperature`, `pressure`, `pressure_spread` | Thermodynamic state and rolling relative pressure stabilization metric. |
| `local_density`, `dominant_cluster_fraction`, `dispersion` | Capture-cluster density, connected-component dominance, and cluster size proxy. |
| `msd`, `rdf_change` | Displacement from the starting state and a radial-behavior change proxy. |
| `temperature_spread`, `liquid` | Rolling temperature stabilization and the combined phase-classifier result. |

The first few stabilization values can be non-finite because a complete rolling window does not yet exist. This is expected and is handled by the post-renderer; acceptance checks occur only after enough samples have been accumulated.

Hashes use deterministic FNV-1a-64 serialization of the authoritative trajectory and final state. They are regression identifiers, not cryptographic integrity signatures.

### Phase classifier

`eta` is retained as a diagnostic only. Liquid confirmation requires all of the following thresholds from the resolved deck:

1. local-density increase relative to the starting gas density;
2. a dominant connected capture cluster;
3. bounded cluster dispersion;
4. bounded displacement from the starting state after volume control;
5. stable radial-behavior proxy (`rdf_change`);
6. stable temperature and pressure/virial behavior over the diagnostic window; and
7. persistent successful classification for `hold_samples` samples.

### Completion states

The GR-1 runner records these statuses separately:

- `NUMERICALLY_CONVERGED`
- `PHYSICALLY_STABLE`
- `LIQUID_STATE_CONFIRMED`
- `HOLD_PERIOD_PASSED`
- `CONSERVATION_PASSED`
- `RESTART_MATCHED`
- `RENDER_STREAM_VALID`
- `GOLDEN_RUN_PASSED`

`GOLDEN_RUN_PASSED` is emitted only when every prior required condition is true.

### Reading a pass or failure

The completion-state list is intentionally granular. A missing status identifies the failed gate without converting a physical qualification problem into a misleading numerical-success message:

| Missing state | Typical interpretation | First artifact to inspect |
| --- | --- | --- |
| `NUMERICALLY_CONVERGED` | Target-temperature residual did not meet the declared tolerance. | `manifest.json` solver block and `diagnostics.csv` temperature column |
| `PHYSICALLY_STABLE` | Final temperature or pressure window remains unstable. | `pressure_stability.png` and final diagnostic samples |
| `LIQUID_STATE_CONFIRMED` | Cluster, density, dispersion, diffusion, or RDF proxy failed. | `phase_classification.json` and `phase_cluster.png` |
| `HOLD_PERIOD_PASSED` | A liquid result occurred but did not persist for `hold_samples`. | Tail of `diagnostics.csv` `liquid` column |
| `CONSERVATION_PASSED` | Mass/species/momentum or declared NVT energy-budget tolerance failed. | `conservation_report.json` |
| `RESTART_MATCHED` | Serialized checkpoint replay diverged from uninterrupted execution. | `restart_comparison.json` and `checkpoint.gr1state` |
| `RENDER_STREAM_VALID` | Frame samples were absent or cadence was not 60 fps. | `render_60fps.jsonl` and manifest configuration |

### Automated verification

`GR1CondensationTest` executes GR-1 twice from the same deck, compares trajectory and final-state hashes, verifies checkpoint replay, asserts all completion states, and confirms that required artifacts exist. The CTest working directory is the source root so the test always consumes the authoritative deck.

## GR-1 chemical-technique demonstrations

The following demonstrations are reduced-unit, argon-like Lennard-Jones visualizations. Their names describe the deterministic thermodynamic technique on display; they do not claim an experimentally validated reactive chemical mechanism.

| Deck | Technique | Launcher |
| --- | --- | --- |
| `GR-1A-cryogenic-quench.gr1` | Lower target-temperature quench during deterministic compression | `scripts/gr1-cryogenic-quench.ps1` |
| `GR-1B-isothermal-compression.gr1` | Reference isothermal capture and condensation | `scripts/gr1-isothermal-compression.ps1` |
| `GR-1C-warm-liquid-anneal.gr1` | Higher target-temperature persistent-liquid anneal | `scripts/gr1-warm-liquid-anneal.ps1` |

Run a demonstration from the repository root after building `gr1-condensation`:

```text
powershell -ExecutionPolicy Bypass -File scripts/gr1-cryogenic-quench.ps1 -Exe build/gr1-condensation.exe
```

Each launcher produces the full GR-1 evidence set plus `post_render/gr1_report.xlsx` and four post-rendered PNGs:

- `energy_temperature.png`
- `phase_cluster.png`
- `pressure_stability.png`
- `final_frame_xy.png`

The workbook includes `Summary`, `Diagnostics`, `Frames`, and `Final particles` worksheets. `GR1ReportRendererTest` verifies that a true OOXML workbook and all four PNG raster files are produced from the authoritative trajectory and diagnostics; post-rendering remains read-only with respect to simulation state.

### Post-rendering details

`tools/render_gr1_report.py` is intentionally dependency-free. It reads `manifest.json`, `diagnostics.csv`, and `trajectory.jsonl`, then writes a standards-based OOXML ZIP workbook (`.xlsx`) and PNG rasters using only Python's standard library. It does not invoke the solver, modify source artifacts, interpolate coordinates, or alter classification results.

The workbook is designed for audit and handoff:

- **Summary** carries run identity, technique, terminal result, hashes, and sample/frame counts.
- **Diagnostics** preserves each exported numerical sample as numeric spreadsheet cells for filtering and downstream charting.
- **Frames** indexes every trajectory sample by step, simulation time, and particle count.
- **Final particles** provides final positions, velocities, and cluster identities for independent inspection.

The PNGs are post-rendered visual aids, not authority artifacts. `final_frame_xy.png` is an XY world-coordinate projection colored by cluster identity. The three chart PNGs use the diagnostic samples directly; non-finite warm-up stabilization values are displayed using the prior finite value so chart rasterization remains defined.

### Reproducible handoff checklist

For a review or publication handoff, retain the following as one immutable run bundle:

1. the exact `.gr1` input deck;
2. `manifest.json`, `checkpoint.gr1state`, and `restart_comparison.json`;
3. `trajectory.jsonl`, `diagnostics.csv`, and `render_60fps.jsonl`;
4. conservation and phase reports;
5. the generated `.xlsx` and PNG files as presentation derivatives; and
6. the executable/build identity recorded in the manifest.

To reproduce a result, build the same `gr1-condensation` target, run it with the preserved deck into a new directory, compare the resulting trajectory/final-state hashes under the declared platform tolerance policy, then run the post-renderer against the newly produced artifacts.
