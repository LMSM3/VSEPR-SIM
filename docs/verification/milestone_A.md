# Verification Milestone A
## Deterministic Baseline Kernel Validation

**Status:** ✅ COMPLETE — 256/256 PASS, 0 FAIL, 0 SKIP  
**Date recorded:** 2025  
**Executable:** `apps/deep_verification.cpp` → `wsl-build/deep_verification`  
**Platform:** GNU/Linux x86\_64 (WSL2 6.6 / Debian), g++ 14.2, CMake 3.31  
**Branch:** `2.5.2.X`

---

## Milestone Statement

> The deep verification framework demonstrates full pass status for the currently
> implemented deterministic baseline across Lennard-Jones interactions, Coulomb
> electrostatics, dielectric screening, small-molecule bond detection, reference
> crystal geometry, composite force-energy consistency, noble-gas cluster
> relaxation with two-layer convergence checks, restoring-force scans,
> crystal-builder geometry fidelity, randomised supercell replication, and NVE
> energy conservation. Randomised seeded runs further confirm reproducibility
> and local consistency of force-energy behaviour. External empirical-reference
> suites pass when downloaded reference data are provisioned, and otherwise skip
> cleanly without indicating model failure.
>
> **This result marks completion of baseline deterministic kernel revalidation for
> the present physics scope, shifting the next development stage from foundational
> correctness recovery toward convergence tightening, expanded physical coverage,
> and formation-oriented workflow integration.**

---

## Suite Summary

