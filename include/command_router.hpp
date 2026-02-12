#pragma once
/**
 * command_router.hpp
 * ------------------
 * Thread-safe command routing system with bidirectional communication.
 * 
 * Architecture:
 * - Single authority for all command I/O (text in, structured out)
 * - Bidirectional queues: Router ↔ SimThread
 *   - cmd_q: Router → SimThread (CmdEnvelope)
 *   - res_q: SimThread → Router (CmdResult)
 * - Router normalizes, parses, validates, and assigns cmd_id
 * - SimThread executes and returns structured results
 * - Router routes results to all registered output callbacks
 * 
 * This design provides:
 * - Single authority for command lifecycle
 * - Thread-safe bidirectional communication
 * - Consistent output across all UIs (STDOUT, ImGui, etc.)
 * - Full traceability (cmd_id links commands to results)
 */

#include "vis/command_parser.hpp"
#include "sim/sim_command.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <functional>
#include <chrono>
#include <memory>
#include <atomic>
#include <optional>
#include <thread>

namespace vsepr {

// Forward declare
class SimulationThread;

// ============================================================================
// Command Source Identification
// ============================================================================

enum class CommandSource {
    STDIN,      // Terminal/console input
    IMGUI,      // ImGui console
    SCRIPT,     // Script file
    INTERNAL    // Internal/programmatic
};

inline const char* source_name(CommandSource src) {
    switch (src) {
        case CommandSource::STDIN:    return "stdin";
        case CommandSource::IMGUI:    return "imgui";
        case CommandSource::SCRIPT:   return "script";
        case CommandSource::INTERNAL: return "internal";
        default: return "unknown";
    }
}

// ============================================================================
// Command Envelope (Router → SimThread)
// ============================================================================

/**
 * Wraps a SimCommand with metadata for tracking and routing.
 * This is what actually goes into the command queue.
 */
struct CmdEnvelope {
    uint64_t cmd_id;                                    // Unique command ID
    CommandSource source;                               // Where it came from
    std::string raw_input;                              // Original text (for history)
    std::chrono::system_clock::time_point timestamp;    // When submitted
    SimCommand command;                                 // Parsed command
    
    // Optional flags
    bool silent = false;        // Don't echo to console
    bool echo_input = true;     // Echo the input line
    bool benchmark = false;     // Measure execution time
    
    // Default constructor for std::array
    CmdEnvelope() : cmd_id(0), source(CommandSource::INTERNAL), command(CmdListParams{}) {}
    
