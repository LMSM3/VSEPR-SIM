# Audit — `thermal_shell_demo.vsim` entry-point & kernel trace

> Generated during VSIM demo authoring session.  
> Covers: entry-point selection rationale, actual kernel flow, confirmed
> inaccuracies in `thermal_loss_explorer.cpp`, and low-quality patterns
> found across the thermal demo stack.

---

## 1. Entry-point map

| Executable / file | Role | Relevance |
|---|---|---|
| `apps/kernel_demo.cpp` | 18-scenario chemistry catalog; random seed per run; drives `KernelEventLog` directly | Reference for event emission pattern; **not** a thermal host |
| `apps/kernel_viz_demo.cpp` | Full 6-panel terminal overlay layout; drives `VsimVizAdapter` + `VsimRenderLayer` | Reference for `terminal_overlay_cycle` output_type |
| `apps/thermal_loss_explorer.cpp` | Standalone 1-D radial FD solver; randomised trial sweep; writes CSV + JSON ledger | **Primary kernel for thermal physics** |
| `apps/test_thermal_ar13.cpp` | Protocol A/B comparison: random quench vs thermal formation | Validation harness; hardcodes energy thresholds |
| `apps/test_thermal_vs_quench.cpp` | Mean/min energy pass/fail test between two thermal protocols | Thin wrapper around the same underlying FD logic |
| `apps/report_generator.cpp` | Autonomous report engine (`EngineConfig`); generates N markdown reports + CSV ledger | Shell-reporting host; no simulation physics |
| `apps/demo_provenance_shell.cpp` | Hash/fingerprint demo; builds H2O/CH4/NH3/SF6 from scratch | Orthogonal — useful only for identity/reproducibility demos |
| `apps/run_report_automation.cpp` | Batch automation wrapper over `report_generator` | Reporting pipeline only |

**Chosen host for `thermal_shell_demo.vsim`:** the VSIM runner (`vsper run`) 
which drives `vsim_parser` → `VsimDocument` → the chemistry/thermal kernel.  
`thermal_loss_explorer.cpp` is the standalone reference implementation; its
physics are the ground truth for the VSIM script parameters.

---

## 2. What the kernel actually does

### 2.1 Parse layer (`src/vsim/vsim_parser.cpp`)

- Sections are dispatched by string prefix in `apply_section_key`.
- `[excite.<type>]` creates an `ExciteEntry` keyed by type name.
  `thermal_spike` is a valid type string; `intensity`, `pulse_width_fs`,
  `profile` are the only parsed keys — everything else falls to `raw_sections`.
- `[environment]` maps `temperature` → `EnvironmentSection::temperature` (K),
  `medium` → string (not validated against an enum at parse time).
- `[observe] metrics` accepts an array or a single string; **unknown metrics
  are silently dropped to `raw_sections["observe"]`**.  No warning is emitted.
- `[run] temperature_K` and `temperature` are aliased — both map to
  `RunSection::temperature_K`.  The parser does **not** reconcile conflicts
  between `[run]` and `[environment]` temperature fields; the runner decides
  which takes precedence.
- `[while]` is parsed into a `WhileSection`; `condition` is a free string —
  the expression evaluator is in the runner, not the parser.

### 2.2 Thermal FD solver (`thermal_loss_explorer.cpp`)

Governing PDE (1-D radial, cylindrical coordinates):

```
dT/dt = alpha * (d²T/dr² + (1/r) * dT/dr)
alpha = k / (rho * Cp)
```

Outer wall heat loss (Newton cooling):

```
Q_loss [W] = h_outer * A_outer * max(T_surface - T_ambient, 0)
```

Explicit forward-Euler with adaptive sub-stepping:

```
dt_stable = dr² / (2 * alpha)
sub_steps = ceil(dt / dt_stable)
dt_sub    = dt / sub_steps
```

Boundary conditions per sub-step: `T[0] = Tc` (inner, Dirichlet),
`T[N-1] = Th` (outer, Dirichlet or decaying).

---

## 3. Confirmed inaccuracies

### 3.1 Q_loss accumulation uses `dt` not `dt_sub`

```cpp
// line ~220
Q_cumul += Q_loss * cfg.dt;   // <-- should be * cfg.dt_sub * sub_steps
							   //     or equivalently * cfg.dt, BUT Q_loss
							   //     is evaluated at T_surface AFTER sub-steps,
							   //     not as a running integral over them.
```

When `sub_steps > 1` the surface temperature changes during the sub-steps but
`Q_loss` is only sampled once at the end of the frame.  The cumulative energy
removed is therefore **underestimated** for high-alpha materials (FLiNaK,
Carbon Steel) where `sub_steps` can reach 10-50.

**Impact:** cumulative Q_loss reported in CSV is a lower bound, not the true
integrated heat removed.

### 3.2 `Th_decays` uses surface area × dr as a volume proxy

```cpp
double dTh = -Q_loss / (mat.rho * mat.Cp * cfg.geom.area_outer() * dr);
```

`area_outer * dr` ≈ a thin cylindrical shell area × radial thickness, not a
volume.  The correct annular shell volume is:

```
V_shell = pi * (r_outer² - r_inner²) * length
```

