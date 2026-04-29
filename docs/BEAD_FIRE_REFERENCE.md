# BeadFIRE Reporting & Visualization Reference

**VSEPR-SIM 3.0.0**
**Module:** Coarse-grained bead FIRE minimization — outcome reporting, CSV export, and trajectory visualization.

---

## 1. Architecture

`
   C++ LAYER                            PYTHON LAYER
 ┌──────────────────┐                 ┌────────────────────────┐
 │ BeadFIRE::       │    CSV export   │ visualize_bead_fire_   │
 │   minimize()     │────────────────>│   trajectory.py        │
 │                  │                 │                        │
 │ • Runs FIRE      │                 │ • Reads CSV            │
 │ • Records history│                 │ • XY/XZ projections    │
 │ • Stores results │                 │ • Energy decomposition │
 └────────┬─────────┘                 │ • Force evolution      │
          │                           │ • FIRE parameter plots │
          v                           │ • Combined panel       │
 ┌──────────────────┐                 └────────────────────────┘
 │ Markdown report  │
 │ (.md)            │
 └──────────────────┘
`

---

## 2. C++ API

### Markdown Report

`cpp
#include "coarse_grain/report/bead_fire_report.hpp"

auto result = BeadFIRE::minimize(beads, env, int_params, fire_params);
std::string report = bead_fire_report_md(result, beads.size(), fire_params);
`

Report sections: convergence outcome, energy decomposition, FIRE parameters,
trajectory history (sampled for large runs), mathematical model, references.

### CSV Export

`cpp
#include "coarse_grain/report/bead_fire_csv_export.hpp"

write_bead_fire_csv("trajectory.csv", result, beads);
`

### Combined Export

`cpp
write_bead_fire_complete_export("output/system_name", result, beads, fire_params);
// Creates: output/system_name.md  +  output/system_name.csv
`

---

## 3. CSV Format

`
step,U_total,U_steric,U_elec,U_disp,Frms,Fmax,alpha,dt,dU_per_bead,
bead0_x,bead0_y,bead0_z,bead1_x,bead1_y,bead1_z,...
`

One header row followed by one row per recorded step.
Per-bead columns repeat for each bead in the system.

---

## 4. Python Visualization

### Usage

`ash
# From CSV file
python scripts/visualize_bead_fire_trajectory.py trajectory.csv output_dir/

# Demo mode (synthetic data)
python scripts/visualize_bead_fire_trajectory.py
`

### Generated Figures

| File                    | Content                | Size     |
|-------------------------|------------------------|----------|
| 	rajectory_xy.png    | XY plane projection    | 10×10 in |
| 	rajectory_xz.png    | XZ plane projection    | 12×6 in  |
| energy_convergence.png| Energy decomposition  | 12×10 in |
| orce_evolution.png  | RMS / max force decay   | 12×6 in  |
| ire_parameters.png  | α and Δt evolution     | 12×8 in  |
| combined_report.png  | All panels combined    | 20×12 in |

All output at 300 DPI, publication-quality PNG.

### Dependencies

`ash
pip install numpy matplotlib
`

---

## 5. Report Content: Convergence Outcome

A generated report contains:

| Section               | Content                                          |
|-----------------------|--------------------------------------------------|
| Convergence Outcome   | Status, steps taken, U_final, F_RMS, F_max, α, Δt |
| Energy Decomposition  | Per-channel (steric, electrostatic, dispersion)   |
| FIRE Parameters       | dt_init, dt_max, α_init, f_inc, f_dec, ε_F, ε_U  |
| Trajectory History    | Sampled step table (first 10, every 10th, last 10)|
| Mathematical Model    | FIRE velocity-Verlet, power criterion, convergence|
| References            | Bitzek et al., PRL 97, 170201 (2006)             |

---

## 6. Mathematical Model

FIRE velocity-Verlet with velocity mixing:

Count\mathbf{v}_{t+1} = (1-\alpha)\mathbf{v}_t + \alpha\frac{\mathbf{F}_t}{\|\mathbf{F}_t\|}\|\mathbf{v}_t\|Count

Power criterion:  = \sum_i \mathbf{v}_i \cdot \mathbf{F}_i$

Convergence: $\|\mathbf{F}\|_\text{RMS} < \varepsilon_F$ or $|U_t - U_{t-1}|/N_\text{bead} < \varepsilon_U$

Forces via central finite difference:
CountF_i^\alpha = -\frac{U(\mathbf{r}_i + \delta\hat{e}_\alpha) - U(\mathbf{r}_i - \delta\hat{e}_\alpha)}{2\delta}Count

---

## 7. Example Report (Ethanol 12-bead)

**Status:** Converged

| Metric         | Value                     |
|----------------|---------------------------|
| Steps          | 150 / 2000                |
| N_beads        | 12                        |
| U_final        | -245.678 kcal/mol         |
| F_RMS          | 0.0001 (< ε_F)           |
| α_final        | 0.025                     |
| Δt_final       | 8.5 fs                    |

Energy decomposition:

| Channel       | Energy (kcal/mol) | Fraction |
|---------------|-------------------|----------|
| Steric        | -100.200          | 40.8%    |
| Electrostatic | -120.500          | 49.1%    |
| Dispersion    | -24.978           | 10.2%    |
| **Total**     | **-245.678**      | **100%** |

---

## 8. File Locations

| Component              | Path                                                 |
|------------------------|------------------------------------------------------|
| Markdown reporting     | coarse_grain/report/bead_fire_report.hpp           |
| CSV export             | coarse_grain/report/bead_fire_csv_export.hpp       |
| Python visualizer      | scripts/visualize_bead_fire_trajectory.py          |
| FIRE minimizer         | coarse_grain/models/bead_fire.hpp                  |
| Interaction engine     | coarse_grain/models/interaction_engine.hpp         |
| Example (basic)        | examples/example_bead_fire_reporting.cpp           |
| Example (complete)     | examples/example_complete_reporting_workflow.cpp    |
| Theory                 | section_anisotropic_beads.tex §4-5                 |

---

## 9. References

- Bitzek, E. et al., *Phys. Rev. Lett.* **97**, 170201 (2006) — FIRE algorithm.
- Lamoureux, G.; Roux, B., *J. Chem. Phys.* **119**, 3025 (2003) — Drude polarization.

---

*Consolidated from: bead_fire_quick_reference.md, bead_fire_reporting.md,
bead_fire_reporting_visual_overview.md, bead_fire_visualization_guide.md,
FINAL_SUMMARY_bead_fire_visualization.md, implementation_summary_bead_fire_reporting.md,
sample_bead_fire_report.md*
