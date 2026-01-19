# XYZC Quick Reference Card

## Format Overview
`.xyzC` = High-precision molecular dynamics + thermal pathway tracking  
Frame capacity: 10,000-20,000 timesteps  
Spatial scale: 10,000 Ångström bounding box

---

## Six Mandatory Pathway Classes

| ID | Name | Physics | Coupling | Gate |
|----|------|---------|----------|------|
| 0 | `PHONON_LATTICE` | Bond vibrations (solids) | ~10 W/(m·K) | No |
| 1 | `ELECTRONIC` | Free electrons (metals) | ~50 W/(m·K) | No |
| 2 | `MOLECULAR_ROTATIONAL` | Asymmetric rotation | ~0.5 W/(m·K) | No |
| 3 | `TRANSLATIONAL_KINETIC` | Collisions (gas/fluid) | ~0.1 W/(m·K) | No |
| 4 | `RADIATIVE_MICRO` | Surface emission | ~0.01 W/(m·K) | Yes (T>0) |
| 5 | `GATED_STRUCTURAL` | Phase change, bond break | ~20 W/(m·K) | Yes (Ea) |

---

## Data Structures

### EnergyNode
```cpp
{
    double capacity;           // J/K
    double current_energy;     // J
    PathwayClass pathway_type; // 0-5
    uint32_t atom_index;       // Atom owner
}
```

### ThermalEdge
```cpp
{
    uint32_t node_i, node_j;           // Source → Target
    double coupling_strength;          // W/K
    std::array<double, 3> directionality;  // Unit vector
    double damping;                    // [0,1] loss
    bool is_gated;                     // Activation control
    double activation_energy;          // J/mol
    double gate_state;                 // [0,1] current
}
```

---

## Simulation Order (NON-NEGOTIABLE)

```cpp
1. Accumulate incoming energy
2. Evaluate activation gates       // gate_state = exp(-Ea/kT)
3. Transfer energy along edges     // flux = g_ij * (Ti - Tj)
4. Apply damping and losses
5. Promote coherent energy
6. Record observables              // Measure k, Cp, α
```

**Critical:** Never change this order!

---

## Quick Code Examples

### Create .xyzC File
```cpp
ThermalPathwayGraph graph(num_atoms);
graph.build_from_bonds(bonds, bond_orders, atomic_numbers);

XYZCWriter writer("output.xyzC");
writer.write_header(header);

for (uint32_t frame = 0; frame < 10000; frame++) {
    graph.simulation_step(1.0);  // 1 fs
    writer.write_frame(state);
}

writer.finalize();
```

### Read .xyzC File
```cpp
XYZCReader reader("output.xyzC");
reader.read_header(header);

FrameStateVector frame;
while (reader.read_frame(frame)) {
    std::cout << "T = " << frame.global_temperature << " K\n";
}
```

### Measure Properties
```cpp
double k = graph.measure_thermal_conductivity();   // W/(m·K)
double Cp = graph.measure_heat_capacity();         // J/K
double α = graph.measure_thermal_expansion();      // K^-1
```

---

## Build Commands

### Linux/Mac
```bash
bash build_xyzc_demo.sh
./build/xyzc_demo
```

### Windows (MSVC)
```cmd
build_xyzc_demo.bat
build\Release\xyzc_demo.exe
```

---

## File Locations

- Header: `include/thermal/xyzc_format.hpp`
- Implementation: `src/thermal/xyzc_format.cpp`
- Demo: `apps/xyzc_demo.cpp`
- Guide: `XYZC_FORMAT_GUIDE.md`
- Summary: `XYZC_IMPLEMENTATION_SUMMARY.md`

---

## File Size Estimates

| Atoms | Frames | Size (MB) | Gzipped (MB) |
|-------|--------|-----------|--------------|
| 100 | 1,000 | 5 | 1.5 |
| 1,000 | 10,000 | 500 | 150 |
| 10,000 | 10,000 | 5,000 | 1,500 |

---

## Activation Gate Example (Water Vaporization)

```cpp
ThermalEdge gated_edge;
gated_edge.is_gated = true;
gated_edge.activation_energy = 40660.0;  // J/mol (H2O)

// During simulation:
double T = node.temperature();
double R = 8.314462618;  // Gas constant J/(mol·K)
gated_edge.gate_state = exp(-40660.0 / (R * T));

// Below 373K: gate_state ≈ 0 (closed)
// At 373K: gate_state ≈ 0.5 (opening)
// Above 400K: gate_state ≈ 1 (open)
```

---

## Philosophy

**Emergent Observables**
- Measure thermal conductivity from pathway dynamics
- Measure heat capacity from energy nodes
- Don't hardcode material properties
- Let physics emerge from simulation

**Explicit Pathways**
- Energy moves along specific edges
- Not averaged/homogenized constants
- Each pathway has unique physics
- Activation gates for phase transitions

---

## Performance Tips

### Parallelization
```cpp
#pragma omp parallel for
for (size_t i = 0; i < edges.size(); i++) {
    compute_edge_flux(edges[i], dt);
}
```

### GPU Acceleration
- CUDA: 1000x speedup for large graphs
- OpenCL: Cross-platform support
- Metal: macOS/iOS optimization

### Compression
```bash
gzip demo_water_thermal.xyzC  # 3-5x reduction
```

---

## Common Pitfalls

❌ **Changing simulation order** - Will break energy conservation  
❌ **Hardcoding k, Cp, α** - Should be measured, not input  
❌ **Skipping gate evaluation** - Phase transitions won't work  
❌ **Forgetting damping** - Energy will accumulate unrealistically  
❌ **Isotropic directionality for all pathways** - Some need anisotropy  

✅ **Follow 6-step order exactly**  
✅ **Measure emergent properties**  
✅ **Evaluate gates before transfer**  
✅ **Apply damping after transfer**  
✅ **Use directionality vectors for rotational/radiative**

---

**Version:** 1.0  
**Last Updated:** January 18, 2026
