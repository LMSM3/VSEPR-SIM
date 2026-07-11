/**
 * energy_nonbonded.cu
 * -------------------
 * CUDA kernel for nonbonded energy computation (LJ + Coulomb).
 *
 * Performance targets:
 * - 1000 atoms:   ~0.5 ms  (vs 50 ms CPU)
 * - 10000 atoms:  ~50 ms   (vs 5000 ms CPU)
 *
 * WO-56B scaling: force kernel, warp reduction, dynamic charge buffer,
 *                 CUDA_CHECK error handling.
 */

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>
#include <cstdio>

namespace vsepr {
namespace gpu {

// ============================================================================
// Error checking
// ============================================================================

#define CUDA_CHECK(call)                                                        \
    do {                                                                        \
        cudaError_t _e = (call);                                                \
        if (_e != cudaSuccess) {                                                 \
            printf("[CUDA ERROR] %s:%d  %s\n",                                  \
                   __FILE__, __LINE__, cudaGetErrorString(_e));                  \
        }                                                                       \
    } while (0)

// ============================================================================
// Device Constants (loaded once at init — element-indexed, fixed size fine)
// ============================================================================

__constant__ float d_epsilon[118];  // LJ epsilon per element (Z=1-118)
__constant__ float d_sigma[118];    // LJ sigma per element

// Per-atom charges: stored in global device memory (no 10k hardcap).
// Pointer set via cuda_set_charges() before each evaluation.
static float* d_charge_global = nullptr;
static int    d_charge_alloc_n = 0;

// ============================================================================
// Configuration
// ============================================================================

// Compile-time flags
#define ENABLE_DEBUG_CHECKS 0  // Set to 1 for development, 0 for production
#define COULOMB_CONSTANT 332.0636f  // kcal*Å/(mol*e²)

// Cutoffs
#define LJ_CUTOFF      12.0f
#define COULOMB_CUTOFF 12.0f
#define LJ_CUTOFF_SQ      (LJ_CUTOFF      * LJ_CUTOFF)
#define COULOMB_CUTOFF_SQ (COULOMB_CUTOFF * COULOMB_CUTOFF)

// ============================================================================
// Inline Device Functions (fast, no overhead)
// ============================================================================

__device__ __forceinline__ 
float safe_rsqrt(float x) {
    // Fast inverse square root (no failsafe in release)
    #if ENABLE_DEBUG_CHECKS
    return (x > 1e-10f) ? rsqrtf(x) : 0.0f;
    #else
    return rsqrtf(x);  // Trust caller to avoid division by zero
    #endif
}

__device__ __forceinline__
float compute_lj_pair(float r2, float epsilon, float sigma) {
    // Lennard-Jones: 4ε[(σ/r)¹² - (σ/r)⁶]
    
    #if ENABLE_DEBUG_CHECKS
    if (r2 < 1e-6f) return 0.0f;  // Avoid singularity
    #endif
    
    float inv_r2 = 1.0f / r2;
    float sigma2 = sigma * sigma;
    float s2_r2 = sigma2 * inv_r2;
    float s6_r6 = s2_r2 * s2_r2 * s2_r2;
    float s12_r12 = s6_r6 * s6_r6;
    
    return 4.0f * epsilon * (s12_r12 - s6_r6);
}

__device__ __forceinline__
float compute_coulomb_pair(float r2, float qi, float qj) {
    // Coulomb: k*qi*qj/r
    
    #if ENABLE_DEBUG_CHECKS
    if (r2 < 1e-6f) return 0.0f;
    #endif
    
    float inv_r = safe_rsqrt(r2);
    return COULOMB_CONSTANT * qi * qj * inv_r;
}

// ============================================================================
// Kernel: Nonbonded Energy (All Pairs)
// ============================================================================

__global__ void compute_nonbonded_energy(
    const float* __restrict__ coords,
    const uint8_t* __restrict__ atomic_numbers,
    const float* __restrict__ charges,         // device pointer (dynamic)
    const int* __restrict__ exclusions,
    int num_exclusions_per_atom,
    int num_atoms,
    float* __restrict__ energy_out
)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    float local_energy = 0.0f;

