# Fatigue Analysis — Reference Document

All formulas, rules, and API for S-N fatigue life analysis.

---

## 1. Loading Types

### (a) Fully Reversed

Stress swings equally positive and negative.

```
sigma_m = 0
sigma_a = sigma_max = |sigma_min|
R = -1
```

Example: rotating shaft in bending, symmetric tension-compression.

### (b) Repeated

Stress goes from 0 to a max.

```
sigma_min = 0,  sigma_max > 0
sigma_m = sigma_max / 2
sigma_a = sigma_max / 2
R = 0
```

### (c) Fluctuating

General case.

```
sigma_m = (sigma_max + sigma_min) / 2
sigma_a = (sigma_max - sigma_min) / 2
```

**These are the first equations to write on almost any fatigue problem.**

---

## 2. Stress Ratio

```
R = sigma_min / sigma_max
```

| Case            | R    |
|-----------------|------|
| Fully reversed  | -1   |
| Repeated        |  0   |

---

## 3. Endurance Limit Estimation

Rotating-beam endurance limit for polished lab specimen (steel):

```
Se' = 0.5 * Sut       for Sut <= 1400 MPa
Se' = 700 MPa         for Sut >  1400 MPa  (cap)
```

Imperial: Se' ≈ 0.5 * Sut for Sut ≤ 200 ksi, cap ≈ 100 ksi.

**This is only the unmodified endurance limit.**

---

## 4. Marin Factors

Actual machine parts are not polished lab coupons.

```
Se = ka * kb * kc * kd * ke * kf * Se'
```

### Surface Factor ka

```
ka = a * Sut^b     (Sut in MPa)
```

| Finish       | a     | b      |
|-------------|-------|--------|
| Ground      | 1.58  | -0.085 |
| Machined    | 4.51  | -0.265 |
| Hot-rolled  | 57.7  | -0.718 |
| As-forged   | 272.0 | -0.995 |

Worse surface → more surface flaws → more crack initiation → lower Se.

### Size Factor kb

For round rotating bar in bending/torsion:

```
d <= 2.79 mm:   kb = 1.0
2.79 < d <= 51: kb = 1.24 * d^(-0.107)
51 < d <= 254:  kb = 1.51 * d^(-0.157)
```

Larger parts are weaker in fatigue (more volume, more likely flaws).
For axial loading: kb = 1.0.

### Load Factor kc

| Loading  | kc   |
|----------|------|
| Bending  | 1.00 |
| Axial    | 0.85 |
| Torsion  | 0.59 |

### Temperature Factor kd

```
kd = 0.975 + 0.432e-3*T - 0.115e-5*T^2 + 0.104e-8*T^3 - 0.595e-12*T^4
```

T in °C. For T ≤ 20°C, kd = 1.0.

### Reliability Factor ke

| Reliability | ke    |
|-------------|-------|
| 50%         | 1.000 |
| 90%         | 0.897 |
| 95%         | 0.868 |
| 99%         | 0.814 |
| 99.9%       | 0.753 |
| 99.99%      | 0.702 |
| 99.999%     | 0.659 |
| 99.9999%    | 0.620 |

Higher reliability → lower ke. "Make it safer by pretending the material got worse."

### Miscellaneous Factor kf

User-supplied, typically 1.0. Covers residual stresses, coatings, etc.

---

## 5. Fatigue Stress Concentration

Static:

```
Kt = max_stress / nominal_stress
```

Fatigue — not all geometric sharpness hurts as much as elasticity predicts:

```
Kf = 1 + q * (Kt - 1)
```

For torsion:

```
Kfs = 1 + qs * (Kts - 1)
```

| Symbol | Meaning                              | Source                    |
|--------|--------------------------------------|---------------------------|
| Kt     | Theoretical SCF                      | Geometry charts           |
| q      | Notch sensitivity (0..1)             | Material/notch-radius     |
| Kf     | Fatigue SCF (what enters the calc)   | Computed                  |

---

## 6. Mean Stress Correction

When both alternating and mean stress exist:

### (a) Goodman (most common)

```
sigma_a/Se + sigma_m/Sut = 1/n
n = 1 / (sigma_a/Se + sigma_m/Sut)
```

### (b) Soderberg (most conservative)

```
sigma_a/Se + sigma_m/Sy = 1/n
n = 1 / (sigma_a/Se + sigma_m/Sy)
```

### (c) Gerber (parabolic, less conservative)

```
sigma_a/Se + (sigma_m/Sut)^2 = 1/n
```

| Method    | Conservatism | When to use                    |
|-----------|-------------|--------------------------------|
| Soderberg | Most        | Maximum safety margin          |
| Goodman   | Moderate    | Standard design line           |
| Gerber    | Least       | More realistic for ductile     |

**Ordering: n_Soderberg < n_Goodman < n_Gerber** (for positive mean stress).

---

## 7. S-N Finite Life

For N in the 10^3 to 10^6 range:

```
Sf = a * N^b
```

### Anchor points

