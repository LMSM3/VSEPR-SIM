#include "gpu/gpu_backend.hpp"
#include <iostream>
#include <cstring>

#ifdef VSEPR_HAS_CUDA
#include <cuda_runtime.h>
#endif

#ifdef VSEPR_HAS_OPENCL
#include <CL/cl.h>
#endif

namespace vsepr {
namespace gpu {

GPUBackend& GPUBackend::instance() {
    static GPUBackend inst;
    return inst;
}

GPUBackend::GPUBackend() : backend_(Backend::CPU_Fallback), available_(false), device_handle_(nullptr) {
    detect_backend();
}

GPUBackend::~GPUBackend() {
    // Cleanup based on backend
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        cudaDeviceReset();
    }
    #endif
}

void GPUBackend::detect_backend() {
    // Try CUDA first
    #ifdef VSEPR_HAS_CUDA
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err == cudaSuccess && device_count > 0) {
        initialize_cuda();
        return;
    }
    #endif
    
    // Try OpenCL
    #ifdef VSEPR_HAS_OPENCL
    cl_uint num_platforms = 0;
    if (clGetPlatformIDs(0, nullptr, &num_platforms) == CL_SUCCESS && num_platforms > 0) {
        initialize_opencl();
        return;
    }
    #endif
    
    // Fallback to CPU
    std::cout << "[GPU] No GPU detected, using CPU fallback\n";
    backend_ = Backend::CPU_Fallback;
    available_ = false;
}

void GPUBackend::initialize_cuda() {
    #ifdef VSEPR_HAS_CUDA
    int device_count;
    cudaGetDeviceCount(&device_count);
    
    devices_.resize(device_count);
    for (int i = 0; i < device_count; ++i) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        
        devices_[i].name = prop.name;
        devices_[i].vendor = GPUVendor::NVIDIA;
        devices_[i].memory_bytes = prop.totalGlobalMem;
        devices_[i].compute_units = prop.multiProcessorCount;
        devices_[i].max_threads_per_block = prop.maxThreadsPerBlock;
        devices_[i].supports_double_precision = prop.major >= 6;
    }
    
    cudaSetDevice(0);
    backend_ = Backend::CUDA;
    available_ = true;
    
    std::cout << "[GPU] CUDA backend initialized\n";
    std::cout << "[GPU] Device: " << devices_[0].name << "\n";
    std::cout << "[GPU] Memory: " << (devices_[0].memory_bytes / 1e9) << " GB\n";
    #endif
}

void GPUBackend::initialize_opencl() {
    #ifdef VSEPR_HAS_OPENCL
    // OpenCL initialization (simplified)
    backend_ = Backend::OpenCL;
    available_ = true;
    std::cout << "[GPU] OpenCL backend initialized\n";
    #endif
}

size_t GPUBackend::get_device_count() const {
    return devices_.size();
}

GPUInfo GPUBackend::get_device_info(size_t device_id) const {
    if (device_id < devices_.size()) {
        return devices_[device_id];
    }
    return GPUInfo{};
}

size_t GPUBackend::get_memory_available() const {
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        size_t free_bytes, total_bytes;
        cudaMemGetInfo(&free_bytes, &total_bytes);
        return free_bytes;
    }
    #endif
    return 0;
}

std::string GPUBackend::get_device_name() const {
    if (!devices_.empty()) {
        return devices_[0].name;
    }
    return "CPU Fallback";
}

void* GPUBackend::allocate(size_t bytes) {
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        void* ptr;
        cudaError_t err = cudaMalloc(&ptr, bytes);
        if (err != cudaSuccess) {
            std::cerr << "[GPU] Allocation failed: " << cudaGetErrorString(err) << "\n";
            return nullptr;
        }
        return ptr;
    }
    #endif
    
    // CPU fallback
    return malloc(bytes);
}

void GPUBackend::deallocate(void* ptr) {
    if (!ptr) return;
    
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        cudaFree(ptr);
        return;
    }
    #endif
    
    free(ptr);
}

void GPUBackend::copy_to_device(void* dst, const void* src, size_t bytes) {
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        cudaMemcpy(dst, src, bytes, cudaMemcpyHostToDevice);
        return;
    }
    #endif
    
    // CPU fallback
    memcpy(dst, src, bytes);
}

void GPUBackend::copy_from_device(void* dst, const void* src, size_t bytes) {
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        cudaMemcpy(dst, src, bytes, cudaMemcpyDeviceToHost);
        return;
    }
    #endif
    
    // CPU fallback
    memcpy(dst, src, bytes);
}

void GPUBackend::synchronize() {
    #ifdef VSEPR_HAS_CUDA
    if (backend_ == Backend::CUDA) {
        cudaDeviceSynchronize();
    }
    #endif
}

}} // namespace vsepr::gpu
