/**
 * command_parser_test.cpp
 * -----------------------
 * Test suite for the command parser - Path-based commands.
 */

#include "../src/vis/command_parser.hpp"
#include "../src/sim/sim_command.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace vsepr;

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) std::cout << "Testing " << name << "... "; 
#define PASS() { std::cout << "PASS\n"; tests_passed++; }
#define FAIL(msg) { std::cout << "FAIL: " << msg << "\n"; tests_failed++; }
#define ASSERT(cond, msg) if (!(cond)) { FAIL(msg); return; } 

// Helper to check success and extract command
bool is_success(const ParseResult& result) {
    return std::holds_alternative<ParseSuccess>(result);
}

std::string get_message(const ParseResult& result) {
    if (auto* s = std::get_if<ParseSuccess>(&result)) {
        return s->echo;
    }
    if (auto* e = std::get_if<ParseError>(&result)) {
        return e->error_message;
    }
    return "";
}

template<typename T>
const T* get_command(const ParseResult& result) {
    if (auto* s = std::get_if<ParseSuccess>(&result)) {
        return std::get_if<T>(&s->command);
    }
    return nullptr;
}

// Helper to get value from ParamValue
template<typename T>
std::optional<T> get_value(const ParamValue& v) {
    if (auto* p = std::get_if<T>(&v)) return *p;
    return std::nullopt;
}

void test_set_pbc_on() {
    TEST("set pbc on");
    CommandParser parser;
    auto result = parser.parse("set pbc on");
    ASSERT(is_success(result), "Parse failed");
    auto* cmd = get_command<CmdSet>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->path == "pbc.enabled", "path should be pbc.enabled");
    auto val = get_value<bool>(cmd->value);
    ASSERT(val.has_value() && *val == true, "value should be true");
    PASS();
}

void test_set_pbc_off() {
    TEST("set pbc off");
    CommandParser parser;
    auto result = parser.parse("set pbc off");
    ASSERT(is_success(result), "Parse failed");
    auto* cmd = get_command<CmdSet>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->path == "pbc.enabled", "path should be pbc.enabled");
    auto val = get_value<bool>(cmd->value);
    ASSERT(val.has_value() && *val == false, "value should be false");
    PASS();
}

void test_set_box_size() {
    TEST("set box 50");
    CommandParser parser;
    auto result = parser.parse("set box 50");
    ASSERT(is_success(result), "Parse failed");
    auto* cmd = get_command<CmdSet>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->path == "pbc.box", "path should be pbc.box");
    auto val = get_value<double>(cmd->value);
    ASSERT(val.has_value() && std::abs(*val - 50.0) < 0.01, "value should be 50");
    PASS();
}

void test_set_box_anisotropic() {
    TEST("set box 10 20 30");
    CommandParser parser;
    auto result = parser.parse("set box 10 20 30");
    ASSERT(is_success(result), "Parse failed");
    // Currently parser returns single box value (first dimension)
    // TODO: Full anisotropic support would need compound commands
    auto* cmd = get_command<CmdSet>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->path == "pbc.box", "path should be pbc.box");
    auto val = get_value<double>(cmd->value);
    ASSERT(val.has_value() && std::abs(*val - 10.0) < 0.01, "value should be 10.0");
    PASS();
}

void test_set_lj_epsilon() {
    TEST("set lj.epsilon 0.01");
    CommandParser parser;
    auto result = parser.parse("set lj.epsilon 0.01");
    ASSERT(is_success(result), "Parse failed");
    auto* cmd = get_command<CmdSet>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->path == "lj.epsilon", "path should be lj.epsilon");
    auto val = get_value<double>(cmd->value);
    ASSERT(val.has_value() && std::abs(*val - 0.01) < 0.001, "value should be 0.01");
    PASS();
}

void test_set_cutoff() {
    TEST("set cutoff 12.0");
    CommandParser parser;
    auto result = parser.parse("set cutoff 12.0");
    ASSERT(is_success(result), "Parse failed");
    auto* cmd = get_command<CmdSet>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->path == "lj.cutoff", "path should be lj.cutoff");
    auto val = get_value<double>(cmd->value);
    ASSERT(val.has_value() && std::abs(*val - 12.0) < 0.01, "value should be 12.0");
    PASS();
}

