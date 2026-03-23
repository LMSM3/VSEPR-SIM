# Coarse-Grained Mapping Report

## Scheme: Ethanol 3-bead (CH3/CH2/OH)

| Property | Value |
|----------|-------|
| Source atoms | 9 |
| Beads produced | 3 |
| Reduction ratio | 9:3 (3:1) |
| Projection mode | COM |
| Bead-bead bonds | 2 |

## Conservation Check

| Quantity | Atomistic | Coarse-Grained | Error | Status |
|----------|-----------|----------------|-------|--------|
| Mass (amu) | 46.069 | 46.069 | 0 | ✓ PASS |
| Charge (e) | -5.55112e-17 | -5.55112e-17 | 0 | ✓ PASS |

## Per-Bead Detail

| Bead | Rule | Atoms | Mass | Charge | Residual (Å) | Position |
|------|------|-------|------|--------|--------------|----------|
| 0 | 0 | 0,1,2,3 | 15.035 | 0 | 0.329829 | (-1.62068, -0.00670436, -2.97733e-18) |
| 1 | 1 | 4,5,6 | 14.027 | 0.265 | 0.261472 | (0.0718614, -1.59564e-18, 0) |
| 2 | 2 | 7,8 | 17.007 | -0.265 | 0.44073 | (1.23556, 0.0474158, 0) |

## Diagnostics

| Metric | Value |
|--------|-------|
| Mean residual | 0.344011 Å |
| Max residual | 0.44073 Å |
| Atoms mapped | 9/9 |
| sane() | ✓ PASS |
