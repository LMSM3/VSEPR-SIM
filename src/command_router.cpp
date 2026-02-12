/**
 * command_router.cpp
 * ------------------
 * Implementation of bidirectional command routing system.
 */

#include "command_router.hpp"
#include "sim/sim_thread.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <variant>
#include <algorithm>
#include <cctype>

namespace vsepr {

// ============================================================================
// CommandRouter Implementation
// ============================================================================

CommandRouter::CommandRouter(SimulationThread& sim_thread)
    : sim_thread_(sim_thread)
    , next_cmd_id_(1)
    , max_output_history_(1000)
    , next_callback_id_(1)
{
    // Emit initial message
    emit_output(0, ResultStatus::INFO, 
                "Command router initialized. Type 'help' for available commands.", 
                CommandSource::INTERNAL);
}

CommandRouter::~CommandRouter() {
    // Nothing to clean up - callbacks will be destroyed automatically
}

std::string CommandRouter::normalize_input(const std::string& raw) const {
    if (raw.empty()) return "";
    
    std::string result;
    result.reserve(raw.size());
    
    bool in_quotes = false;
    bool last_was_space = true;  // Trim leading spaces
    
    for (char c : raw) {
        if (c == '"') {
            in_quotes = !in_quotes;
            result += c;
            last_was_space = false;
        } else if (std::isspace(c)) {
            if (in_quotes) {
                // Preserve spaces in quotes
                result += c;
            } else if (!last_was_space) {
                // Collapse multiple spaces to one
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }
    
    // Trim trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

void CommandRouter::submit_command(const std::string& command_line, CommandSource source) {
    // Normalize input
    std::string normalized = normalize_input(command_line);
    
    // Ignore empty commands
    if (normalized.empty()) {
        return;
    }
    
    // Assign unique command ID
    uint64_t cmd_id = next_cmd_id_.fetch_add(1, std::memory_order_relaxed);
    
    // Special handling for 'help' command (process locally)
    if (normalized == "help" || normalized.find("help ") == 0) {
        std::string help_arg = normalized.length() > 5 ? normalized.substr(5) : "";
        std::string help_text = parser_.get_help(help_arg);
        emit_output(cmd_id, ResultStatus::INFO, help_text, source);
        return;
    }
    
    // Parse command
    ParseResult result = parser_.parse(normalized);
    
    if (std::holds_alternative<ParseSuccess>(result)) {
        auto& success = std::get<ParseSuccess>(result);
        
        // Create envelope
        CmdEnvelope envelope(cmd_id, source, normalized, success.command);
        
        // Try to enqueue
        if (command_queue_.try_push(envelope)) {
            // Echo the command if not silent
            if (source != CommandSource::IMGUI && envelope.echo_input) {
                std::string echo = std::string("[") + source_name(source) + "] > " + normalized;
                emit_output(cmd_id, ResultStatus::INFO, echo, source);
            }
            
            // Emit acknowledgment
            emit_output(cmd_id, ResultStatus::OK, success.echo, source);
        } else {
            // Queue full
            emit_output(cmd_id, ResultStatus::ERROR, 
                       "Command queue full - simulation thread may be blocked", 
                       source);
        }
        
    } else {
        auto& error = std::get<ParseError>(result);
        
        // Emit error
        emit_output(cmd_id, ResultStatus::ERROR, error.error_message, source);
        
        // Emit suggestion if present
        if (!error.suggestion.empty()) {
            emit_output(cmd_id, ResultStatus::INFO, "Suggestion: " + error.suggestion, source);
        }
    }
}

int CommandRouter::process_results() {
    int count = 0;
    
    // Drain all pending results from SimThread
    while (auto result_opt = result_queue_.try_pop()) {
        const CmdResult& result = *result_opt;
        
        // Emit result text
        emit_output(result.cmd_id, result.status, result.text, CommandSource::INTERNAL);
        
        // Emit stats if present
        if (result.stats) {
            std::ostringstream oss;
            oss << "  Execution time: " 
                << std::fixed << std::setprecision(2)
                << result.stats->exec_time.count() / 1000.0 << " ms";
            
            if (result.stats->iterations > 0) {
                oss << ", iterations: " << result.stats->iterations;
            }
            
            if (result.stats->converged) {
                oss << " [CONVERGED]";
            }
            
            emit_output(result.cmd_id, ResultStatus::INFO, oss.str(), CommandSource::INTERNAL);
        }
        
        // Emit payload data if present
        if (result.payload) {
            for (const auto& [key, value] : result.payload->kv_pairs) {
                std::string msg = "  " + key + ": " + value;
                emit_output(result.cmd_id, ResultStatus::INFO, msg, CommandSource::INTERNAL);
            }
            
            if (result.payload->energy) {
                std::ostringstream oss;
                oss << "  Energy: " << std::fixed << std::setprecision(6) 
                    << *result.payload->energy << " kcal/mol";
                emit_output(result.cmd_id, ResultStatus::INFO, oss.str(), CommandSource::INTERNAL);
            }
        }
        
        count++;
    }
    
    return count;
}

int CommandRouter::register_output_callback(OutputCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    
    int id = next_callback_id_++;
    output_callbacks_.emplace_back(id, callback);
    
    return id;
}

void CommandRouter::unregister_output_callback(int callback_id) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    
    auto it = std::remove_if(output_callbacks_.begin(), output_callbacks_.end(),
                             [callback_id](const auto& pair) { return pair.first == callback_id; });
    output_callbacks_.erase(it, output_callbacks_.end());
}

std::vector<OutputEntry> CommandRouter::get_output_history(size_t max_count) const {
    std::lock_guard<std::mutex> lock(output_mutex_);
    
    size_t start_idx = 0;
    if (output_history_.size() > max_count) {
        start_idx = output_history_.size() - max_count;
    }
    
    return std::vector<OutputEntry>(output_history_.begin() + start_idx, 
                                     output_history_.end());
}

void CommandRouter::clear_output_history() {
    std::lock_guard<std::mutex> lock(output_mutex_);
    output_history_.clear();
}

void CommandRouter::emit_output(uint64_t cmd_id, ResultStatus status, 
                                 const std::string& text, CommandSource source) {
    // Create output entry
    OutputEntry entry(cmd_id, status, text, source);
    
    // Add to history (thread-safe)
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_history_.push_back(entry);
        
        // Limit history size
        while (output_history_.size() > max_output_history_) {
            output_history_.pop_front();
        }
    }
    
    // Call all registered callbacks (thread-safe)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        for (const auto& [id, callback] : output_callbacks_) {
            callback(entry);
        }
    }
}

