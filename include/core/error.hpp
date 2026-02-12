/**
 * Error Handling Framework
 * 
 * Production-quality error handling for VSEPR-Sim
 * Replaces exception-throwing with structured error returns
 */

#pragma once

#include <string>
#include <variant>
#include <optional>
#include <stdexcept>

namespace vsepr {

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    // Success
    OK = 0,
    
    // File I/O errors
    FILE_NOT_FOUND = 100,
    FILE_CANNOT_OPEN,
    FILE_INVALID_FORMAT,
    FILE_CORRUPTED,
    FILE_WRITE_FAILED,
    
    // Parsing errors
    PARSE_INVALID_NUMBER = 200,
    PARSE_INVALID_ELEMENT,
    PARSE_UNEXPECTED_EOF,
    PARSE_MISSING_FIELD,
    PARSE_INVALID_ATOM_COUNT,
    
    // Chemistry errors
    CHEMISTRY_INVALID_ELEMENT = 300,
    CHEMISTRY_INVALID_BOND,
    CHEMISTRY_ATOMS_TOO_CLOSE,
    CHEMISTRY_UNREASONABLE_GEOMETRY,
    CHEMISTRY_INVALID_VALENCE,
    
    // Thermal errors
    THERMAL_INVALID_PATHWAY = 400,
    THERMAL_UNSTABLE_SIMULATION,
    THERMAL_ENERGY_OVERFLOW,
    
    // General errors
    INVALID_ARGUMENT = 500,
    OUT_OF_RANGE,
    NOT_IMPLEMENTED,
    INTERNAL_ERROR
};

// ============================================================================
// Error Context
// ============================================================================

struct ErrorContext {
    ErrorCode code;
    std::string message;
    std::string file;         // Source file where error occurred
    int line = 0;             // Line number
    std::string detail;       // Additional context (e.g., filename, atom index)
    
    ErrorContext(ErrorCode c, const std::string& msg)
        : code(c), message(msg) {}
    
    ErrorContext(ErrorCode c, const std::string& msg, const std::string& det)
        : code(c), message(msg), detail(det) {}
    
    // Human-readable error message
    std::string to_string() const {
        std::string result = "[Error " + std::to_string(static_cast<int>(code)) + "] " + message;
        if (!detail.empty()) {
            result += " (" + detail + ")";
        }
        if (!file.empty() && line > 0) {
            result += " at " + file + ":" + std::to_string(line);
        }
        return result;
    }
    
    bool is_ok() const { return code == ErrorCode::OK; }
};

// ============================================================================
// Result<T> - Either success value or error
// ============================================================================

template<typename T>
class Result {
public:
    // Success constructor
    static Result ok(T value) {
        Result r;
        r.data_ = std::move(value);
        return r;
    }
    
    // Error constructor
    static Result error(ErrorContext err) {
        Result r;
        r.data_ = std::move(err);
        return r;
    }
    
    static Result error(ErrorCode code, const std::string& msg) {
        return error(ErrorContext(code, msg));
    }
    
    static Result error(ErrorCode code, const std::string& msg, const std::string& detail) {
        return error(ErrorContext(code, msg, detail));
    }
    
    // Check if result is success
    bool is_ok() const {
        return std::holds_alternative<T>(data_);
    }
    
    bool is_error() const {
        return std::holds_alternative<ErrorContext>(data_);
    }
    
    // Get value (undefined behavior if error)
    T& value() {
        return std::get<T>(data_);
    }
    
    const T& value() const {
        return std::get<T>(data_);
    }
    
    // Get error (undefined behavior if success)
    const ErrorContext& error() const {
        return std::get<ErrorContext>(data_);
    }
    
    // Safe access with fallback
    T value_or(T default_value) const {
        return is_ok() ? value() : default_value;
    }
    
    // Unwrap (returns value or throws - use sparingly)
    T unwrap() const {
        if (is_error()) {
            throw std::runtime_error(error().to_string());
        }
        return value();
    }
    
private:
    std::variant<T, ErrorContext> data_;
    Result() = default;
};

// ============================================================================
// Status - Error-only result (no value)
// ============================================================================

class Status {
public:
    static Status ok() {
        return Status(ErrorCode::OK, "");
    }
    
    static Status error(ErrorContext err) {
        return Status(std::move(err));
    }
    
    static Status error(ErrorCode code, const std::string& msg) {
        return Status(ErrorContext(code, msg));
    }
    
    static Status error(ErrorCode code, const std::string& msg, const std::string& detail) {
        return Status(ErrorContext(code, msg, detail));
    }
    
    bool is_ok() const {
        return !error_.has_value();
    }
    
    bool is_error() const {
        return error_.has_value();
    }
    
    const ErrorContext& error() const {
        return *error_;
    }
    
    std::string message() const {
        return error_.has_value() ? error_->to_string() : "OK";
    }
    
private:
    std::optional<ErrorContext> error_;
    
    Status(ErrorCode code, const std::string& msg) {
        if (code != ErrorCode::OK) {
            error_ = ErrorContext(code, msg);
        }
    }
    
    Status(ErrorContext err) : error_(std::move(err)) {}
};

// ============================================================================
// Diagnostics Channel
// ============================================================================

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class DiagnosticsChannel {
public:
    using LogCallback = void(*)(LogLevel, const std::string&);
    
    static DiagnosticsChannel& instance() {
        static DiagnosticsChannel instance;
        return instance;
    }
    
    void set_callback(LogCallback callback) {
        callback_ = callback;
    }
    
    void log(LogLevel level, const std::string& message) {
        if (callback_) {
            callback_(level, message);
        }
    }
    
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warning(const std::string& msg) { log(LogLevel::WARNING, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void critical(const std::string& msg) { log(LogLevel::CRITICAL, msg); }
    
private:
    LogCallback callback_ = nullptr;
    DiagnosticsChannel() = default;
};

// Convenience macros
#define VSEPR_LOG_DEBUG(msg) vsepr::DiagnosticsChannel::instance().debug(msg)
#define VSEPR_LOG_INFO(msg) vsepr::DiagnosticsChannel::instance().info(msg)
#define VSEPR_LOG_WARNING(msg) vsepr::DiagnosticsChannel::instance().warning(msg)
#define VSEPR_LOG_ERROR(msg) vsepr::DiagnosticsChannel::instance().error(msg)

} // namespace vsepr