```
S1 = f * Sut   at  N1 = 10^3       (f ~ 0.9 for steel)
S2 = Se         at  N2 = 10^6
```

### Deriving a, b

```
b = log10(S2/S1) / log10(N2/N1)
a = S1 / N1^b
```

### Cycles to failure

```
N = (sigma_a / a)^(1/b)
```

### Life regimes

| N range    | Regime         | Method                 |
|------------|----------------|------------------------|
| < 10^3     | Low-cycle      | Strain-life (E-N)      |
| 10^3-10^6  | Finite life    | S-N curve              |
| >= 10^6    | Infinite life  | Endurance limit design |

---

## 8. Combined Loading (von Mises for Fatigue)

For shafts with bending + torsion:

```
sigma_a' = sqrt(sigma_a^2 + 3 * tau_a^2)
sigma_m' = sqrt(sigma_m^2 + 3 * tau_m^2)
```

Then use Goodman with equivalent stresses:

```
sigma_a'/Se + sigma_m'/Sut = 1/n
```

Common shaft stress sources: bending (gears/pulleys), torsion (transmitted torque),
stress raisers (shoulders, keyways, snap-ring grooves, press fits).

---

## 9. API Reference

See `include/core/fatigue.h` for the full interface.

### Loading

| Function                       | Purpose                          |
|--------------------------------|----------------------------------|
| `fatigue_stress_components()`  | Decompose max/min → mean/alt    |
| `fatigue_classify_loading()`   | Reversed/Repeated/Fluctuating   |
| `fatigue_stress_ratio()`       | R = sigma_min / sigma_max        |

### Endurance

| Function                  | Purpose                                   |
|---------------------------|-------------------------------------------|
| `fatigue_sep_estimate()`  | Se' from Sut (with 700 MPa cap)           |

### Marin Factors

| Function              | Purpose                                        |
|-----------------------|------------------------------------------------|
| `fatigue_marin_ka()`  | Surface factor from Sut + finish type          |
| `fatigue_marin_kb()`  | Size factor from diameter                      |
| `fatigue_marin_kc()`  | Load factor (bending/axial/torsion)            |
| `fatigue_marin_kd()`  | Temperature factor (polynomial)                |
| `fatigue_marin_ke()`  | Reliability factor (table lookup)              |
| `fatigue_marin_Se()`  | Corrected Se = ka*kb*kc*kd*ke*kf*Sep           |

### Stress Concentration

| Function          | Purpose                            |
|-------------------|------------------------------------|
| `fatigue_Kf()`    | Kf = 1 + q*(Kt - 1) for normal    |
| `fatigue_Kfs()`   | Kfs = 1 + qs*(Kts - 1) for torsion|

### Mean Stress

| Function                    | Purpose                               |
|-----------------------------|---------------------------------------|
| `fatigue_safety_factor()`   | n via Goodman/Soderberg/Gerber        |

### S-N Curve

| Function               | Purpose                                     |
|------------------------|---------------------------------------------|
| `fatigue_sn_constants()` | Derive a, b from f*Sut and Se             |
| `fatigue_Sf()`           | Fatigue strength at N cycles: a*N^b       |
| `fatigue_cycles()`       | Cycles to failure: (sigma_a/a)^(1/b)     |

### Combined Loading

| Function                  | Purpose                                    |
|---------------------------|--------------------------------------------|
| `fatigue_vonmises_alt()`  | sigma_a' = sqrt(sa^2 + 3*ta^2)            |
| `fatigue_vonmises_mean()` | sigma_m' = sqrt(sm^2 + 3*tm^2)            |

---

## 10. Tools

| Tool                     | Purpose                           | Build                |
|--------------------------|-----------------------------------|----------------------|
| `tools/fatigue_calc.c`   | Full pipeline CLI tool            | CMake: `fatigue_calc`|

```bash
# Simple mode (matches FATIGUE.ti)
./fatigue_calc default
./fatigue_calc 400 200 0.70 0.85 0.897 150

# Full pipeline mode
./fatigue_calc full 400 300 300 100 40 20 1 25 0 0.90 2.0 0.85
#                   Sut Sy  sMax sMin tA tM surf d  load rel  Kt  q
```

Surface codes: 0=ground, 1=machined, 2=hot-rolled, 3=as-forged
Load codes: 0=bending, 1=axial, 2=torsion

---

## 11. TI-84 Program

See `ti84/programs/FATIGUE.ti` — implements the full pipeline on-calculator.

```
LCONF layout:
  (1)=Sut  (2)=Sy  (3)=Sep  (4)=ka  (5)=kb  (6)=kc  (7)=Kt  (8)=q
```

Run `SETUP` first to configure defaults, then `FATIGUE` for analysis.

---

## 12. Build

```bash
# Library
cmake --build build --target vsepr_fatigue

# Tests (116 assertions)
cmake --build build --target test_fatigue
./build/tests/test_fatigue

# CLI tool
cmake --build build --target fatigue_calc
```