void test_spawn_gas() {
    TEST("spawn gas --n 1000 --box 100 --species Ar");
    CommandParser parser;
    auto result = parser.parse("spawn gas --n 1000 --box 100 --species Ar");
    ASSERT(is_success(result), "Parse failed: " + get_message(result));
    auto* cmd = get_command<CmdSpawn>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->type == SpawnType::GAS, "Should be GAS spawn");
    ASSERT(cmd->n_particles == 1000, "Should have 1000 particles");
    ASSERT(std::abs(cmd->box_x - 100.0) < 0.01, "Box should be 100");
    ASSERT(cmd->species == "Ar", "Species should be Ar");
    PASS();
}

void test_spawn_crystal_fcc() {
    TEST("spawn crystal --type fcc --n 4 --a 4.0");
    CommandParser parser;
    auto result = parser.parse("spawn crystal --type fcc --n 4 --a 4.0");
    ASSERT(is_success(result), "Parse failed: " + get_message(result));
    auto* cmd = get_command<CmdSpawn>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->type == SpawnType::CRYSTAL, "Should be CRYSTAL spawn");
    ASSERT(cmd->lattice == LatticeType::FCC, "Should be FCC lattice");
    ASSERT(cmd->nx == 4 && cmd->ny == 4 && cmd->nz == 4, "Should have 4x4x4 cells");
    ASSERT(std::abs(cmd->lattice_constant - 4.0) < 0.01, "Lattice constant should be 4.0");
    PASS();
}

void test_window_show() {
    TEST("window console --show");
    CommandParser parser;
    auto result = parser.parse("window console --show");
    ASSERT(is_success(result), "Parse failed: " + get_message(result));
    auto* cmd = get_command<CmdWindowControl>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->panel_name == "console", "Panel should be console");
    ASSERT(cmd->action == WindowAction::SHOW, "Action should be SHOW");
    PASS();
}

void test_window_hide() {
    TEST("window diagnostics --hide");
    CommandParser parser;
    auto result = parser.parse("window diagnostics --hide");
    ASSERT(is_success(result), "Parse failed: " + get_message(result));
    auto* cmd = get_command<CmdWindowControl>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->panel_name == "diagnostics", "Panel should be diagnostics");
    ASSERT(cmd->action == WindowAction::HIDE, "Action should be HIDE");
    PASS();
}

void test_window_toggle() {
    TEST("window all --toggle");
    CommandParser parser;
    auto result = parser.parse("window all --toggle");
    ASSERT(is_success(result), "Parse failed: " + get_message(result));
    auto* cmd = get_command<CmdWindowControl>(result);
    ASSERT(cmd != nullptr, "Wrong command type");
    ASSERT(cmd->panel_name == "all", "Panel should be all");
    ASSERT(cmd->action == WindowAction::TOGGLE, "Action should be TOGGLE");
    PASS();
}

void test_help() {
    TEST("help");
    CommandParser parser;
    auto result = parser.parse("help");
    // Help returns a parse error with helpful information (no command needed)
    std::cout << "(message: " << get_message(result) << ") ";
    PASS();
}

void test_help_spawn() {
    TEST("help spawn");
    CommandParser parser;
    auto result = parser.parse("help spawn");
    std::cout << "(message: " << get_message(result) << ") ";
    PASS();
}

int main() {
    std::cout << "=== Command Parser Tests (Path-based) ===\n\n";
    
    // CmdSet commands with path-based parameters
    test_set_pbc_on();
    test_set_pbc_off();
    test_set_box_size();
    test_set_box_anisotropic();
    test_set_lj_epsilon();
    test_set_cutoff();
    
    // Spawn commands
    test_spawn_gas();
    test_spawn_crystal_fcc();
    
    // Window commands
    test_window_show();
    test_window_hide();
    test_window_toggle();
    
    // Help
    test_help();
    test_help_spawn();
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
