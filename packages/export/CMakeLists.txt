# packages/export/CMakeLists.txt
# ================================
# xsim::xport  --  XSIM export package
#
# Standalone static library.
#
# To consume from the root build:
#   add_subdirectory(packages/export)
#   target_link_libraries(my_target PRIVATE xsim_xport)
#
# Public headers are under include/xsim/xport/.
# The bridge header (xsim_export_bridge.hpp) requires the full project
# include path (include/vsim/) and is not compiled into this library.
#
#   #include <xsim/xport/xsim_xport.hpp>           -- standalone use
#   #include <xsim/xport/xsim_export_bridge.hpp>   -- full project only
# ============================================================

cmake_minimum_required(VERSION 3.15)

project(xsim_xport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Library target
# ------------------------------------------------------------

add_library(xsim_xport STATIC
	src/xsim_export.cpp
	src/xsim_xport_parser.cpp
	src/xsim_xport_writer.cpp
	src/xsim_xport_writers.cpp
	src/xsim_xport_verify.cpp
)

target_include_directories(xsim_xport
	PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# Compiler options
# ------------------------------------------------------------

if(MSVC)
	target_compile_options(xsim_xport PRIVATE /W4)
else()
	target_compile_options(xsim_xport PRIVATE -Wall -Wextra -Wpedantic)
endif()
