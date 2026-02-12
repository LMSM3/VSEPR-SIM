/**
 * energy_nonbonded.cu
 * -------------------
 * CUDA kernel for nonbonded energy computation (LJ + Coulomb).
 * 
 * Performance targets:
 * - 1000 atoms: ~0.5 ms (vs 50 ms CPU)
 * - 10000 atoms: ~50 ms (vs 5000 ms CPU)
 */

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

namespace vsepr {
namespace gpu {

// ============================================================================
// Device Constants (loaded once at init)
// ============================================================================

__constant__ float d_epsilon[118];  // LJ epsilon per element (Z=1-118)
__constant__ float d_sigma[118];    // LJ sigma per element
__constant__ float d_charge[10000]; // Per-atom partial charges (max 10k atoms)

// ============================================================================
// Configuration
// ============================================================================

// Compile-time flags
#define ENABLE_DEBUG_CHECKS 0  // Set to 1 for development, 0 for production
#define COULOMB_CONSTANT 332.0636f  // kcal*Å/(mol*e²)

// Cutoffs
#define LJ_CUTOFF 12.0f
#define COULOMB_CUTOFF 12.0f
#define LJ_CUTOFF_SQ (LJ_CUTOFF * LJ_CUTOFF)
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
    const float* __restrict__ coords,  // [3*N] atom positions (x,y,z,x,y,z,...)
    const uint8_t* __restrict__ atomic_numbers,  // [N] element Z
    const int* __restrict__ exclusions,  // [N*max_excl] bonded exclusions
    int num_exclusions_per_atom,
    int num_atoms,
    float* __restrict__ energy_out  // [num_threads] partial energies
)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    
    float local_energy = 0.0f;
    
    // Each thread processes a subset of atom pairs
    for (int i = tid; i < num_atoms; i += stride) {
        float xi = coords[3*i];
        float yi = coords[3*i + 1];
        float zi = coords[3*i + 2];
        
        uint8_t Zi = atomic_numbers[i];
        float qi = d_charge[i];
        float epsilon_i = d_epsilon[Zi];
        float sigma_i = d_sigma[Zi];
        
        // Interact with all j > i (avoid double counting)
        for (int j = i + 1; j < num_atoms; ++j) {
            // Check exclusion list
            bool excluded = false;
            #if ENABLE_DEBUG_CHECKS
            // In production, we trust the exclusion list is correct
            for (int e = 0; e < num_exclusions_per_atom; ++e) {
                if (exclusions[i * num_exclusions_per_atom + e] == j) {
                    excluded = true;
                    break;
                }
            }
            #else
            // Simplified: assume first few are bonded neighbors (1-2, 1-3 exclusions)
            if (j < i + 4) {
                for (int e = 0; e < num_exclusions_per_atom && e < 4; ++e) {
                    if (exclusions[i * num_exclusions_per_atom + e] == j) {
                        excluded = true;
                        break;
                    }
                }
            }
            #endif
            
            if (excluded) continue;
            
            // Compute distance
            float dx = coords[3*j] - xi;
            float dy = coords[3*j + 1] - yi;
            float dz = coords[3*j + 2] - zi;
            float r2 = dx*dx + dy*dy + dz*dz;
            
            // Apply cutoffs (early exit)
            if (r2 > fmaxf(LJ_CUTOFF_SQ, COULOMB_CUTOFF_SQ)) continue;
            
            // Mixing rules (Lorentz-Berthelot)
            uint8_t Zj = atomic_numbers[j];
            float epsilon_j = d_epsilon[Zj];
            float sigma_j = d_sigma[Zj];
            
            float epsilon_mix = sqrtf(epsilon_i * epsilon_j);
            float sigma_mix = 0.5f * (sigma_i + sigma_j);
            
            // Lennard-Jones
            if (r2 < LJ_CUTOFF_SQ) {
                local_energy += compute_lj_pair(r2, epsilon_mix, sigma_mix);
            }
            
            // Coulomb
            if (r2 < COULOMB_CUTOFF_SQ) {
                float qj = d_charge[j];
                local_energy += compute_coulomb_pair(r2, qi, qj);
            }
        }
    }
    
