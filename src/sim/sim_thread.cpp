/**
 * sim_thread.cpp
 * --------------
 * Implementation of simulation thread manager.
 */

#include "sim_thread.hpp"
#include "../../include/command_router.hpp"
#include "molecule.hpp"
#include "vsepr/formula_parser.hpp"
#include "pot/periodic_db.hpp"
#include "graph_builder.hpp"
#include "core/element_data.hpp"
#include <iostream>
#include <chrono>
#include <variant>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

namespace vsepr {

namespace {

std::optional<double> numeric_parameter_value(const ParamValue& value) {
    if (const auto* number = std::get_if<double>(&value)) {
        return *number;
    }
    if (const auto* integer = std::get_if<int>(&value)) {
        return static_cast<double>(*integer);
    }
    return std::nullopt;
}

} // namespace

// ============================================================================
// Construction & Lifecycle
// ============================================================================

SimulationThread::SimulationThread()
    : running_(false)
    , should_stop_(false)
    , command_router_(nullptr)
    , frame_counter_(0)
{
    sim_state_ = std::make_unique<SimulationState>();
    
    // Load periodic table for dynamic formula parsing
    try {
        ptable_ = PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        init_chemistry_db(&ptable_);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load periodic table: " << e.what() << std::endl;
    }
}

SimulationThread::~SimulationThread() {
    stop();
}

void SimulationThread::set_command_router(CommandRouter* router) {
    command_router_ = router;
}

void SimulationThread::start() {
    if (running_) {
        return;  // Already running
    }
    
    if (!command_router_) {
        std::cerr << "[SimThread] ERROR: CommandRouter not set - call set_command_router() first!\n";
        return;
    }
    
    should_stop_ = false;
    running_ = true;
    
    thread_ = std::make_unique<std::thread>(&SimulationThread::run, this);
    
    std::cout << "[SimThread] Started\n";
}

void SimulationThread::stop() {
    if (!running_) {
        return;
    }
    
    should_stop_ = true;
    
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    
    running_ = false;
    thread_.reset();
    
    std::cout << "[SimThread] Stopped\n";
}

// ============================================================================
// Command Interface
// ============================================================================

FrameSnapshot SimulationThread::get_latest_frame() {
    return frame_buffer_.read();
}

FramePacket SimulationThread::get_latest_frame_packet() {
    return frame_buffer_.read_packet();
}

void SimulationThread::send_result(const CmdResult& result) {
    if (command_router_) {
        if (!command_router_->result_queue().try_push(result)) {
            std::cerr << "[SimThread] WARNING: Result queue full, dropping result\n";
        }
    }
}

// ============================================================================
// Main Simulation Loop
// ============================================================================

void SimulationThread::run() {
    std::cout << "[SimThread] Main loop started\n";
    reset_live_clock();
    publish_frame();

    auto last_wall_time = std::chrono::steady_clock::now();
    
    while (!should_stop_) {
        const auto now = std::chrono::steady_clock::now();
        const double wall_seconds = std::clamp(
            std::chrono::duration<double>(now - last_wall_time).count(),
            0.0, LiveSimulationClock::kMaximumFrameSeconds);
        last_wall_time = now;
        live_real_time_seconds_ += wall_seconds;

        process_commands();

        if (sim_state_ && sim_state_->is_running() && !sim_state_->is_paused()) {
            // Tick throughput is an active-simulation metric. Paused and
            // bootstrap wall time remains visible through real_time_seconds.
            tick_rate_window_seconds_ += wall_seconds;
            const int due_ticks = live_clock_.consume_elapsed(wall_seconds);
            for (int tick = 0; tick < due_ticks; ++tick) {
                if (!advance_live_tick()) {
                    break;
                }
            }
        }

        if (tick_rate_window_seconds_ >= 0.25) {
            live_ticks_per_second_ = static_cast<double>(ticks_in_rate_window_) /
                tick_rate_window_seconds_;
            tick_rate_window_seconds_ = 0.0;
            ticks_in_rate_window_ = 0;
        }

        // The renderer is never throttled here.  This short sleep only keeps
        // the worker from busy-spinning between fixed simulation ticks.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "[SimThread] Main loop finished\n";
}

void SimulationThread::reset_live_clock() {
    live_clock_.reset();
    live_real_time_seconds_ = 0.0;
    tick_rate_window_seconds_ = 0.0;
    live_ticks_per_second_ = 0.0;
    ticks_in_rate_window_ = 0;
    scheduled_run_steps_ = -1;
}

bool SimulationThread::advance_live_tick() {
    if (!sim_state_ || !sim_state_->is_running() || sim_state_->is_paused()) {
        return false;
    }

    sim_state_->step();
    live_clock_.complete_tick();
    ++frame_counter_;
    ++ticks_in_rate_window_;

    if (scheduled_run_steps_ > 0) {
        --scheduled_run_steps_;
    }

    const bool optimization_mode = sim_state_->mode() != SimMode::MD;
    const bool converged = optimization_mode && sim_state_->molecule().num_atoms() > 0 &&
        sim_state_->is_converged();
    const bool iteration_limit = sim_state_->stats().iteration >= sim_state_->params().max_iterations;

    if (converged || (scheduled_run_steps_ == 0 && scheduled_run_steps_ != -1)) {
        sim_state_->pause();
    } else if (iteration_limit) {
        sim_state_->stop();
    }

    // Publish every completed simulation tick.  The renderer may consume a
    // subset, but it can always distinguish a new physical state by sequence.
    publish_frame();
    return sim_state_->is_running() && !sim_state_->is_paused();
}

void SimulationThread::process_commands() {
    if (!command_router_) return;
    
    // Drain all pending command envelopes from CommandRouter
    while (auto env_opt = command_router_->command_queue().try_pop()) {
        handle_envelope(*env_opt);
    }
}

void SimulationThread::handle_envelope(const CmdEnvelope& envelope) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::visit([this, &envelope, &start_time](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        // Session / Mode commands
        if constexpr (std::is_same_v<T, CmdSetMode>) {
            const char* mode_names[] = {"IDLE", "VSEPR", "OPTIMIZE", "MD", "CRYSTAL"};
            int mode_idx = static_cast<int>(arg.mode);
            std::cout << "[SimThread] Set mode: " << mode_names[mode_idx] << "\n";
            if (sim_state_) {
                sim_state_->set_mode(arg.mode);
                publish_frame();
                
                // Send success result
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    "Mode set to " + std::string(mode_names[mode_idx]));
                result.stats = CmdResult::Stats{exec_ms, 0, false};
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdReset>) {
            std::cout << "[SimThread] Reset: " << arg.config_id << "\n";
            if (sim_state_) {
                sim_state_->reset();
                reset_live_clock();
                publish_frame();
                
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, "Simulation reset");
                result.stats = CmdResult::Stats{exec_ms, 0, false};
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdShutdown>) {
            std::cout << "[SimThread] Shutdown requested\n";
            should_stop_ = true;
            
            CmdResult result = CmdResult::ok(envelope.cmd_id, "Shutting down simulation thread");
            send_result(result);
        }
        
        // I/O commands
        else if constexpr (std::is_same_v<T, CmdLoad>) {
            std::cout << "[SimThread] Load: " << arg.filepath << "\n";
            if (sim_state_) {
                if (sim_state_->load_from_file(arg.filepath)) {
                    reset_live_clock();
                    publish_frame();
                    
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                    
                    CmdResult result = CmdResult::ok(envelope.cmd_id, 
                        "Loaded molecule from " + arg.filepath);
                    result.stats = CmdResult::Stats{exec_ms, 0, false};
                    send_result(result);
                } else {
                    CmdResult result = CmdResult::error(envelope.cmd_id, 
                        "Failed to load file: " + arg.filepath);
                    send_result(result);
                }
            }
        }
        else if constexpr (std::is_same_v<T, CmdSave>) {
            std::cout << "[SimThread] Save: " << arg.filepath 
                      << (arg.snapshot ? " (snapshot)" : "") << "\n";
            if (sim_state_) {
                sim_state_->save_to_file(arg.filepath);
                
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    "Saved to " + arg.filepath);
                result.stats = CmdResult::Stats{exec_ms, 0, false};
                send_result(result);
            }
        }
        
        // Build system commands
        else if constexpr (std::is_same_v<T, CmdInitMolecule>) {
            std::cout << "[SimThread] Init molecule: " << arg.atomic_numbers.size() << " atoms\n";
            if (sim_state_) {
                Molecule mol;
                for (size_t i = 0; i < arg.atomic_numbers.size(); ++i) {
                    mol.add_atom(arg.atomic_numbers[i],
                                arg.coords[3*i],
                                arg.coords[3*i+1],
                                arg.coords[3*i+2]);
                }
                for (const auto& bond : arg.bonds) {
                    mol.add_bond(bond.first, bond.second, 1);
                }
                mol.generate_angles_from_bonds();
                sim_state_->initialize(mol);
                reset_live_clock();
                publish_frame();
                
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    "Initialized molecule with " + std::to_string(arg.atomic_numbers.size()) + " atoms");
                result.stats = CmdResult::Stats{exec_ms, 0, false};
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdSpawn>) {
            const char* type_names[] = {"GAS", "CRYSTAL", "LATTICE"};
            std::cout << "[SimThread] Spawn: " << type_names[static_cast<int>(arg.type)] 
                      << " n=" << arg.n_particles << " box=" << arg.box_x << "\n";
            if (sim_state_) {
                sim_state_->spawn_particles(arg);
                reset_live_clock();
                publish_frame();
                
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    "Spawned " + std::to_string(arg.n_particles) + " particles");
                result.stats = CmdResult::Stats{exec_ms, 0, false};
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdBuild>) {
            std::cout << "[SimThread] Build: " << arg.formula << "\n";
            if (sim_state_) {
                try {
                    std::string formula = arg.formula;
                    std::string formula_lower = formula;
                    for (char& c : formula_lower) c = std::tolower(c);
                    
                    // Check if user is confirming a pending formula build
                    if ((formula_lower == "yes" || formula_lower == "y") && !pending_formula_.empty()) {
                        std::cout << "[SimThread] User confirmed - building " << pending_formula_ << "\n";
                        bool success = build_from_composition(pending_composition_, pending_formula_, envelope.cmd_id);
                        
                        // Clear pending state
                        pending_formula_.clear();
                        pending_composition_.clear();
                        pending_cmd_id_ = 0;
                        
                        return;  // build_from_composition already sends result
                    }
                    
                    // Check if user is declining a pending formula build
                    if ((formula_lower == "no" || formula_lower == "n") && !pending_formula_.empty()) {
                        std::cout << "[SimThread] User declined - canceling build of " << pending_formula_ << "\n";
                        
                        CmdResult result = CmdResult::info(envelope.cmd_id,
                            "Canceled build of " + pending_formula_);
                        send_result(result);
                        
                        // Clear pending state
                        pending_formula_.clear();
                        pending_composition_.clear();
                        pending_cmd_id_ = 0;
                        
                        return;
                    }
                    
                    // Simple formula-based molecule builder
                    Molecule mol;
                    
                    // Simple molecule templates (hard-coded for now - can be extended)
                    if (formula_lower == "h2o" || formula_lower == "water") {
                        mol.add_atom(8, 0.0, 0.0, 0.0);       // O
                        mol.add_atom(1, 0.96, 0.0, 0.0);      // H
                        mol.add_atom(1, -0.24, 0.93, 0.0);    // H
                        mol.add_bond(0, 1, 1);
                        mol.add_bond(0, 2, 1);
                    }
                    else if (formula_lower == "ch4" || formula_lower == "methane") {
                        mol.add_atom(6, 0.0, 0.0, 0.0);       // C
                        mol.add_atom(1, 0.63, 0.63, 0.63);    // H
                        mol.add_atom(1, -0.63, -0.63, 0.63);  // H
                        mol.add_atom(1, -0.63, 0.63, -0.63);  // H
                        mol.add_atom(1, 0.63, -0.63, -0.63);  // H
                        mol.add_bond(0, 1, 1);
                        mol.add_bond(0, 2, 1);
                        mol.add_bond(0, 3, 1);
                        mol.add_bond(0, 4, 1);
                    }
                    else if (formula_lower == "nh3" || formula_lower == "ammonia") {
                        mol.add_atom(7, 0.0, 0.0, 0.1);       // N
                        mol.add_atom(1, 0.94, 0.0, -0.3);     // H
                        mol.add_atom(1, -0.47, 0.81, -0.3);   // H
                        mol.add_atom(1, -0.47, -0.81, -0.3);  // H
                        mol.add_bond(0, 1, 1);
                        mol.add_bond(0, 2, 1);
                        mol.add_bond(0, 3, 1);
                    }
                    else if (formula_lower == "co2") {
                        mol.add_atom(6, 0.0, 0.0, 0.0);       // C
                        mol.add_atom(8, 1.16, 0.0, 0.0);      // O
                        mol.add_atom(8, -1.16, 0.0, 0.0);     // O
                        mol.add_bond(0, 1, 2);  // Double bond
                        mol.add_bond(0, 2, 2);
                    }
                    else if (formula_lower == "h2s") {
                        mol.add_atom(16, 0.0, 0.0, 0.0);      // S
                        mol.add_atom(1, 0.97, 0.0, 0.0);      // H
                        mol.add_atom(1, -0.33, 0.91, 0.0);    // H
                        mol.add_bond(0, 1, 1);
                        mol.add_bond(0, 2, 1);
                    }
                    else if (formula_lower == "sf6") {
                        mol.add_atom(16, 0.0, 0.0, 0.0);      // S
                        mol.add_atom(9, 1.56, 0.0, 0.0);      // F
                        mol.add_atom(9, -1.56, 0.0, 0.0);     // F
                        mol.add_atom(9, 0.0, 1.56, 0.0);      // F
                        mol.add_atom(9, 0.0, -1.56, 0.0);     // F
                        mol.add_atom(9, 0.0, 0.0, 1.56);      // F
                        mol.add_atom(9, 0.0, 0.0, -1.56);     // F
                        for (int i = 1; i <= 6; ++i) mol.add_bond(0, i, 1);
                    }
                    else if (formula_lower == "pcl5") {
                        mol.add_atom(15, 0.0, 0.0, 0.0);      // P
                        mol.add_atom(17, 1.8, 0.0, 0.0);      // Cl axial
                        mol.add_atom(17, -1.8, 0.0, 0.0);     // Cl axial
                        mol.add_atom(17, 0.0, 2.0, 0.0);      // Cl equatorial
                        mol.add_atom(17, 0.0, -1.0, 1.73);    // Cl equatorial
                        mol.add_atom(17, 0.0, -1.0, -1.73);   // Cl equatorial
                        for (int i = 1; i <= 5; ++i) mol.add_bond(0, i, 1);
                    }
                    else if (formula_lower == "xef4") {
                        mol.add_atom(54, 0.0, 0.0, 0.0);      // Xe
                        mol.add_atom(9, 1.95, 0.0, 0.0);      // F
                        mol.add_atom(9, -1.95, 0.0, 0.0);     // F
                        mol.add_atom(9, 0.0, 1.95, 0.0);      // F
                        mol.add_atom(9, 0.0, -1.95, 0.0);     // F
                        for (int i = 1; i <= 4; ++i) mol.add_bond(0, i, 1);
                    }
                    else {
                        // Try to parse as a dynamic formula using the formula parser
                        try {
                            vsepr::formula::Composition composition = vsepr::formula::parse(formula, ptable_);
                            
                            // Ask user for confirmation before creating
                            std::string confirm_msg = "Would you like to FIRE up a new molecule '" + formula + "'? (Type 'yes' to confirm)";
                            
                            // Store pending build request
                            pending_formula_ = formula;
                            pending_composition_ = composition;
                            pending_cmd_id_ = envelope.cmd_id;
                            
                            CmdResult result = CmdResult::info(envelope.cmd_id, confirm_msg);
                            send_result(result);
                            return;
                            
                        } catch (const vsepr::formula::ParseError& e) {
                            // Add to recently attempted formulas for defaults suggestion
                            add_to_custom_defaults(formula);
                            
                            std::string default_list = get_defaults_list();
                            CmdResult result = CmdResult::error(envelope.cmd_id, 
                                "Unknown formula: " + formula + " (supported: " + default_list + ")\n" +
                                "Parse error: " + std::string(e.what()));
                            send_result(result);
                            return;
                        }
                    }
                    
                    // Generate angles from bonds
                    mol.generate_angles_from_bonds();
                    
                    // Initialize simulation with new molecule
                    sim_state_->initialize(mol);
                    reset_live_clock();
                    publish_frame();
                    
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                    
                    CmdResult result = CmdResult::ok(envelope.cmd_id, 
                        "Built " + formula + " with " + std::to_string(mol.num_atoms()) + " atoms");
                    result.stats = CmdResult::Stats{exec_ms, 0, false};
                    send_result(result);
                } catch (const std::exception& e) {
                    CmdResult result = CmdResult::error(envelope.cmd_id, 
                        "Failed to build " + arg.formula + ": " + e.what());
                    send_result(result);
                }
            }
        }
        
        // Parameter commands (path-based)
        else if constexpr (std::is_same_v<T, CmdSet>) {
            std::cout << "[SimThread] Set: " << arg.path << "\n";
            if (sim_state_) {
                if (arg.path == "live.speed") {
                    const auto speed = numeric_parameter_value(arg.value);
                    if (!speed || !live_clock_.set_speed_multiplier(*speed)) {
                        CmdResult result = CmdResult::error(envelope.cmd_id,
                            "live.speed must be one of: 0.1, 0.5, 1, 2, 10");
                        send_result(result);
                        return;
                    }
                    publish_frame();
                    CmdResult result = CmdResult::ok(envelope.cmd_id,
                        "Live speed set to " + std::to_string(*speed) + "x");
                    send_result(result);
                    return;
                }
                sim_state_->set_param(arg.path, arg.value);
                
                // Convert ParamValue to string using std::visit
                std::string value_str = std::visit([](auto&& val) -> std::string {
                    using V = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<V, std::string>) {
                        return val;
                    } else if constexpr (std::is_same_v<V, bool>) {
                        return val ? "true" : "false";
                    } else {
                        return std::to_string(val);
                    }
                }, arg.value);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    "Set " + arg.path + " = " + value_str);
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdGet>) {
            std::cout << "[SimThread] Get: " << arg.path << "\n";
            // TODO: Actually get value from sim_state
            CmdResult result = CmdResult::info(envelope.cmd_id, 
                "Get parameter (not yet implemented): " + arg.path);
            send_result(result);
        }
        else if constexpr (std::is_same_v<T, CmdListParams>) {
            std::cout << "[SimThread] List params: " << arg.prefix << "\n";
            CmdResult result = CmdResult::info(envelope.cmd_id, 
                "List params (not yet implemented): " + arg.prefix);
            send_result(result);
        }
        
        // Runtime control commands
        else if constexpr (std::is_same_v<T, CmdPause>) {
            std::cout << "[SimThread] Pause\n";
            if (sim_state_) {
                scheduled_run_steps_ = -1;
                sim_state_->pause();
                publish_frame();
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, "Paused");
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdResume>) {
            std::cout << "[SimThread] Resume\n";
            if (sim_state_) {
                scheduled_run_steps_ = -1;
                std::cout << "[SimThread]   - Num atoms: " << sim_state_->molecule().num_atoms() << "\n";
                std::cout << "[SimThread]   - Current mode: " << static_cast<int>(sim_state_->mode()) << "\n";
                sim_state_->resume();
                std::cout << "[SimThread]   - Now running: " << sim_state_->is_running() << "\n";
                std::cout << "[SimThread]   - Paused: " << sim_state_->is_paused() << "\n";
                publish_frame();
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, "Resumed");
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdSingleStep>) {
            std::cout << "[SimThread] Single step: " << arg.n_steps << "\n";
            if (sim_state_) {
                scheduled_run_steps_ = -1;
                int completed = 0;
                for (int step = 0; step < arg.n_steps; ++step) {
                    sim_state_->resume();
                    const bool still_running = advance_live_tick();
                    ++completed;
                    if (!still_running) {
                        break;
                    }
                }
                sim_state_->pause();
                publish_frame();
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    "Advanced " + std::to_string(completed) + " live tick(s)");
                send_result(result);
            }
        }
        else if constexpr (std::is_same_v<T, CmdRun>) {
            std::cout << "[SimThread] Run: steps=" << arg.steps << "\n";
            if (sim_state_) {
                if (arg.steps > 0) {
                    scheduled_run_steps_ = arg.steps;
                    sim_state_->resume();
                } else {
                    scheduled_run_steps_ = -1;
                    sim_state_->resume();  // Run indefinitely
                }
                publish_frame();
                
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto exec_ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
                
                CmdResult result = CmdResult::ok(envelope.cmd_id, 
                    arg.steps > 0 ? "Ran " + std::to_string(arg.steps) + " steps" : "Running");
                result.stats = CmdResult::Stats{exec_ms, 0, false};
                send_result(result);
            }
        }
        