    for (int i = tid; i < num_atoms; i += stride) {
        float xi = coords[3*i];
        float yi = coords[3*i + 1];
        float zi = coords[3*i + 2];

        uint8_t Zi   = atomic_numbers[i];
        float   qi   = charges[i];
        float epsilon_i = d_epsilon[Zi];
        float sigma_i   = d_sigma[Zi];

        for (int j = i + 1; j < num_atoms; ++j) {
            bool excluded = false;
            for (int e = 0; e < num_exclusions_per_atom; ++e) {
                if (exclusions[i * num_exclusions_per_atom + e] == j) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            float dx = coords[3*j]     - xi;
            float dy = coords[3*j + 1] - yi;
            float dz = coords[3*j + 2] - zi;
            float r2 = dx*dx + dy*dy + dz*dz;

            if (r2 > fmaxf(LJ_CUTOFF_SQ, COULOMB_CUTOFF_SQ)) continue;

            uint8_t Zj        = atomic_numbers[j];
            float epsilon_j   = d_epsilon[Zj];
            float sigma_j     = d_sigma[Zj];
            float epsilon_mix = sqrtf(epsilon_i * epsilon_j);
            float sigma_mix   = 0.5f * (sigma_i + sigma_j);

            if (r2 < LJ_CUTOFF_SQ)
                local_energy += compute_lj_pair(r2, epsilon_mix, sigma_mix);

            if (r2 < COULOMB_CUTOFF_SQ)
                local_energy += compute_coulomb_pair(r2, qi, charges[j]);
        }
    }

    energy_out[tid] = local_energy;
}

// ============================================================================
// Warp-level reduction helper
// ============================================================================

__device__ __forceinline__ float warp_reduce_sum(float val) {
    for (int offset = warpSize / 2; offset > 0; offset >>= 1)
        val += __shfl_down_sync(0xffffffff, val, offset);
    return val;
}

// ============================================================================
// Kernel: Reduce Partial Energies (Warp + Block reduction)
// ============================================================================

__global__ void reduce_energy(
    const float* __restrict__ partial_energies,
    int num_partials,
    float* __restrict__ total_energy
)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    float val = (idx < num_partials) ? partial_energies[idx] : 0.0f;

    // Warp-level reduction
    val = warp_reduce_sum(val);

    // First lane of each warp writes to shared memory
    __shared__ float warp_sums[32];
    int lane = threadIdx.x % warpSize;
    int warp_id = threadIdx.x / warpSize;
    if (lane == 0) warp_sums[warp_id] = val;
    __syncthreads();

    // Final reduction across warps (thread 0 only)
    if (threadIdx.x == 0) {
        float block_sum = 0.0f;
        int num_warps = (blockDim.x + warpSize - 1) / warpSize;
        for (int w = 0; w < num_warps; ++w)
            block_sum += warp_sums[w];
        atomicAdd(total_energy, block_sum);
    }
}

// ============================================================================
// Kernel: Nonbonded Forces (Gradients) — enables GPU-accelerated FIRE/MD
// ============================================================================

__global__ void compute_nonbonded_forces(
    const float* __restrict__ coords,
    const uint8_t* __restrict__ atomic_numbers,
    const float* __restrict__ charges,
    const int* __restrict__ exclusions,
    int num_exclusions_per_atom,
    int num_atoms,
    float* __restrict__ forces,            // [3*N] accumulated (+=)
    float* __restrict__ energy_out         // [num_threads] partial energies
)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    float local_energy = 0.0f;