    CmdEnvelope(uint64_t id, CommandSource src, const std::string& raw, SimCommand cmd)
        : cmd_id(id)
        , source(src)
        , raw_input(raw)
        , timestamp(std::chrono::system_clock::now())
        , command(std::move(cmd))
    {}
};

// ============================================================================
// Command Result Status
// ============================================================================

enum class ResultStatus {
    OK,         // Command succeeded
    ERROR,      // Command failed
    WARNING,    // Command succeeded with warnings
    INFO        // Informational message (not a command result)
};

inline const char* status_name(ResultStatus status) {
    switch (status) {
        case ResultStatus::OK:      return "OK";
        case ResultStatus::ERROR:   return "ERROR";
        case ResultStatus::WARNING: return "WARNING";
        case ResultStatus::INFO:    return "INFO";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Command Result (SimThread → Router)
// ============================================================================

/**
 * Structured result from command execution.
 * SimThread sends these back to Router for output routing.
 */
struct CmdResult {
    uint64_t cmd_id;            // Matches CmdEnvelope.cmd_id
    ResultStatus status;        // OK / ERROR / WARNING / INFO
    std::string text;           // Human-readable message
    
    // Optional structured data
    struct Payload {
        std::vector<std::pair<std::string, std::string>> kv_pairs;  // Key-value data
        std::optional<double> energy;                               // Energy value
        std::optional<int> iteration_count;                         // Iteration count
        std::optional<double> convergence;                          // Convergence metric
    };
    std::optional<Payload> payload;
    
    // Optional stats
    struct Stats {
        std::chrono::microseconds exec_time;    // Execution time
        int iterations = 0;                     // Number of iterations
        bool converged = false;                 // Convergence flag
        
        // Constructor for aggregate initialization
        Stats(std::chrono::microseconds time, int iter = 0, bool conv = false)
            : exec_time(time), iterations(iter), converged(conv) {}
    };
    std::optional<Stats> stats;
    
    // Default constructor for std::array
    CmdResult() : cmd_id(0), status(ResultStatus::INFO), text("") {}
    
    CmdResult(uint64_t id, ResultStatus st, const std::string& txt)
        : cmd_id(id), status(st), text(txt)
    {}
    
    // Helper constructors
    static CmdResult ok(uint64_t id, const std::string& msg) {
        return CmdResult(id, ResultStatus::OK, msg);
    }
    
    static CmdResult error(uint64_t id, const std::string& msg) {
        return CmdResult(id, ResultStatus::ERROR, msg);
    }
    
    static CmdResult warning(uint64_t id, const std::string& msg) {
        return CmdResult(id, ResultStatus::WARNING, msg);
    }
    
    static CmdResult info(uint64_t id, const std::string& msg) {
        return CmdResult(id, ResultStatus::INFO, msg);
    }
};

// ============================================================================
// Lock-Free SPSC Queues for Command/Result Transport
// ============================================================================

template<typename T, size_t Capacity = 256>
class SPSCQueue {
public:
    SPSCQueue() : head_(0), tail_(0) {}
    
    // Producer: try to enqueue (returns false if queue full)
    bool try_push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Capacity;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Consumer: try to dequeue (returns nullopt if queue empty)
    std::optional<T> try_pop() {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;  // Queue empty
        }
        
        T item = buffer_[current_head];
        head_.store((current_head + 1) % Capacity, std::memory_order_release);
        return item;
    }
    
    // Check if queue is empty (consumer side)
    bool empty() const {
        return head_.load(std::memory_order_relaxed) == 
               tail_.load(std::memory_order_acquire);
    }
    
    // Check if queue is full (producer side)
    bool full() const {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Capacity;
        return next_tail == head_.load(std::memory_order_acquire);
    }
    
private:
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> head_;  // Consumer index
    alignas(64) std::atomic<size_t> tail_;  // Producer index
};

using CommandQueue = SPSCQueue<CmdEnvelope, 256>;
using ResultQueue = SPSCQueue<CmdResult, 256>;

// ============================================================================
// Output Entry (for display in UI/terminal)
// ============================================================================

struct OutputEntry {
    uint64_t cmd_id;        // Associated command ID (0 for system messages)
    ResultStatus status;    // Message level
    std::string text;       // Display text
    CommandSource source;   // Where the command came from
    std::chrono::system_clock::time_point timestamp;
    
    OutputEntry(uint64_t id, ResultStatus st, const std::string& txt, CommandSource src = CommandSource::INTERNAL)
        : cmd_id(id)
        , status(st)
        , text(txt)
        , source(src)
        , timestamp(std::chrono::system_clock::now())
    {}
};

// ============================================================================
// Command Router
// ============================================================================

/**
 * Single authority for all command I/O.
 * 
 * Responsibilities:
 * - Normalize and parse text input
 * - Validate and type-check arguments
 * - Assign unique cmd_id to each command
 * - Enqueue CmdEnvelope to SimThread
 * - Consume CmdResult from SimThread
 * - Route results to all registered output callbacks
 * - Maintain history of commands and results
 */
class CommandRouter {
public:
    using OutputCallback = std::function<void(const OutputEntry&)>;
    
    CommandRouter(SimulationThread& sim_thread);
    ~CommandRouter();
    
    /**
     * Submit a command for execution.
     * Thread-safe - can be called from any thread.
     * 
     * Flow:
     * 1. Normalize input (trim, collapse whitespace)
     * 2. Parse into SimCommand
     * 3. Validate arguments
     * 4. Create CmdEnvelope with unique cmd_id
     * 5. Enqueue to SimThread
     */
    void submit_command(const std::string& command_line, CommandSource source);
    
    /**
     * Process pending results from SimThread.
     * Call this from the main/render thread each frame.
     * Returns number of results processed.
     */
    int process_results();
    
    /**
     * Get the command queue for SimThread to consume.
     * SimThread should call try_pop() on this.
     */
    CommandQueue& command_queue() { return command_queue_; }
    
    /**
     * Get the result queue for SimThread to produce into.
     * SimThread should call try_push() on this.
     */
    ResultQueue& result_queue() { return result_queue_; }
    
    /**
     * Register an output callback.
     * All output will be sent to registered callbacks.
     * Returns a callback ID for unregistering.
     */
    int register_output_callback(OutputCallback callback);
    
    /**
     * Unregister an output callback.
     */
    void unregister_output_callback(int callback_id);
    
    /**
     * Get command parser (for help, completions, etc.)
     */
    CommandParser& parser() { return parser_; }
    const CommandParser& parser() const { return parser_; }
    
    /**
     * Get output history (for ImGui console display).
     * Thread-safe.
     */
    std::vector<OutputEntry> get_output_history(size_t max_count = 1000) const;
    
    /**
     * Clear output history.
     */
    void clear_output_history();
    
private:
    // Normalize input text (trim, collapse whitespace, handle quotes)
    std::string normalize_input(const std::string& raw) const;
    
    // Emit output to all registered callbacks
    void emit_output(uint64_t cmd_id, ResultStatus status, const std::string& text, CommandSource source);
    
    // Command parser
    CommandParser parser_;
    
    // Simulation thread (for direct command submission - deprecated path)
    SimulationThread& sim_thread_;
    
    // Command ID counter (atomic for thread safety)
    std::atomic<uint64_t> next_cmd_id_;
    
    // Bidirectional queues
    CommandQueue command_queue_;    // Router → SimThread
    ResultQueue result_queue_;      // SimThread → Router
    
    // Output history (thread-safe)
    mutable std::mutex output_mutex_;
    std::deque<OutputEntry> output_history_;
    size_t max_output_history_;
    
    // Output callbacks (thread-safe)
    mutable std::mutex callback_mutex_;
    std::vector<std::pair<int, OutputCallback>> output_callbacks_;
    int next_callback_id_;
};

// ============================================================================
// STDIN Reader Thread
// ============================================================================

/**
 * Background thread that reads from STDIN and submits to CommandRouter.
 * 
 * This allows the main render thread to remain responsive while
 * blocking on console input.
 */
class StdinReader {
public:
    StdinReader(CommandRouter& router);
    ~StdinReader();
    
    /**
     * Start reading from STDIN in background thread.
     */
    void start();
    
    /**
     * Stop reading and shutdown thread.
     */
    void stop();
    
    /**
     * Check if reader is running.
     */
    bool is_running() const { return running_; }
    
    /**
     * Enable/disable prompt display.
     */
    void set_prompt_enabled(bool enabled) { prompt_enabled_ = enabled; }
    void set_prompt(const std::string& prompt) { prompt_ = prompt; }
    
private:
    // Main reader loop
    void read_loop();
    
    // Command router
    CommandRouter& router_;
    
    // Thread control
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    std::unique_ptr<std::thread> thread_;
    
    // Prompt settings
    std::atomic<bool> prompt_enabled_;
    std::string prompt_;
};

} // namespace vsepr
