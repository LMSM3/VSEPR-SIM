# WO-56B — Appendix: CUDA Layer Review & Scale Report

**Day:** #56B  
**Subject:** `src/gpu/` — complete audit + scaling roadmap  
**Status:** Scale in progress

---

## 1. Current CUDA Footprint

| File | KB | Lines | Role |
|---|---|---|---|
| `src/gpu/kernels/energy_nonbonded.cu` | 9.4 | 285 | The only `.cu` — LJ + Coulomb energy kernel |
| `src/gpu/gpu_backend.cpp` | 4.6 | 188 | Singleton backend: CUDA/OpenCL/CPU fallback |
| `include/gpu/cuda_kernels.hpp` | 3.5 | 112 | Host interface declarations |
| `include/gpu/gpu_backend.hpp` | 2.8 | 117 | `GPUBackend` class + `DeviceBuffer<T>` |
| `tests/bench_gpu_cpu.cpp` | — | 288 | CPU vs GPU benchmark harness |

**Total GPU layer: ~20.3 KB across 5 files.**

CMake wiring (`cmake/CoreBuild.cmake` lines 267–299):
- `VSEPR_HAS_CUDA ON` → builds `vsepr_cuda_kernels` (STATIC) + `vsepr_gpu` (STATIC) with full kernel linkage
- `VSEPR_HAS_CUDA ON` but no `.cu` found → skeleton-only GPU backend
- `VSEPR_HAS_CUDA OFF` → CPU fallback, `vsepr_gpu` still links, no kernels

---

## 2. What Is Implemented (Review)

### 2.1 `compute_nonbonded_energy` kernel
- **O(N²) all-pairs** — no neighbor list
- LJ 12-6 via `compute_lj_pair(r², ε, σ)` with Lorentz-Berthelot mixing
- Coulomb via `compute_coulomb_pair(r², qᵢ, qⱼ)` with constant `k = 332.0636 kcal·Å/mol·e²`
- Hard cutoffs: `LJ_CUTOFF = COULOMB_CUTOFF = 12.0 Å`
- Thread stride loop: each thread handles a stride of `i` indices, inner loop `j > i`
- Partial energy per thread → `reduce_energy` pass

### 2.2 `reduce_energy` kernel
- Shared memory reduction, 256-thread blocks
- `atomicAdd` final accumulation into `d_total_energy`

### 2.3 `GPUBackend` singleton
- Auto-detects: CUDA → OpenCL → CPU fallback
- Populates `GPUInfo`: name, vendor, memory, compute units, double precision support
- `allocate / deallocate / copy_to_device / copy_from_device / synchronize`
- OpenCL init is a **stub** — declared, not implemented

### 2.4 Host interface (`cuda_kernels.hpp`)
- `cuda_init_nonbonded_params(ε[], σ[], N)` — loads to `__constant__` memory
- `cuda_set_charges(q[], N)` — loads to `__constant__ d_charge[10000]`
- `cuda_compute_nonbonded_energy(...)` — full malloc→compute→reduce→free pipeline
- Force computation declared as **"Future"** — not implemented

---

## 3. Known Defects & Limits

| ID | Defect | Location | Severity |
|---|---|---|---|
| D-GPU-1 | `d_charge[10000]` hardcoded — crashes > 10k atoms | `energy_nonbonded.cu:24` | **HIGH** |
| D-GPU-2 | Production exclusion check only covers `j < i+4` — wrong for general topology | `energy_nonbonded.cu:132–139` | **HIGH** |
| D-GPU-3 | No force/gradient kernel — FIRE/MD integration impossible on GPU | `cuda_kernels.hpp:70–112` | **HIGH** |
| D-GPU-4 | No PBC minimum-image convention | kernel inner loop | **HIGH** |
| D-GPU-5 | O(N²) scaling — no neighbor list | kernel structure | MEDIUM (OK for ≤8192 atoms) |
| D-GPU-6 | No shared memory tiling in main kernel — excessive global memory traffic | kernel | MEDIUM |
| D-GPU-7 | OpenCL path is a stub | `gpu_backend.cpp:88` | LOW |
| D-GPU-8 | No error checking on `cudaMalloc/cudaMemcpy` calls | host interface | MEDIUM |
| D-GPU-9 | `reduce_energy` uses only 1 block — bottleneck for large N | kernel | MEDIUM |
| D-GPU-10 | No multi-stream / async overlap | host interface | LOW |
| D-GPU-11 | ERB modulation (γ_steric, γ_elec, γ_disp) not on GPU | bead layer | MEDIUM |

---

## 4. Scaling Roadmap

Priority order for beta-7 → beta-8 GPU arc:

### Phase 1 — Fix blockers (this session)
1. **Force kernel** — `compute_nonbonded_forces` — unlocks GPU-accelerated FIRE
2. **Fix charge limit** — replace `__constant__ d_charge[10000]` with device buffer
3. **CUDA error checking** — wrap every CUDA call with `CUDA_CHECK` macro
4. **Warp-level reduction** — replace shared memory tree with `__shfl_down_sync`

### Phase 2 — Correctness
5. PBC minimum-image convention in kernel inner loop
6. Proper exclusion list (general topology, not `j < i+4` heuristic)

### Phase 3 — Scaling
7. Tiled O(N²) kernel with shared memory blocking (standard CUDA N-body pattern)
8. Neighbor list (Verlet) → O(N) scaling for large systems
9. Multi-block reduce (not 1 block)

### Phase 4 — Bead layer
10. ERB modulation on GPU — density-dependent γ channels in CUDA

---

## 5. Files to be Created/Modified

| File | Action |
|---|---|
| `src/gpu/kernels/energy_nonbonded.cu` | Add force kernel + warp reduction + charge fix + CUDA_CHECK |
| `include/gpu/cuda_kernels.hpp` | Declare `cuda_compute_nonbonded_forces()` |

---

*Phase 1 implementation follows immediately.*
