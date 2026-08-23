# cmake/VSEPRBuildInfo.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Generates ${CMAKE_BINARY_DIR}/generated/vsepr_build_config.hpp from the
# template cmake/templates/vsepr_build_config.hpp.in.
#
# Captures: project version, build type, compiler, C++ standard, build
# timestamp, git commit/branch/dirty status, and all VSEPR feature flags.
#
# Usage (in CMakeLists.txt, after project() and option() declarations):
#
#   include(VSEPRBuildInfo)
#   vsepr_generate_build_config(OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated")
# ─────────────────────────────────────────────────────────────────────────────

# ── Git metadata ──────────────────────────────────────────────────────────────
function(vsepr_get_git_info OUT_COMMIT OUT_BRANCH OUT_DIRTY)
	find_package(Git QUIET)

	if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			OUTPUT_VARIABLE GIT_COMMIT
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			OUTPUT_VARIABLE GIT_BRANCH
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" status --porcelain
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			OUTPUT_VARIABLE GIT_STATUS
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		if(GIT_STATUS STREQUAL "")
			set(GIT_DIRTY "false")
		else()
			set(GIT_DIRTY "true")
		endif()
	else()
		set(GIT_COMMIT "unknown")
		set(GIT_BRANCH "unknown")
		set(GIT_DIRTY  "unknown")
	endif()

	set(${OUT_COMMIT} "${GIT_COMMIT}" PARENT_SCOPE)
	set(${OUT_BRANCH} "${GIT_BRANCH}" PARENT_SCOPE)
	set(${OUT_DIRTY}  "${GIT_DIRTY}"  PARENT_SCOPE)
endfunction()

# ── Bool → string helper ──────────────────────────────────────────────────────
function(vsepr_bool_to_string VAR OUT)
	if(${VAR})
		set(${OUT} "true" PARENT_SCOPE)
	else()
		set(${OUT} "false" PARENT_SCOPE)
	endif()
endfunction()

# ── Main generator ────────────────────────────────────────────────────────────
function(vsepr_generate_build_config)
	set(oneValueArgs OUTPUT_DIR)
	cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

	if(NOT ARG_OUTPUT_DIR)
		message(FATAL_ERROR "vsepr_generate_build_config: OUTPUT_DIR is required")
	endif()

	file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")

	# Timestamp (UTC)
	string(TIMESTAMP VSEPR_BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

	# Git
	vsepr_get_git_info(VSEPR_GIT_COMMIT VSEPR_GIT_BRANCH VSEPR_GIT_DIRTY)

	# Build type: use CMAKE_BUILD_TYPE for single-config generators,
	# generator expression fallback for multi-config (MSVC / Xcode).
	if(CMAKE_BUILD_TYPE)
		set(VSEPR_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
	else()
		set(VSEPR_BUILD_TYPE "RelWithDebInfo")   # sensible default for multi-config
	endif()

	set(VSEPR_PROJECT_VERSION  "${PROJECT_VERSION}")
	set(VSEPR_COMPILER_ID      "${CMAKE_CXX_COMPILER_ID}")
	set(VSEPR_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
	set(VSEPR_CXX_STANDARD     "${CMAKE_CXX_STANDARD}")

	# Feature flags → "true" / "false" strings for the .hpp template
	vsepr_bool_to_string(VSEPR_ENABLE_ATOMISTIC     VSEPR_FEATURE_ATOMISTIC)
	vsepr_bool_to_string(VSEPR_ENABLE_COARSE_GRAIN  VSEPR_FEATURE_COARSE_GRAIN)
	vsepr_bool_to_string(VSEPR_ENABLE_XYZ_IO        VSEPR_FEATURE_XYZ_IO)
	vsepr_bool_to_string(VSEPR_ENABLE_FIRE          VSEPR_FEATURE_FIRE)
	vsepr_bool_to_string(VSEPR_ENABLE_THERMO        VSEPR_FEATURE_THERMO)
	vsepr_bool_to_string(VSEPR_ENABLE_REACTION      VSEPR_FEATURE_REACTION)
	vsepr_bool_to_string(VSEPR_ENABLE_CRYSTAL_LIB   VSEPR_FEATURE_CRYSTAL_LIB)
	vsepr_bool_to_string(VSEPR_ENABLE_REGISTRY_CORE VSEPR_FEATURE_REGISTRY_CORE)
	vsepr_bool_to_string(VSEPR_ENABLE_INSTALL_TOOLS VSEPR_FEATURE_INSTALL_TOOLS)

	configure_file(
		"${CMAKE_SOURCE_DIR}/cmake/templates/vsepr_build_config.hpp.in"
		"${ARG_OUTPUT_DIR}/vsepr_build_config.hpp"
		@ONLY
	)

	message(STATUS "Generated: ${ARG_OUTPUT_DIR}/vsepr_build_config.hpp")
endfunction()