    for (int i = tid; i < num_atoms; i += stride) {
        float xi = coords[3*i];
        float yi = coords[3*i + 1];
        float zi = coords[3*i + 2];

        uint8_t Zi      = atomic_numbers[i];
        float   qi      = charges[i];
        float epsilon_i = d_epsilon[Zi];
        float sigma_i   = d_sigma[Zi];

        float fix = 0.0f, fiy = 0.0f, fiz = 0.0f;

        for (int j = 0; j < num_atoms; ++j) {
            if (j == i) continue;

            bool excluded = false;
            for (int e = 0; e < num_exclusions_per_atom; ++e) {
                if (exclusions[i * num_exclusions_per_atom + e] == j) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            float dx = coords[3*j]     - xi;
            float dy = coords[3*j + 1] - yi;
            float dz = coords[3*j + 2] - zi;
            float r2 = dx*dx + dy*dy + dz*dz;

            if (r2 > fmaxf(LJ_CUTOFF_SQ, COULOMB_CUTOFF_SQ)) continue;

            uint8_t Zj        = atomic_numbers[j];
            float epsilon_j   = d_epsilon[Zj];
            float sigma_j     = d_sigma[Zj];
            float epsilon_mix = sqrtf(epsilon_i * epsilon_j);
            float sigma_mix   = 0.5f * (sigma_i + sigma_j);

            float inv_r2 = 1.0f / r2;
            float dU_dr = 0.0f; // dU/dr2 * 2 = force magnitude scalar (divided by r)

            // LJ force: dU/dr = 4ε [ -12σ¹²/r¹³ + 6σ⁶/r⁷ ]
            // In terms of r²: f_scalar = (1/r²) * 4ε [ -12(σ²/r²)⁶ + 6(σ²/r²)³ ]
            if (r2 < LJ_CUTOFF_SQ) {
                float s2 = sigma_mix * sigma_mix * inv_r2;
                float s6 = s2 * s2 * s2;
                float lj_force = 24.0f * epsilon_mix * inv_r2 * s6 * (1.0f - 2.0f * s6);
                dU_dr += lj_force;
                // Energy (half weight — each pair counted once from both sides)
                local_energy += 0.5f * 4.0f * epsilon_mix * s6 * (s6 - 1.0f);
            }

            // Coulomb force: dU/dr = -k*qi*qj / r² → in r²: -k*qi*qj*inv_r2*inv_r
            if (r2 < COULOMB_CUTOFF_SQ) {
                float qj = charges[j];
                float inv_r = safe_rsqrt(r2);
                dU_dr += -COULOMB_CONSTANT * qi * qj * inv_r * inv_r2;
                local_energy += 0.5f * COULOMB_CONSTANT * qi * qj * inv_r;
            }

            // Accumulate force on i: F_i = -dU/dx_i = dU/dr * (xi - xj)/r² * (-1) ... simplified
            fix += dU_dr * dx;
            fiy += dU_dr * dy;
            fiz += dU_dr * dz;
        }

        // Atomic accumulation (threads may overlap on the same i when stride != 1)
        atomicAdd(&forces[3*i],     fix);
        atomicAdd(&forces[3*i + 1], fiy);
        atomicAdd(&forces[3*i + 2], fiz);
    }

    energy_out[tid] = local_energy;
}

// ============================================================================
// Host Interface Functions
// ============================================================================

extern "C" {

void cuda_init_nonbonded_params(
    const float* epsilon,
    const float* sigma,
    int num_elements
) {
    CUDA_CHECK(cudaMemcpyToSymbol(d_epsilon, epsilon, num_elements * sizeof(float)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_sigma,   sigma,   num_elements * sizeof(float)));
}

void cuda_set_charges(const float* charges, int num_atoms) {
    // Grow device buffer if needed
    if (num_atoms > d_charge_alloc_n) {
        if (d_charge_global) CUDA_CHECK(cudaFree(d_charge_global));
        CUDA_CHECK(cudaMalloc(&d_charge_global, num_atoms * sizeof(float)));
        d_charge_alloc_n = num_atoms;
    }
    CUDA_CHECK(cudaMemcpy(d_charge_global, charges,
                          num_atoms * sizeof(float), cudaMemcpyHostToDevice));
}

float cuda_compute_nonbonded_energy(
    const float* h_coords,
    const uint8_t* h_atomic_numbers,
    const int* h_exclusions,
    int num_exclusions_per_atom,
    int num_atoms
) {
    float *d_coords, *d_partial_energies, *d_total_energy;
    uint8_t *d_atomic_numbers;
    int *d_exclusions;

    CUDA_CHECK(cudaMalloc(&d_coords,          3 * num_atoms * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_atomic_numbers,  num_atoms * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&d_exclusions,      num_atoms * num_exclusions_per_atom * sizeof(int)));

    CUDA_CHECK(cudaMemcpy(d_coords,         h_coords,         3 * num_atoms * sizeof(float),            cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_atomic_numbers, h_atomic_numbers, num_atoms * sizeof(uint8_t),              cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_exclusions,     h_exclusions,     num_atoms * num_exclusions_per_atom * sizeof(int), cudaMemcpyHostToDevice));

    int threads_per_block = 256;
    int num_blocks   = (num_atoms + threads_per_block - 1) / threads_per_block;
    int num_partials = num_blocks * threads_per_block;

    CUDA_CHECK(cudaMalloc(&d_partial_energies, num_partials * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_total_energy,     sizeof(float)));
    CUDA_CHECK(cudaMemset(d_total_energy, 0,   sizeof(float)));

    compute_nonbonded_energy<<<num_blocks, threads_per_block>>>(
        d_coords, d_atomic_numbers, d_charge_global,
        d_exclusions, num_exclusions_per_atom, num_atoms,
        d_partial_energies
    );

    reduce_energy<<<num_blocks, threads_per_block>>>(
        d_partial_energies, num_partials, d_total_energy
    );

    float total_energy;
    CUDA_CHECK(cudaMemcpy(&total_energy, d_total_energy, sizeof(float), cudaMemcpyDeviceToHost));

    CUDA_CHECK(cudaFree(d_coords));
    CUDA_CHECK(cudaFree(d_atomic_numbers));
    CUDA_CHECK(cudaFree(d_exclusions));
    CUDA_CHECK(cudaFree(d_partial_energies));
    CUDA_CHECK(cudaFree(d_total_energy));

    return total_energy;
}

float cuda_compute_nonbonded_forces(
    const float* h_coords,
    const uint8_t* h_atomic_numbers,
    const int* h_exclusions,
    int num_exclusions_per_atom,
    int num_atoms,
    float* h_forces_out
) {
    float *d_coords, *d_forces, *d_partial_energies, *d_total_energy;
    uint8_t *d_atomic_numbers;
    int *d_exclusions;

    CUDA_CHECK(cudaMalloc(&d_coords,          3 * num_atoms * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_atomic_numbers,  num_atoms * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&d_exclusions,      num_atoms * num_exclusions_per_atom * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_forces,          3 * num_atoms * sizeof(float)));

    CUDA_CHECK(cudaMemcpy(d_coords,         h_coords,         3 * num_atoms * sizeof(float),            cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_atomic_numbers, h_atomic_numbers, num_atoms * sizeof(uint8_t),              cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_exclusions,     h_exclusions,     num_atoms * num_exclusions_per_atom * sizeof(int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_forces, 0,      3 * num_atoms * sizeof(float)));

    int threads_per_block = 256;
    int num_blocks   = (num_atoms + threads_per_block - 1) / threads_per_block;
    int num_partials = num_blocks * threads_per_block;

    CUDA_CHECK(cudaMalloc(&d_partial_energies, num_partials * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_total_energy,     sizeof(float)));
    CUDA_CHECK(cudaMemset(d_total_energy, 0,   sizeof(float)));

    compute_nonbonded_forces<<<num_blocks, threads_per_block>>>(
        d_coords, d_atomic_numbers, d_charge_global,
        d_exclusions, num_exclusions_per_atom, num_atoms,
        d_forces, d_partial_energies
    );

    reduce_energy<<<num_blocks, threads_per_block>>>(
        d_partial_energies, num_partials, d_total_energy
    );

    float total_energy;
    CUDA_CHECK(cudaMemcpy(h_forces_out,  d_forces,       3 * num_atoms * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(&total_energy, d_total_energy, sizeof(float),                 cudaMemcpyDeviceToHost));

    CUDA_CHECK(cudaFree(d_coords));
    CUDA_CHECK(cudaFree(d_atomic_numbers));
    CUDA_CHECK(cudaFree(d_exclusions));
    CUDA_CHECK(cudaFree(d_forces));
    CUDA_CHECK(cudaFree(d_partial_energies));
    CUDA_CHECK(cudaFree(d_total_energy));

    return total_energy;
}

} // extern "C"

}} // namespace vsepr::gpu