        // UI commands (handled by UI, but logged here)
        else if constexpr (std::is_same_v<T, CmdWindowControl>) {
            const char* action_names[] = {"SHOW", "HIDE", "TOGGLE"};
            std::cout << "[SimThread] Window control (UI side): " << arg.panel_name 
                      << " " << action_names[static_cast<int>(arg.action)] << "\n";
            
            CmdResult result = CmdResult::info(envelope.cmd_id, 
                "Window control: " + arg.panel_name);
            send_result(result);
        }
        
        // Unknown command
        else {
            std::cerr << "[SimThread] WARNING: Unknown command type in envelope\n";
            CmdResult result = CmdResult::error(envelope.cmd_id, "Unknown command type");
            send_result(result);
        }
    }, envelope.command);
}

void SimulationThread::publish_frame() {
    if (sim_state_) {
        FrameSnapshot snap = sim_state_->get_snapshot();
        snap.live.fixed_tick_hz = 1.0 / LiveSimulationClock::kFixedTickSeconds;
        // Publish a current partial window as well as completed 250 ms windows.
        // A finite run can pause on its final tick before the next full window
        // rollover, and the live telemetry should still report its real rate.
        snap.live.ticks_per_second = (ticks_in_rate_window_ > 0 && tick_rate_window_seconds_ > 0.0)
            ? static_cast<double>(ticks_in_rate_window_) / tick_rate_window_seconds_
            : live_ticks_per_second_;
        snap.live.simulation_time_seconds = live_clock_.simulation_time_seconds();
        snap.live.real_time_seconds = live_real_time_seconds_;
        snap.live.speed_multiplier = live_clock_.speed_multiplier();
        snap.live.tick_count = live_clock_.tick_count();
        snap.live.paused = sim_state_->is_paused();
        snap.live.solver_converged = sim_state_->mode() != SimMode::MD &&
            sim_state_->molecule().num_atoms() > 0 && sim_state_->is_converged();
        frame_buffer_.write(snap);
    }
}

