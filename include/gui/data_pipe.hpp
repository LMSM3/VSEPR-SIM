/**
 * VSEPR-Sim GUI Data Piping System
 * Reactive data flow for UI components
 * Version: 2.3.1
 */

#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <any>
#include <mutex>
#include <iostream>

namespace vsepr {
namespace gui {

// Forward declarations
template<typename T> class DataPipe;
template<typename T> class DataSource;
template<typename T> class DataSink;

// Data pipe event types
enum class PipeEvent {
    DATA_UPDATED,
    DATA_CLEARED,
    PIPE_CONNECTED,
    PIPE_DISCONNECTED,
    PIPE_ERROR
};

// Base pipe interface
class IPipe {
public:
    virtual ~IPipe() = default;
    virtual std::string name() const = 0;
    virtual std::string type() const = 0;
    virtual bool isConnected() const = 0;
};

// Data pipe - reactive data flow
template<typename T>
class DataPipe : public IPipe {
public:
    using Callback = std::function<void(const T&)>;
    using Transform = std::function<T(const T&)>;
    
    explicit DataPipe(const std::string& name) 
        : name_(name), has_value_(false) {}
    
    // IPipe interface
    std::string name() const override { return name_; }
    std::string type() const override { return typeid(T).name(); }
    bool isConnected() const override { return !subscribers_.empty(); }
    
    // Push data through pipe
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = value;
        has_value_ = true;
        notify(value);
    }
    
    // Subscribe to data updates
    void subscribe(Callback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.push_back(callback);
        
        // Send current value if available
        if (has_value_) {
            callback(value_);
        }
    }
    
    // Transform and pipe to another
    template<typename U>
    std::shared_ptr<DataPipe<U>> transform(
        const std::string& name,
        std::function<U(const T&)> transformer) {
        
        auto output = std::make_shared<DataPipe<U>>(name);
        
        subscribe([output, transformer](const T& value) {
            output->push(transformer(value));
        });
        
        return output;
    }
    
    // Filter data
    std::shared_ptr<DataPipe<T>> filter(
        const std::string& name,
        std::function<bool(const T&)> predicate) {
        
        auto output = std::make_shared<DataPipe<T>>(name);
        
        subscribe([output, predicate](const T& value) {
            if (predicate(value)) {
                output->push(value);
            }
        });
        
        return output;
    }
    
    // Get current value
    bool tryGet(T& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (has_value_) {
            out = value_;
            return true;
        }
        return false;
    }
    
    // Clear pipe
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        has_value_ = false;
        value_ = T{};
    }
    
private:
    void notify(const T& value) {
        for (auto& callback : subscribers_) {
            callback(value);
        }
    }
    
    std::string name_;
    T value_;
    bool has_value_;
    std::vector<Callback> subscribers_;
    mutable std::mutex mutex_;
};

// Pipe network - manages connected pipes
class PipeNetwork {
public:
    static PipeNetwork& instance();
    
    // Register pipe
    template<typename T>
    void registerPipe(const std::string& name, std::shared_ptr<DataPipe<T>> pipe) {
        std::lock_guard<std::mutex> lock(mutex_);
        pipes_[name] = pipe;
    }
    
    // Get pipe by name
    template<typename T>
    std::shared_ptr<DataPipe<T>> getPipe(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pipes_.find(name);
        if (it != pipes_.end()) {
            return std::static_pointer_cast<DataPipe<T>>(it->second);
        }
        return nullptr;
    }
    
    // Connect pipes
    template<typename T>
    void connect(const std::string& source_name, 
                const std::string& sink_name) {
        auto source = getPipe<T>(source_name);
        auto sink = getPipe<T>(sink_name);
        
        if (source && sink) {
            source->subscribe([sink](const T& value) {
                sink->push(value);
            });
        }
    }
    
    // List all pipes
    std::vector<std::string> listPipes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : pipes_) {
            names.push_back(name);
        }
        return names;
    }
    
    // Get pipe info
    struct PipeInfo {
        std::string name;
        std::string type;
        bool connected;
    };
    
    std::vector<PipeInfo> getPipeInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PipeInfo> info;
        for (const auto& [name, pipe] : pipes_) {
            info.push_back({
                pipe->name(),
                pipe->type(),
                pipe->isConnected()
            });
        }
        return info;
    }
    
private:
    PipeNetwork() = default;
    std::map<std::string, std::shared_ptr<IPipe>> pipes_;
    mutable std::mutex mutex_;
};

// Common pipe types for VSEPR-Sim
using MoleculeDataPipe = DataPipe<struct MoleculeData>;
using EnergyDataPipe = DataPipe<double>;
using GeometryDataPipe = DataPipe<std::vector<double>>;
using StatusDataPipe = DataPipe<std::string>;

// Pipe builder for common patterns
class PipeBuilder {
public:
    // Molecule → Energy pipe
    static std::shared_ptr<EnergyDataPipe> 
    moleculeToEnergy(const std::string& name);
    
    // Molecule → Geometry pipe
    static std::shared_ptr<GeometryDataPipe>
    moleculeToGeometry(const std::string& name);
    
    // State → Status pipe
    static std::shared_ptr<StatusDataPipe>
    stateToStatus(const std::string& name);
    
    // Debugging pipe (logs all data)
    template<typename T>
    static std::shared_ptr<DataPipe<T>>
    debugPipe(const std::string& name) {
        auto pipe = std::make_shared<DataPipe<T>>(name);
        pipe->subscribe([name](const T& value) {
            std::cout << "[PIPE:" << name << "] Data updated" << std::endl;
        });
        return pipe;
    }
};

} // namespace gui
} // namespace vsepr