For thin shells (`r_outer - r_inner << r_outer`) the two are approximately
equal, but for the default geometry used in the explorer (`r_inner=0.02`,
`r_outer=0.05`) the error is ~30 %.

### 3.3 RMS convergence threshold is unscaled

```cpp
result.converged = result.rms_final < 1e4; // residual in T/m^2 units
```

`1e4 T/m²` is a hard-coded magic number with no material or geometry
normalization.  For a coarse grid the residual naturally inflates; for a fine
grid it naturally shrinks.  The `converged` flag is therefore **unreliable**
across different trial configurations.

### 3.4 Stability sub-stepping is per-trial, not per-material lookup

The `dt_stable` guard is computed from `alpha` of the selected material, but
`alpha()` is:

```cpp
double alpha() const { return k / (rho * Cp); }
```

For `FLiNaK` (k=1.0, rho=2290, Cp=1357) → alpha ≈ 3.2e-7 m²/s.  
For `Carbon_Steel` (k=50.0, rho=7850, Cp=490) → alpha ≈ 1.3e-5 m²/s.

The latter is ~40× higher; with the same `dt` and `dr` the steel case needs
~40× more sub-steps, making frame times unpredictable.  There is no warning
when `sub_steps` is large.

### 3.5 `[observe]` metric names are not validated at parse time

The parser stores unknown metric strings in `raw_sections` without warning.
If a metric name is misspelled (e.g., `"temperature.centre"` vs
`"temperature.center"`) the runner silently produces no output for that
metric.  There is no schema-enforced metric registry.

---

## 4. Low-quality patterns

| Location | Pattern | Problem |
|---|---|---|
| `kernel_demo.cpp` scenario catalog | Raw `ScenarioData` struct with positional double fields (`re_A`, `re_B`, `pe`, `bond_len`, `cn`, `ox`, `charge_A`, `charge_B`, `n_mol`, `eta`, `tag`) | No field names at call site; adding/removing a field silently shifts all values |
| `kernel_demo.cpp` emit_scenario | `std::string f(desc.formula); auto plus = f.find('+');` — reactant splitting by `+` character | Fails for multi-product formulas like `CO2+H2O`, produces wrong reactant list |
| `thermal_loss_explorer.cpp` MATERIAL_DB | `constexpr std::array<MaterialProps, 6>` — hardcoded 6-entry table | Adding a material requires changing the size constant and the array literal simultaneously; easy to desync |
| `thermal_loss_explorer.cpp` | `std::max(cfg.N_radial - 2, 1)` in the RMS loop denominator | Should be `std::max(cfg.N_radial - 2, 1)` in integer context, which is correct, but if `N_radial <= 2` the FD interior loop body never executes yet the residual is reported as 0 (not NaN/invalid) |
| `test_thermal_ar13.cpp` validation | `bool pass_mean = (E_mean_B < E_mean_A - 0.5)` | The 0.5 kcal/mol margin is hardcoded with no justification; passes on some random seeds, fails on others |
| `vsim_parser.cpp` observe handler | Unknown metrics silently dropped | Should at minimum emit a `[warn]` to stderr |
| `ExciteEntry::photon_energy_eV` | Present in schema for `thermal_spike` type | `photon_energy_eV` is meaningless for thermal spikes; the struct is reused without a per-type subset |

---

## 5. Shell reporting — running the script

```powershell
# From repo root, user-owned workspace (no sudo needed)
vsper run scripts/gallery/thermal_shell_demo.vsim

# On a shared/system install where out/ is not user-writable:
sudo vsper run scripts/gallery/thermal_shell_demo.vsim --output-root /var/vsim/out

# Headless CI (no terminal chart, just files)
vsper run scripts/gallery/thermal_shell_demo.vsim --headless --no-display

# Inspect artefacts after run
Get-ChildItem out/thermal_shell_demo -Recurse | Select-Object Name, Length
```

Expected outputs under `out/thermal_shell_demo/`:

```
report.md             -- Markdown report (human-readable)
manifest.json         -- Provenance manifest with hashes
metrics.tsv           -- Per-step scalar metrics
events.json           -- Kernel event log
analysis.json         -- Structure/chemistry analysis
initial_state.xyz     -- Starting geometry
trajectory.xyzf       -- Per-snapshot trajectory
figures/
  energy_trace.svg
  rdf.svg
  scalar_panel_xxx.svg
```

---

## 6. Recommended follow-up fixes

1. **Fix Q_loss accumulation** in `thermal_loss_explorer.cpp`: integrate over
   sub-steps or document that reported Q_cumul is a frame-endpoint estimate.
2. **Fix Th_decays volume**: replace `area_outer * dr` with
   `PI * (r_outer^2 - r_inner^2) * length`.
3. **Add metric validation** in `vsim_parser.cpp`: emit a `[warn]` when an
   unknown metric string is encountered in `[observe]`.
4. **Parameterize MATERIAL_DB** or replace the fixed-size `std::array` with a
   `std::vector` loaded from a `.toml` / `.vsim` material library.
5. **Add a `sub_steps` warning threshold**: if `sub_steps > 20`, log a warning
   so users know the timestep is very conservative for the chosen material.