void SimulationThread::add_to_custom_defaults(const std::string& formula) {
    // Add to custom defaults list (up to 5 recent attempts)
    if (std::find(custom_defaults_.begin(), custom_defaults_.end(), formula) == custom_defaults_.end()) {
        custom_defaults_.push_back(formula);
        if (custom_defaults_.size() > 5) {
            custom_defaults_.erase(custom_defaults_.begin());
        }
    }
}

std::string SimulationThread::get_defaults_list() const {
    std::string result;
    for (size_t i = 0; i < default_molecules_.size(); ++i) {
        if (i > 0) result += ", ";
        result += default_molecules_[i];
    }
    
    // Add custom defaults if any
    if (!custom_defaults_.empty()) {
        for (const auto& custom : custom_defaults_) {
            result += ", " + custom;
        }
    }
    
    return result;
}

bool SimulationThread::build_from_composition(
    const vsepr::formula::Composition& composition,
    const std::string& formula,
    uint64_t cmd_id)
{
    try {
        // Check if star-like molecule (single-center VSEPR)
        int heavy_count = 0;
        int total_atoms = 0;
        
        for (const auto& [Z, count] : composition) {
            total_atoms += count;
            if (Z > 1) heavy_count += count;
        }
        
        // For now, only support star-like molecules in interactive mode
        if (heavy_count > 1 && total_atoms > 3) {
            CmdResult result = CmdResult::error(cmd_id,
                "Complex multi-center molecules not yet supported in interactive mode. " +
                std::string("Formula: ") + formula + " would create " + 
                std::to_string(total_atoms) + " atoms with " + std::to_string(heavy_count) + " centers.");
            send_result(result);
            return false;
        }
        
        // Build molecule using graph builder
        Molecule mol = build_molecule_from_graph(composition, ptable_);
        
        // Generate angles from bonds
        mol.generate_angles_from_bonds();
        
        // Initialize simulation with new molecule
        sim_state_->initialize(mol);
        reset_live_clock();
        publish_frame();
        
        // Add to custom defaults
        if (std::find(default_molecules_.begin(), default_molecules_.end(), formula) == default_molecules_.end()) {
            custom_defaults_.push_back(formula);
            default_molecules_.push_back(formula);  // Add to permanent defaults for this session
        }
        
        CmdResult result = CmdResult::ok(cmd_id,
            "🔥 FIRED up " + formula + " with " + std::to_string(mol.num_atoms()) + " atoms! " +
            "(Added to defaults)");
        send_result(result);
        
        return true;
        
    } catch (const std::exception& e) {
        CmdResult result = CmdResult::error(cmd_id,
            "Failed to build " + formula + ": " + e.what());
        send_result(result);
        return false;
    }
}

} // namespace vsepr
