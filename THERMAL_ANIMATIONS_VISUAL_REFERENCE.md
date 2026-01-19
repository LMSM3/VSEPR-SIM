/**
 * Thermal Animations - Quick Visual Reference
 * 
 * One-page guide to understanding and implementing thermal visualization
 */

# Thermal Animations - Visual Reference Guide

## Color Mapping System

### Thermal Color Spectrum (HSV)

```
TEMPERATURE SCALE (Kelvin)
┌────────────────────────────────────────────────────────────────┐
│  0 K          1000 K        2500 K        4000 K        5000 K │
│  ◆             ◆              ◆             ◆              ◆    │
│  BLUE        CYAN           GREEN         YELLOW          RED   │
│  (Frozen)  (Cold)        (Moderate)     (Warm)        (Hot)    │
└────────────────────────────────────────────────────────────────┘

HSV VALUES:
Hue:        240° ──────→ 180° ──────→ 120° ──────→ 60° ──────→ 0°
Saturation: 100% ────────────────────────────────────────────────
Value:      80% ─────────────────→ 100% (increases with temp)
```

### RGB Equivalents (Common)
```
Temperature    Color      RGB              Hex      Use Case
───────────────────────────────────────────────────────────────
0 K (Blue)     ■ Blue     (0, 0, 255)     #0000FF  Frozen atoms
500 K          ■ Cyan     (0, 255, 255)   #00FFFF  Cold liquid
1500 K         ■ Green    (0, 255, 0)     #00FF00  Ambient
2500 K         ■ Yellow   (255, 255, 0)   #FFFF00  Warm
4000 K         ■ Orange   (255, 165, 0)   #FFA500  Hot (glowing)
5000 K (Red)   ■ Red      (255, 0, 0)     #FF0000  Very hot
```

---

## Visualization Modes Comparison

### Mode: INSTANT (Per-Frame)
```
Time: t=100 ps
┌─────────────────────────────┐
│ System: Ethanol + Water     │
│ INSTANT Snapshot            │
│                             │
│    ●●●●●                    │
│   ●○●●●●                    │
│   ●●●●○●    Legend:         │
│    ●●●●●    ● Hot (red)    │
│     ●●●     ○ Cold (blue)  │
│                             │
│ Use: Real-time monitoring  │
│ Cost: Low                  │
│ Noise: High                │
└─────────────────────────────┘
```

### Mode: SMOOTH_50 (Averaged)
```
Same system, 50-frame rolling average
┌─────────────────────────────┐
│ Temperature: Smoothed       │
│ ⬤⬤⬤⬤⬤⬤⬤⬤⬤⬤                   │
│ (Less jittery, trends clear)│
│                             │
│ Use: Trend analysis         │
│ Cost: Medium                │
│ Noise: Low                  │
└─────────────────────────────┘
```

### Mode: HISTORY_1000 (Time Series)
```
Last 1000 frames plotted
┌──────────────────────────────────┐
│ Temperature Over Time            │
│                         ╱╲        │
│                    ╱╲  ╱  ╲       │
│              ╱╲   ╱  ╲╱    ╲╱╲  │
│         ╱╲ ╱  ╲ ╱               │
│     ╱╲ ╱  ╲╱                     │
│ ────────────────────────────────  │
│ 0      250    500    750   1000  │
│         Time Steps              │
│                                  │
│ Use: Equilibration tracking      │
│ Cost: High (plot rendering)      │
│ Noise: None (averaged)           │
└──────────────────────────────────┘
```

### Mode: DIFFERENTIAL (Change Rate)
```
Color indicates dT/dt (temperature change rate)

⬤ RED    = Heating (dT/dt > 0)
⬜ WHITE  = Stable (dT/dt ≈ 0)
⬤ BLUE   = Cooling (dT/dt < 0)

Visual effect: Shows energy flow patterns
               Reveals heat sources/sinks
               Identifies dissipative processes
```

---

## Implementation Architecture

### Data Flow Diagram

