#pragma once

#include <string>
#include <vector>
#include <memory>

namespace vsepr {
namespace gpu {

enum class Backend {
    CUDA,
    OpenCL,
    CPU_Fallback
};

enum class GPUVendor {
    NVIDIA,
    AMD,
    Intel,
    Unknown
};

struct GPUInfo {
    std::string name;
    GPUVendor vendor;
    size_t memory_bytes;
    int compute_units;
    int max_threads_per_block;
    bool supports_double_precision;
};

class GPUBackend {
public:
    static GPUBackend& instance();
    
    // Device query
    Backend get_backend() const { return backend_; }
    bool is_available() const { return available_; }
    size_t get_device_count() const;
    GPUInfo get_device_info(size_t device_id = 0) const;
    size_t get_memory_available() const;
    std::string get_device_name() const;
    
    // Memory management
    void* allocate(size_t bytes);
    void deallocate(void* ptr);
    void copy_to_device(void* dst, const void* src, size_t bytes);
    void copy_from_device(void* dst, const void* src, size_t bytes);
    
    // Synchronization
    void synchronize();
    
    // Kernel launch (backend-agnostic)
    template<typename... Args>
    void launch_kernel(
        const char* kernel_name,
        size_t num_threads,
        size_t threads_per_block,
        Args&&... args
    );
    
private:
    GPUBackend();
    ~GPUBackend();
    
    void detect_backend();
    void initialize_cuda();
    void initialize_opencl();
    
    Backend backend_;
    bool available_;
    void* device_handle_;
    std::vector<GPUInfo> devices_;
};

// RAII GPU memory wrapper
template<typename T>
class DeviceBuffer {
public:
    DeviceBuffer(size_t count) : count_(count) {
        ptr_ = static_cast<T*>(GPUBackend::instance().allocate(count * sizeof(T)));
    }
    
    ~DeviceBuffer() {
        if (ptr_) {
            GPUBackend::instance().deallocate(ptr_);
        }
    }
    
    // No copy
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    
    // Move
    DeviceBuffer(DeviceBuffer&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
    }
    
    T* data() { return ptr_; }
    const T* data() const { return ptr_; }
    size_t size() const { return count_; }
    
    void upload(const std::vector<T>& host_data) {
        GPUBackend::instance().copy_to_device(ptr_, host_data.data(), count_ * sizeof(T));
    }
    
    void download(std::vector<T>& host_data) {
        host_data.resize(count_);
        GPUBackend::instance().copy_from_device(host_data.data(), ptr_, count_ * sizeof(T));
    }
    
private:
    T* ptr_ = nullptr;
    size_t count_;
};

}} // namespace vsepr::gpu
