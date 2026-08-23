/**
 * Headless integration regression for the live SimulationThread worker.
 */

#include "command_router.hpp"
#include "core/element_data.hpp"
#include "pot/periodic_db.hpp"
#include "sim/sim_thread.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

#define CHECK(condition) \
    do { if (!(condition)) throw std::runtime_error("check failed: " #condition); } while (false)

bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

void initialize_chemistry_database() {
    static const vsepr::PeriodicTable periodic_table = vsepr::PeriodicTable::load_default();
    vsepr::init_chemistry_db(&periodic_table);
}

void text_build_bootstraps_molecule_topology() {
    initialize_chemistry_database();
    vsepr::SimulationThread simulation;
    vsepr::CommandRouter commands(simulation);
    simulation.set_command_router(&commands);
    simulation.start();

    commands.submit_command("build h2o", vsepr::CommandSource::INTERNAL);
    CHECK(wait_until([&] { return simulation.get_latest_frame().is_valid(); },
        std::chrono::milliseconds(500)));
    const auto frame = simulation.get_latest_frame();
    simulation.stop();

    CHECK(frame.positions.size() == 3);
    CHECK(frame.bonds.size() == 2);
}

void live_worker_uses_fixed_tick_scheduler() {
    initialize_chemistry_database();
    vsepr::SimulationThread simulation;
    vsepr::CommandRouter commands(simulation);
    simulation.set_command_router(&commands);
    simulation.start();

    vsepr::CmdInitMolecule water;
    water.atomic_numbers = {8, 1, 1};
    water.coords = {0.0, 0.0, 0.0, 0.96, 0.0, 0.0, -0.24, 0.93, 0.0};
    water.bonds = {{0, 1}, {0, 2}};
    CHECK(commands.command_queue().try_push(vsepr::CmdEnvelope(
        1, vsepr::CommandSource::INTERNAL, "init water", water)));
    CHECK(commands.command_queue().try_push(vsepr::CmdEnvelope(
        2, vsepr::CommandSource::INTERNAL, "mode md", vsepr::CmdSetMode{vsepr::SimMode::MD})));
    CHECK(wait_until([&] { return simulation.get_latest_frame().is_valid(); },
        std::chrono::milliseconds(500)));

    commands.submit_command("run 30", vsepr::CommandSource::INTERNAL);
    CHECK(wait_until([&] {
        const auto frame = simulation.get_latest_frame();
        return frame.live.tick_count >= 30 && simulation.is_paused();
    }, std::chrono::milliseconds(1000)));

    const auto frame = simulation.get_latest_frame();
    simulation.stop();

    CHECK(frame.live.fixed_tick_hz == 120.0);
    CHECK(frame.live.tick_count >= 30);
    CHECK(frame.live.tick_count == 30);
    CHECK(frame.live.ticks_per_second > 80.0);
    CHECK(frame.live.ticks_per_second < 160.0);
    CHECK(frame.live.simulation_time_seconds > 0.20);
    CHECK(frame.live.speed_multiplier == 1.0);
}

} // namespace

int main() {
    try {
        text_build_bootstraps_molecule_topology();
        std::cout << "PASS text_build_bootstraps_molecule_topology\n";
        live_worker_uses_fixed_tick_scheduler();
        std::cout << "PASS live_worker_uses_fixed_tick_scheduler\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL live_worker_uses_fixed_tick_scheduler - " << error.what() << "\n";
        return 1;
    }
}