```
┌──────────────────────────────────────────────────┐
│ Molecular Dynamics Simulation                    │
│ ┌────────────────────────────────────────────┐  │
│ │ atoms[i].velocity[]                        │  │
│ │ atoms[i].mass                              │  │
│ │ atoms[i].position[]                        │  │
│ └────────────────────────────────────────────┘  │
└──────────────┬───────────────────────────────────┘
               │
               ▼
        ┌─────────────────────────────┐
        │ Compute Kinetic Energy      │
        │ KE = 0.5 * m * v²           │
        │ for each atom               │
        └──────────┬──────────────────┘
                   │
                   ▼
        ┌─────────────────────────────┐
        │ Map KE → Temperature        │
        │ T = KE / (3kB/2)            │
        │ (Boltzmann constant)        │
        └──────────┬──────────────────┘
                   │
                   ▼
        ┌─────────────────────────────┐
        │ Temperature → HSV Color     │
        │ ├─ Normalize T range        │
        │ ├─ Map to hue (240°→0°)     │
        │ └─ RGB output               │
        └──────────┬──────────────────┘
                   │
                   ▼
    ┌──────────────────────────────────┐
    │ Update Vertex Attributes         │
    │ atoms[i].color = RGB             │
    │ Upload to GPU (VAO)              │
    └──────────┬───────────────────────┘
               │
               ▼
    ┌──────────────────────────────────┐
    │ Render with Thermal Glow         │
    │ ├─ If T > threshold              │
    │ ├─ Add orange/red emission       │
    │ └─ Motion blur for fast atoms    │
    └──────────┬───────────────────────┘
               │
               ▼
    ┌──────────────────────────────────┐
    │ Display to User                  │
    │ ImGui Overlay:                   │
    │ ├─ Current T                     │
    │ ├─ Min/Max/Avg                   │
    │ ├─ Energy values                 │
    │ └─ Drift indicator               │
    └──────────────────────────────────┘
```

---

## Performance Profile

### Computational Cost Breakdown

```
For 1000-atom system @ 60 FPS

Task                        Time        % CPU
────────────────────────────────────────────────
Per-atom KE calculation     0.5 ms      8%
Temperature normalization   0.3 ms      5%
HSV → RGB conversion        0.8 ms      13%
GPU color buffer update     0.1 ms      2%
Statistics computation      0.5 ms      8%
ImGui overlay rendering     0.8 ms      13%
Thermal glow shader (GPU)   ~1.0 ms*    (GPU)
────────────────────────────────────────────────
TOTAL CPU                   3.0 ms      50% of 6ms frame budget
GPU overhead                ~1.0 ms     ~10% of GPU time
────────────────────────────────────────────────

Scaling: O(N) for N atoms
At 10K atoms: ~30 ms CPU (too slow, use GPU compute)
```

### GPU Compute Shader Alternative

```
For 10K+ atoms, use GLSL compute shader:

#version 430
layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer AtomBuffer {
    vec4 velocities[];
};
layout(std430, binding = 1) buffer ColorBuffer {
    vec4 colors[];
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    // Compute KE (parallel on GPU)
    vec3 v = velocities[idx].xyz;
    float mass = velocities[idx].w;
    float ke = 0.5 * mass * dot(v, v);
    
    // KE → color (parallel)
    colors[idx] = vec4(ke_to_hsv(ke), 1.0);
}

// Cost: <1ms for 100K atoms on modern GPU
// Scales linearly with parallelization
```

---

## Key Equations

### Temperature from Kinetic Energy
```
T = (2 * KE) / (3 * k_B)

where:
  T = Temperature (Kelvin)
  KE = Kinetic energy per atom (kcal/mol)
  k_B = Boltzmann constant (1.987 × 10⁻³ kcal/mol·K)

Simplified:
  T (K) ≈ KE (kcal/mol) / 0.0005956
```

### HSV to RGB Conversion
```
Given: H (hue), S (saturation), V (value)

C = V × S                       (chroma)
H' = H / 60°
X = C × (1 - |H' mod 2 - 1|)   (intermediate)

Depending on H' sector:
  if 0° ≤ H < 60°:   (R, G, B) = (C, X, 0)
  if 60° ≤ H < 120°:  (R, G, B) = (X, C, 0)
  if 120° ≤ H < 180°: (R, G, B) = (0, C, X)
  if 180° ≤ H < 240°: (R, G, B) = (0, X, C)
  if 240° ≤ H < 300°: (R, G, B) = (X, 0, C)
  if 300° ≤ H < 360°: (R, G, B) = (C, 0, X)

m = V - C
(R, G, B) = (R+m, G+m, B+m)
```