// ============================================================================
// StdinReader Implementation
// ============================================================================

StdinReader::StdinReader(CommandRouter& router)
    : router_(router)
    , running_(false)
    , should_stop_(false)
    , prompt_enabled_(true)
    , prompt_("vsepr> ")
{
}

StdinReader::~StdinReader() {
    stop();
}

void StdinReader::start() {
    if (running_) {
        return;  // Already running
    }
    
    should_stop_ = false;
    running_ = true;
    
    thread_ = std::make_unique<std::thread>(&StdinReader::read_loop, this);
    
    std::cout << "[StdinReader] Started (background thread)\n";
}

void StdinReader::stop() {
    if (!running_) {
        return;
    }
    
    should_stop_ = true;
    
    // Note: We cannot easily interrupt std::getline, so this may block
    // until the next input line. Consider using platform-specific
    // non-blocking I/O if this is a problem.
    
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    
    running_ = false;
    thread_.reset();
    
    std::cout << "[StdinReader] Stopped\n";
}

void StdinReader::read_loop() {
    std::string line;
    
    while (!should_stop_) {
        // Display prompt if enabled
        if (prompt_enabled_) {
            std::cout << prompt_ << std::flush;
        }
        
        // Blocking read from stdin
        if (!std::getline(std::cin, line)) {
            // EOF or error
            if (std::cin.eof()) {
                std::cout << "\n[StdinReader] EOF detected, stopping...\n";
                break;
            }
            
            // Clear error and continue
            std::cin.clear();
            continue;
        }
        
        // Submit to router
        if (!line.empty()) {
            router_.submit_command(line, CommandSource::STDIN);
        }
    }
    
    std::cout << "[StdinReader] Read loop finished\n";
}

} // namespace vsepr
