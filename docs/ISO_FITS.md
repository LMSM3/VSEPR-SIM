# ISO 286 Tolerance & Fit System

Reference document for `iso_fit.h` / `iso_fit.c`.

---

## 1. Zero Line + Deviations

Everything is measured from the **zero line** (basic / nominal size `D`).

```
                        <-- ES -->
    Zero line ----------[=========]---- hole tolerance zone
    (D = nominal)       EI=0 (for H)

                  <-- es -->
              [=========]--------------  shaft tolerance zone
              ei         es
```

**Hole** (uppercase letters):

    D_max = D + ES      D_min = D + EI      IT = ES - EI

**Shaft** (lowercase letters):

    d_max = d + es      d_min = d + ei      IT = es - ei

Deviation sign convention:
- Positive = above zero line (larger than nominal)
- Negative = below zero line (smaller than nominal)

---

## 2. The Three Fit Types

These three rules are **exhaustive and mutually exclusive**. No fourth state exists.

### A. Clearance Fit

```
delta_min = D_min - d_max > 0
delta_max = D_max - d_min > 0
therefore  delta > 0 always
```

```
HOLE:   [      D_min -------- D_max      ]
SHAFT:      [ d_min ---- d_max ]
                                  ^ gap always exists
```

| Property   | Value                                 |
|------------|---------------------------------------|
| Sign       | Always positive                       |
| Behavior   | Always a gap -- no contact            |
| Use        | Bearings, sliding shafts, hinges      |
| Purpose    | Allow movement, reduce friction       |

### B. Transition Fit

```
delta_min < 0   AND   delta_max > 0
therefore  delta in (-, +)
```

```
HOLE:   [     D_min -------- D_max     ]
SHAFT:     [ d_min -------- d_max ]
                    ^ overlap zone -- could be tight OR loose
```

| Property   | Value                                 |
|------------|---------------------------------------|
| Sign       | Sometimes +, sometimes -              |
| Behavior   | Uncertain -- depends on actual dims   |
| Use        | Precision alignment, gear mounting    |
| Purpose    | Balance easy assembly + positioning   |

### C. Interference Fit

```
delta_max = D_max - d_min < 0
delta_min = D_min - d_max < 0
therefore  delta < 0 always
```

```
HOLE:   [   D_min ---- D_max   ]
SHAFT:      [ d_min -------- d_max ]
                ^ shaft ALWAYS larger -- must force fit
```

| Property   | Value                                 |
|------------|---------------------------------------|
| Sign       | Always negative                       |
| Behavior   | Always overlap -- requires press/heat |
| Use        | Press-fit bearings, gears on shafts   |
| Purpose    | Prevent slip, transfer torque         |

---

## 3. Tolerance Grades (IT)

`IT = tolerance width` (precision of manufacture).

| Grade    | Meaning                        |
|----------|--------------------------------|
| IT5-IT6  | Tight precision                |
| IT7-IT8  | Common engineering fits        |
| IT9-IT10 | General manufacturing          |
| IT11+    | Loose / rough                  |

Formula (grades 5-18):

    IT_n = k * i,   where   i = 0.45 * cbrt(D) + 0.001 * D   [um, D in mm]

The **letter** = position relative to zero line.
The **number** = width of tolerance zone.

---

## 4. The H System

For hole letter **H**:

    EI = 0   ->   D_min = D_nominal

This is why H dominates:
- Holes are manufactured at nominal size
- Shaft is adjusted to achieve the desired fit
- Industry standard

---

## 5. Example: H7/k6

```c
IsoFitResult r = iso_compute_fit(25.0, 'H', 7, 'k', 6);
iso_fit_print(&r, stdout);
```

**Hole H7**: EI = 0, ES = IT7
**Shaft k6**: tolerance zone shifted just above zero line

Result: **Transition fit** -- sometimes clearance, sometimes interference.

---

## 6. Fit to Fatigue Link

| Fit Type     | Stress Effect                        | Fatigue Risk              |
|--------------|--------------------------------------|---------------------------|
| Clearance    | Minimal assembly stress              | Vibration fatigue         |
| Transition   | Moderate contact stress              | Stress concentrations     |
| Interference | Contact pressure (compressive)       | Fretting, edge notch     |

```
Fit type -> local stress field -> fatigue life
```

---

## 7. Solver Pipeline

```
Material lookup  ->  Sut, Se'
       |
Marin factors    ->  Se (corrected endurance limit)
       |
Geometry         ->  shaft / hole / gear dimensions
       |
Nominal stress   ->  from loading
       |
Fit type         ->  modifies local stress (contact pressure, fretting)
       |
sigma_a (local)  ->  actual alternating stress
       |
N = (sigma_a / a)^(1/b)   ->   fatigue life
```

---

## 8. API Reference

See `include/core/iso_fit.h` for the full interface.

### Core functions

| Function                | Purpose                                  |
|-------------------------|------------------------------------------|
| `iso_tolerance_unit(d)` | Standard tolerance unit `i` in um        |
| `iso_geometric_mean(d)` | Geometric mean of ISO size range         |
| `iso_it_value(d, grade)`| IT grade value in um                     |
| `iso_hole_deviations()` | EI, ES for a hole letter + grade         |
| `iso_shaft_deviations()`| ei, es for a shaft letter + grade        |
| `iso_compute_fit()`     | Full fit: deviations + delta + classify  |
| `iso_classify_fit()`    | Pure logic: three rules, no tables       |
| `iso_fit_print()`       | Formatted output with ASCII diagram      |

### Supported letters

**Holes**: H (dominant), A B C D E F G, K M N P R S T U

**Shafts**: a b c d e f g h, js (symmetric), k m n p r s t u

---

## 9. Build

```bash
# Standalone (gcc)
gcc -std=c11 -O2 -Wall -Wextra -lm -Iinclude -o test_iso_fit \
    tests/test_iso_fit.c src/core/iso_fit.c

# Via CMake (integrated into VSEPR-SIM)
cmake --build build --target test_iso_fit
ctest -R iso_fit
```