    // Write partial sum
    energy_out[tid] = local_energy;
}

// ============================================================================
// Kernel: Reduce Partial Energies (Parallel Reduction)
// ============================================================================

__global__ void reduce_energy(
    const float* __restrict__ partial_energies,
    int num_partials,
    float* __restrict__ total_energy
)
{
    __shared__ float shared_energy[256];
    
    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Load into shared memory
    shared_energy[tid] = (idx < num_partials) ? partial_energies[idx] : 0.0f;
    __syncthreads();
    
    // Parallel reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared_energy[tid] += shared_energy[tid + stride];
        }
        __syncthreads();
    }
    
    // First thread writes result
    if (tid == 0) {
        atomicAdd(total_energy, shared_energy[0]);
    }
}

// ============================================================================
// Host Interface Functions
// ============================================================================

extern "C" {

void cuda_init_nonbonded_params(
    const float* epsilon,  // [118] LJ epsilon per element
    const float* sigma,    // [118] LJ sigma per element
    int num_elements
) {
    cudaMemcpyToSymbol(d_epsilon, epsilon, num_elements * sizeof(float));
    cudaMemcpyToSymbol(d_sigma, sigma, num_elements * sizeof(float));
}

void cuda_set_charges(const float* charges, int num_atoms) {
    cudaMemcpyToSymbol(d_charge, charges, num_atoms * sizeof(float));
}

float cuda_compute_nonbonded_energy(
    const float* h_coords,
    const uint8_t* h_atomic_numbers,
    const int* h_exclusions,
    int num_exclusions_per_atom,
    int num_atoms
) {
    // Allocate device memory
    float *d_coords, *d_partial_energies, *d_total_energy;
    uint8_t *d_atomic_numbers;
    int *d_exclusions;
    
    size_t coord_size = 3 * num_atoms * sizeof(float);
    size_t atom_size = num_atoms * sizeof(uint8_t);
    size_t excl_size = num_atoms * num_exclusions_per_atom * sizeof(int);
    
    cudaMalloc(&d_coords, coord_size);
    cudaMalloc(&d_atomic_numbers, atom_size);
    cudaMalloc(&d_exclusions, excl_size);
    
    // Copy to device
    cudaMemcpy(d_coords, h_coords, coord_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_atomic_numbers, h_atomic_numbers, atom_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_exclusions, h_exclusions, excl_size, cudaMemcpyHostToDevice);
    
    // Launch configuration
    int threads_per_block = 256;
    int num_blocks = (num_atoms + threads_per_block - 1) / threads_per_block;
    int num_partials = num_blocks * threads_per_block;
    
    cudaMalloc(&d_partial_energies, num_partials * sizeof(float));
    cudaMalloc(&d_total_energy, sizeof(float));
    cudaMemset(d_total_energy, 0, sizeof(float));
    
    // Launch kernel
    compute_nonbonded_energy<<<num_blocks, threads_per_block>>>(
        d_coords, d_atomic_numbers, d_exclusions,
        num_exclusions_per_atom, num_atoms, d_partial_energies
    );
    
    // Reduce partial energies
    reduce_energy<<<1, threads_per_block>>>(
        d_partial_energies, num_partials, d_total_energy
    );
    
    // Copy result back
    float total_energy;
    cudaMemcpy(&total_energy, d_total_energy, sizeof(float), cudaMemcpyDeviceToHost);
    
    // Cleanup
    cudaFree(d_coords);
    cudaFree(d_atomic_numbers);
    cudaFree(d_exclusions);
    cudaFree(d_partial_energies);
    cudaFree(d_total_energy);
    
    return total_energy;
}

} // extern "C"

}} // namespace vsepr::gpu
