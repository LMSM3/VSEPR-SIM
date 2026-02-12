/**
 * cmd_version.cpp
 * ---------------
 * Version command - display version and build information.
 */

#include "cmd_version.hpp"
#include "cli/display.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

int VersionCommand::Execute(const std::vector<std::string>& /*args*/) {
    Display::Banner("VSEPR-Sim Version Information");
    Display::BlankLine();
    
    Display::KeyValue("Program", "VSEPR-Sim", 15);
    Display::KeyValue("Version", "2.0.0", 15);
    Display::KeyValue("Build Date", std::string(__DATE__) + " " + std::string(__TIME__), 15);
    Display::KeyValue("C++ Standard", "C++17", 15);
    Display::BlankLine();
    
    Display::Subheader("Physics Engine");
    std::cout << "  Energy Model:     Harmonic bond + Lennard-Jones + VSEPR domains\n"
              << "  Optimizer:        FIRE (Fast Inertial Relaxation Engine)\n"
              << "  Coordinates:      Cartesian (3N-dimensional)\n";
    Display::BlankLine();
    
    Display::Subheader("Components");
    std::cout << "  • Energy evaluation (bond, angle, torsion, nonbonded)\n"
              << "  • Gradient computation (numerical validation supported)\n"
              << "  • Geometry optimization with convergence criteria\n"
              << "  • Periodic boundary conditions (PBC) for crystals\n"
              << "  • Molecular topology generation from connectivity\n";
    Display::BlankLine();
    
    Display::Subheader("Data Files");
    std::cout << "  Periodic Table:   data/PeriodicTableJSON.json\n"
              << "  Element Data:     data/elements.vsepr.json\n"
              << "                    data/elements.physics.json\n"
              << "                    data/elements.visual.json\n";
    Display::BlankLine();
    
    return 0;
}

std::string VersionCommand::Name() const {
    return "version";
}

std::string VersionCommand::Description() const {
    return "Show version and build information";
}

std::string VersionCommand::Help() const {
    return "Display VSEPR-Sim version, build information, and system details.";
}

}} // namespace vsepr::cli