| Suite | Description | Checks | Result |
|-------|-------------|--------|--------|
| **A** | LJ parameter audit — 42 UFF elements vs Rappé 1992 | 42 | ✅ PASS |
| **B** | Homonuclear dimer sweeps — energy curve, r_min, smoothness | 20 | ✅ PASS |
| **C** | Heteronuclear ion-pair Coulomb + force consistency | 15 | ✅ PASS |
| **D** | Crystal geometry vs Wyckoff (30 structures) | 90 | ✅ PASS |
| **E** | Molecule bond detection (9 molecules) | 9 | ✅ PASS |
| **F** | Composite force-energy consistency (H₂O, NH₃, CH₄, Ar₂) | 4 | ✅ PASS |
| **G** | Noble-gas cluster FIRE — two-layer: descent + convergence (Ar₂–Ar₇) | 30 | ✅ PASS |
| **H** | Solvent dielectric sweep (19 CRC 104 solvents) | 19 | ✅ PASS |
| **N** | Restoring-force scan: He₂–Xe₂ direction + energy-minimum check | 10 | ✅ PASS |
| **Q** | Crystal builder geometry fidelity — 4 presets, 2×2×2 supercell | 8 | ✅ PASS |
| **I** | Randomised LJ force-energy consistency (*N*=500, seeded) | 1 | ✅ PASS |
| **J** | Randomised Coulomb 1/*r* law (*N*=500, seeded) | 2 | ✅ PASS |
| **K** | Randomised dielectric ratio (*N*=500, seeded) | 1 | ✅ PASS |
| **L** | Randomised crystal supercell atom count | 6 | ✅ PASS |
| **M** | NVE energy conservation (Ar₃–Ar₅, velocity-Verlet) | 6 | ✅ PASS |
| **P** | Downloaded empirical references (PubChem + NIST) | 53 | ✅ PASS |
| | | | |
| | **Total** | **256** | **256 PASS** |

Canonical seed for the recorded run: `1772952709`  
Re-run with exact seed: `./deep_verification --seed 1772952709`

---

## Key Metrics

### Suite G — FIRE Convergence (Fibonacci sphere start, ε_Ar = 0.238 kcal/mol)

| Cluster | U₀ (kcal/mol) | U_final | Frms (kcal/mol/Å) | Steps |
|---------|--------------|---------|-------------------|-------|
| Ar₂ | −0.026 | −0.238 | 9.98 × 10⁻⁶ | 603 |
| Ar₃ | −0.696 | −0.714 | 5.35 × 10⁻⁶ | 308 |
| Ar₄ | −1.219 | −1.428 | 4.78 × 10⁻⁶ | 919 |
| Ar₅ | −1.675 | −2.167 | 8.72 × 10⁻⁶ | 1 269 |
| Ar₆ | −2.077 | −3.025 | 4.97 × 10⁻⁶ | 2 075 |
| Ar₇ | −2.462 | −3.928 | 8.55 × 10⁻⁶ | 2 491 |

Layer-1 (descent) and layer-2 (Frms < 0.10, no overlap, no stagnation) both pass for all clusters.

### Suite M — NVE Drift

| System | E₀ (kcal/mol) | Max drift |
|--------|--------------|-----------|
| Ar₃ | 1.167 | 2.91 × 10⁻¹² |
| Ar₄ | 1.817 | 5.27 × 10⁻¹² |
| Ar₅ | 0.314 | 2.26 × 10⁻¹¹ |

### Suite I/J/K — Randomised Worst-Case Error (500 samples)

| Suite | Worst relative error |
|-------|---------------------|
| I (LJ F-E) | < 1 × 10⁻³ |
| J (Coulomb 1/r) | < 1 × 10⁻⁶ |
| K (dielectric ratio) | < 1 × 10⁻⁶ |

---

## Interpretation

**What this confirms:**

- The LJ 12-6 kernel correctly reproduces Rappé 1992 UFF σ/ε for all 42 covered elements; forces match analytic gradients to < 10⁻³ relative error.
- The Coulomb kernel scales exactly as 1/*r* and the dielectric screening model scales exactly as *U*_vac / ε_r for the full experimental range.
- The FIRE minimiser converges deterministically to Frms ~ 10⁻⁵–10⁻⁶ kcal/mol/Å for noble-gas clusters, and NVE velocity-Verlet conserves total energy to machine precision (~10⁻¹¹).
- The crystal builder produces supercells with atom counts and nearest-neighbour distances matching Wyckoff references within 3%.
- 25 PubChem bond lengths and 20 NIST diatomic spectroscopic constants cross-check without contradiction.

**FIRE implementation note:** A deadlock condition in the Euler-FIRE integrator was identified and corrected during this milestone. After a P < 0 velocity reset in the pure-Euler formulation (`X += V·dt`, no acceleration term), setting `V = 0` caused `dU = 0` on the next evaluation, triggering premature exit on the `epsU` criterion. The fix: after a P < 0 reset, velocities are kicked along the normalised force direction (magnitude `dt`) rather than zeroed, and the `epsU` guard is conditioned on `epsU > 0`. This is consistent with the original Bitzek et al. (2006) intent when used with velocity-Verlet; the Euler variant requires the explicit kick.

---

## Known Limitations

| Area | Limitation |
|------|-----------|
| Metal/covalent crystals | UFF LJ r_min > actual bond length for Cu, Fe, Si. FIRE relaxation not applicable; Suite Q tests geometry only. |
| Suite G starting geometry | Fibonacci-sphere initialisation is a local descent test, not a global minimum search. Multiple local minima exist for Ar₄+. |
| Charges | All tests use formal integer charges or zero charge. QEq / Mulliken charge equilibration is not yet in the verification loop. |
| PBC | Suite G, N, Q run without periodic boundary conditions. Crystal stability under PBC is a planned future suite. |
| Bonded terms | Harmonic bond-stretch and angle-bend terms are present in the codebase but not yet covered by deep_verification. |

---

## Next Verification Targets

1. **Bond/angle perturbation scans on polyatomics** — extend Suite N to H₂O, NH₃ with bonded force fields
2. **PBC crystal stability** — small supercell NVE with periodic boundaries
3. **Defect perturbation** — vacancy and interstitial in NaCl/Cu supercell, topology preservation
4. **QEq charges** — verify charge equilibration convergence and Coulomb consistency
5. **Basin multiplicity** — multi-start FIRE on Ar₇ to map LJ local minima landscape

---

## Reproducibility

```bash
# Fetch live empirical references (PubChem + NIST)
wsl python3 scripts/fetch_empirical.py

# Full verification (downloaded refs included)
wsl bash scripts/run_all_verification.sh --full

# Exact milestone seed
wsl bash -c "cd wsl-build && ./deep_verification --seed 1772952709 --rand-iters 500"
```

Raw verbose output archived at: `verification/deep/milestone_A_run.txt`