### Energy Conservation Check
```
Energy Drift = |E_current - E_initial| / |E_initial|

Acceptable thresholds:
  < 0.1%   → Excellent (Verlet, RK4)
  < 1.0%   → Good (standard integrators)
  < 10%    → Acceptable (rough approximation)
  > 10%    → Error (simulation invalid)
```

---

## Real-Time Diagnostics

### What Temperature Colors Tell You

```
🔵 BLUE (Cold)
   └─ Atoms at rest, low kinetic energy
   └─ Stable configuration
   └─ Good for: Crystal structures, bound states

🟢 GREEN (Ambient)
   └─ Normal thermal motion
   └─ 300-500 K typical
   └─ Good for: Room temperature simulations

🟡 YELLOW (Warm)
   └─ Elevated motion
   └─ 1000-2000 K
   └─ Good for: Heating, phase transitions

🟠 ORANGE (Hot)
   └─ High kinetic energy
   └─ 2500-4000 K
   └─ Good for: Identifying active sites, melting

🔴 RED (Very Hot)
   └─ Extreme motion, likely to dissociate
   └─ > 4000 K
   └─ Problem: Check for simulation instability
```

---

## Troubleshooting Guide

### Issue: All atoms are blue
**Cause**: KE calculation error or T_max too high
**Fix**: 
- Verify velocity values are correct
- Check Boltzmann constant in code
- Lower T_max to visible range

### Issue: Colors oscillate wildly
**Cause**: High-frequency noise in velocities
**Fix**:
- Use SMOOTH_50 mode instead of INSTANT
- Check timestep size (might be too large)
- Enable velocity smoothing filter

### Issue: Energy drift > 1%
**Cause**: Integrator instability or timestep too large
**Fix**:
- Reduce timestep (try 0.5 fs → 0.1 fs)
- Switch to better integrator (Verlet → RK4)
- Check force calculation accuracy

### Issue: Thermal colors don't update
**Cause**: GPU buffer not synced
**Fix**:
- Call `update_thermal_colors()` each frame
- Verify VAO binding is correct
- Check glBufferSubData() success

---

## Best Practices

### ✅ DO
- ✅ Use SMOOTH_50 for user-facing visualization
- ✅ Show energy conservation metric in overlay
- ✅ Log energy values to file for post-analysis
- ✅ Update thermal colors only when needed (dirty flag)
- ✅ Validate against literature heat capacity values

### ❌ DON'T
- ❌ Don't use INSTANT mode for publications (noisy)
- ❌ Don't ignore energy drift > 0.5%
- ❌ Don't hardcode temperature ranges (make configurable)
- ❌ Don't update GPU buffers every frame if unchanged
- ❌ Don't forget to convert units (K vs °C vs mK)

---

## Example: Setting Up Thermal Visualization

```cpp
// 1. Initialize
ThermalAnimation thermal;
thermal.T_min = 0.0f;       // Kelvin
thermal.T_max = 5000.0f;

// 2. Each simulation step
for (int step = 0; step < num_steps; ++step) {
    md.step(dt);            // Run MD step
    
    // 3. Update thermal data
    for (size_t i = 0; i < atoms.size(); ++i) {
        thermal.kinetic_energy[i] = 0.5f * atoms[i].mass * 
                                    dot(atoms[i].velocity, atoms[i].velocity);
    }
    
    // 4. Convert to colors
    std::vector<glm::vec3> colors;
    for (size_t i = 0; i < atoms.size(); ++i) {
        colors.push_back(thermal.energy_to_color(
            thermal.kinetic_energy[i]
        ));
    }
    
    // 5. Update visualization
    window.viz_router().update_thermal_colors(colors);
    
    // 6. Display stats
    if (step % 100 == 0) {
        std::cout << "T = " << thermal.compute_temperature() 
                  << " K\n";
    }
}
```

---

**Status**: ✅ Complete Reference  
**Ready for Implementation**: Yes  
**Quick Start Time**: 2-3 hours  

This reference enables rapid prototyping and debugging of thermal visualization systems.
