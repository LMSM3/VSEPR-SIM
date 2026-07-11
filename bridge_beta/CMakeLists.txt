# ============================================================================
# bridge_beta/CMakeLists.txt - Beta Scaffold Build
# ============================================================================

# --- DNF Sim (mock dependency resolver) ---
add_library(vsepr_dnf_sim STATIC
    dnf_sim/dnf_sim.cpp
)
target_include_directories(vsepr_dnf_sim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/dnf_sim
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)

# --- CG 3D Prerender (bead -> glass bridge) ---
add_library(vsepr_cg3d_new STATIC
    coarsegrain_3d_new/cg3d_prerender.cpp
)
target_include_directories(vsepr_cg3d_new PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/coarsegrain_3d_new
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(vsepr_cg3d_new PUBLIC vsepr_glass vsepr_core)

