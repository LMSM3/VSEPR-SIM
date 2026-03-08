cmake_minimum_required(VERSION 3.15)

# ============================================================================
# ATOMISTIC LIBRARY BUILD INITIALIZATION
# ============================================================================
# This is the core atomistic (nano-scale) simulation library.
# Visualization on configure is an integral part of the build identity.
# ============================================================================

# Capture build timestamp
string(TIMESTAMP ATOMISTIC_BUILD_TIME "%Y-%m-%d %H:%M:%S UTC" UTC)

# Detect compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(ATOMISTIC_COMPILER "GCC ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(ATOMISTIC_COMPILER "Clang ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(ATOMISTIC_COMPILER "MSVC ${CMAKE_CXX_COMPILER_VERSION}")
else()
    set(ATOMISTIC_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║                                                                                ║")
message(STATUS "║                    ATOMISTIC: Nano-Scale Molecular Simulation                  ║")
message(STATUS "║                                                                                ║")
message(STATUS "╚════════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "                                                       ...  ..... ...........")
message(STATUS "                                                   ..:==+*:..=--+. ..-=-+.-+:=-.")
message(STATUS "                                                   .-===+*#=...-+.  .+..--..:+:..")
message(STATUS "                                                    -+**##%=.:..=:...+..=--...*...")
message(STATUS "                                                   ..-%%%%*:.-++=.-=.:++-..+++..")
message(STATUS "                                                   . .....:=-.... .. .  .  ..... ")
message(STATUS "                                                           .==.")
message(STATUS "                                                           ..:=-.........")
message(STATUS "....   ...........                                             ..........:-=:..")
message(STATUS "....++-.  .:=..+.+:.=:                   ..     ... ... ... ..         ..........:-=*=..")
message(STATUS ".....--. ....=+-:+=-+..                  .+... ..+=+:.+==+....         ........::-=+*#. .")
message(STATUS "    .--.  .=:..+.+..=:.                  .+..  .:. :+.+--...           ......::--=+*##.")
message(STATUS "    .:-:.:..:-:...:-..                   .+.    .==......*. .           .:----==+*###=:.")
message(STATUS " ..:==++**. .                           ..+...+.++++=.+=+=...           ..:++****###:..")
message(STATUS " ..++++*#%-                                                             ..+..:=+=:...")
message(STATUS "...-*##%%#. .                                    ..==++=..           ...-+")
message(STATUS "     .==-=- ..                                  ..===+*##.         ....+:.")
message(STATUS "    ......==                              .   ...:++**#%#.         ..-=.")
message(STATUS "         ..==.                       ........-+*-..#%+-*..        ..*. .")
message(STATUS "           ..==                     ...:-++:..... .....-.        .-+.")
message(STATUS "             .=+..                ....=*=....           ..:..    ..:==+-*:")
message(STATUS "              .-=-........     ....:=+=:.                  ...-   .:===**%*.")
message(STATUS "    ...:=+=...  .=+######*.. .......:=*-..                         .:..  .:***#%%=.=...")
message(STATUS "    ..===+*#+...+##**####%%=..-=+-.                               ..=    ...:+*#%#:..+...")
message(STATUS "     .+++*=:...=##**+###%%%@+:.......")
message(STATUS "    ..+##%%#...+######%%%#=*+.. .   .   ..     .. ....")
message(STATUS "     . ....... -####%%%%%@%%----:.......")
message(STATUS "            ..-**%%%%%@@@@@-. . ..   .-==:.....-=-=+*%.")
message(STATUS "            ...:+=.....+%@@@@%--=-.              ...+++**#%*-:...")
message(STATUS "...........=+:.               ..-=-..              .###%%+..=*###*+:........::--=+*-..")
message(STATUS ".==++*+-++.                      .-=:..                          .:=+*##+...:--=+*##..")
message(STATUS "+==+*#%-...                      . .==: .                          ...::::::--=+**##-.")
message(STATUS "-**#%%%..                         .. .==.                            .-----==++*######*+-..")
message(STATUS "..=*+:...                           ....==.                          ..=++++**#####:. ..:-+##*++")
message(STATUS "                                       ..:==..                       ...**####*.#-...")
message(STATUS "                                         ..:=-.                      .-=.... ...:")
message(STATUS "                                           ..-=-                   .:*.        ..-")
message(STATUS "                                             ..-=:.              ..=-           ..:")
message(STATUS "                                              . .==..           .:*..          ...")
message(STATUS "                                                  .==.        ..+-.             ...")
message(STATUS "                                                  ...==.     .-+ ..               ...")
message(STATUS "                                                   ...:===+*:+:.                  ...")
message(STATUS "                                                   ....==-+*#%.                   .===+*#:.")
message(STATUS "                                                     ..++**#%%:.                  =+++*#%*.")
message(STATUS "                                                   .....*#%%#: ..                 .**##%%:.")
message(STATUS "                                                     ..  ... ..                   ...=.-..")
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║  BUILD CONFIGURATION                                                           ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  Version:         2.5.0-dev (Atomistic Core)                                  ║")
message(STATUS "║  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "║  Compiler:        ${ATOMISTIC_COMPILER}")
message(STATUS "║  C++ Standard:    C++20                                                        ║")
message(STATUS "║  Build Time:      ${ATOMISTIC_BUILD_TIME}")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  PHYSICS MODELS                                                                ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  ✓ Real force fields (LJ + Coulomb)                                            ║")
message(STATUS "║  ✓ Bonded interactions (bonds, angles, torsions)                               ║")
message(STATUS "║  ✓ Thermodynamics (partition functions, free energy)                           ║")
message(STATUS "║  ✓ FIRE minimization (Fast Inertial Relaxation Engine)                         ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  CORE FEATURES                                                                 ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  ✓ State alignment (Kabsch algorithm, RMSD calculation)                        ║")
message(STATUS "║  ✓ Property prediction (dipole, polarizability, energy)                        ║")
message(STATUS "║  ✓ Reaction discovery (random exploration, pattern mining)                     ║")
message(STATUS "║  ✓ Causal modeling (mechanistic reaction networks)                             ║")
message(STATUS "║  ✓ XYZ I/O (parsing, compilation, validation)                                  ║")
message(STATUS "║  ✓ Markdown reporting (automated result generation)                            ║")
message(STATUS "║  ✓ Crystal benchmark library (31 presets: fluorite, spinel, perovskite,        ║")
message(STATUS "║    garnet, apatite, monazite, pyrochlore, actinide oxide polymorphs)           ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  PHILOSOPHY: No Reliance on External Data Sources O                                            ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  All simulations use validated physical models. No visual-only approximations. ║")
message(STATUS "║  Every geometry emerges from energy minimization, not hardcoded rules.         ║")
message(STATUS "╚════════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")

# Module count for diagnostic
set(ATOMISTIC_MODULE_COUNT 0)
macro(count_module)
    math(EXPR ATOMISTIC_MODULE_COUNT "${ATOMISTIC_MODULE_COUNT} + 1")
endmacro()

# ============================================================================
# ATOMISTIC LIBRARY: Static library compilation
# ============================================================================

# ============================================================================
# ATOMISTIC LIBRARY: Static library compilation
# ============================================================================

message(STATUS "Compiling ATOMISTIC static library...")
message(STATUS "")

# Count first module
math(EXPR ATOMISTIC_MODULE_COUNT "${ATOMISTIC_MODULE_COUNT} + 1")
message(STATUS "  [${ATOMISTIC_MODULE_COUNT}] Core: State, LinearAlgebra, Alignment, Statistics, Thermodynamics")

add_library(atomistic STATIC
    core/state.hpp
    core/linalg.hpp
    core/linalg.cpp
    core/alignment.hpp
    core/alignment.cpp
    core/statistics.hpp
    core/thermodynamics.hpp
    core/maxwell_boltzmann.hpp
    core/maxwell_boltzmann.cpp
    analysis/pmf.hpp
    analysis/pmf.cpp
    crystal/lattice.hpp
    crystal/lattice.cpp
    crystal/unit_cell.hpp
    crystal/unit_cell.cpp
    crystal/supercell.hpp
    crystal/supercell.cpp
    crystal/crystal_metrics.hpp
    crystal/crystal_metrics.cpp
    crystal/reference_data.hpp
    classify/fingerprints.hpp
    classify/fingerprints.cpp
    classify/cluster.hpp
    classify/cluster.cpp
    models/model.hpp
    models/lj_coulomb.cpp
    models/bonded.hpp
    models/bonded.cpp
    models/composite.hpp
    models/composite.cpp
    models/atomic_descriptors.hpp
    models/alpha_model.hpp
    models/nuclear_stability.hpp
    models/alpha_stability.hpp
    models/polarization_scf.hpp
    models/polarization_scf.cpp
    integrators/fire.hpp
    integrators/fire.cpp
    integrators/velocity_verlet.hpp
    integrators/velocity_verlet.cpp
    parsers/xyz_parser.hpp
    parsers/xyz_parser.cpp
    compilers/xyz_compiler.hpp
    compilers/xyz_compiler.cpp
    predict/properties.hpp
    predict/properties.cpp
    reaction/engine.hpp
    reaction/engine.cpp
    reaction/heat_gate.hpp
    reaction/heat_gate.cpp
    reaction/discovery.hpp
    reaction/discovery.cpp
    reaction/causal.hpp
    reaction/causal.cpp
    reaction/discovery_viz.hpp
    reaction/discovery_viz.cpp
    report/report_md.hpp
    report/report_md.cpp
    validation/experiment_match.hpp
    validation/experiment_match.cpp
    validation/nanoparticle_ref.hpp
)

message(STATUS "")
message(STATUS "  Modules:         ${ATOMISTIC_MODULE_COUNT}/10  ✓ ")
message(STATUS "    + crystal_metrics (6-category verification scorecard)")
message(STATUS "    + classify (polymorph / isomorph / defect detection)")
message(STATUS "    + validation (experimental data matching, 4-6nm nanoparticle regime)")

target_include_directories(atomistic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(atomistic PUBLIC
    vsepr_core
    vsepr_io
    vsepr_pot
)

target_compile_features(atomistic PUBLIC cxx_std_20)


