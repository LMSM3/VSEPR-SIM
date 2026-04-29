#pragma once
/**
 * ehd.hpp
 *
 * Electrohydrodynamic Simulation — Unified Include
 *
 * Master header that pulls in all five EHD stages:
 *
 *   Stage 1 — CAD:     Parametric geometry (helix, tube, electrodes, STEP export)
 *   Stage 2 — Domain:  Domain extraction, named regions, boundary tags
 *   Stage 3 — Physics: Flow, electrostatics, ion transport, coupled solver
 *   Stage 4 — Mesh:    Mesh controls, inflation, adaptive refinement
 *   Stage 5 — Post:    Contour/streamline export, section probes, comparison tables
 *
 * Plus: run-card I/O for reproducible case specification.
 *
 * Governing equations (Navier-Stokes + Poisson + Nernst-Planck):
 *
 *   ∇·u = 0
 *   ρ(∂u/∂t + u·∇u) = -∇p + μ∇²u + f_elec
 *   ∇·(ε∇φ) = -ρ_e
 *   E = -∇φ
 *   ∂c_i/∂t + ∇·N_i = 0
 *   N_i = -D_i∇c_i - z_iμ_iFc_i∇φ + c_iu
 *
 * Usage:
 *   #include "sim/ehd/ehd.hpp"
 *   using namespace vsepr::ehd;
 */

// Types and constants
#include "sim/ehd/ehd_types.hpp"

// Stage 1: CAD / Geometry
#include "sim/ehd/cad/helix_generator.hpp"
#include "sim/ehd/cad/tube_body_generator.hpp"
#include "sim/ehd/cad/electrode_layout.hpp"
#include "sim/ehd/cad/step_export.hpp"
#include "sim/ehd/cad/planar_channel_generator.hpp"
#include "sim/ehd/cad/needle_ring_generator.hpp"
#include "sim/ehd/cad/disk_stack_generator.hpp"
#include "sim/ehd/cad/prism_slit_generator.hpp"

// Stage 2: Domain extraction
#include "sim/ehd/domains/domain_extract.hpp"
#include "sim/ehd/domains/named_regions.hpp"
#include "sim/ehd/domains/boundary_tags.hpp"

// Stage 3: Physics
#include "sim/ehd/physics/flow_model.hpp"
#include "sim/ehd/physics/electrostatic_model.hpp"
#include "sim/ehd/physics/ion_transport_model.hpp"
#include "sim/ehd/physics/body_force_model.hpp"
#include "sim/ehd/physics/coupled_solver.hpp"
#include "sim/ehd/physics/combustion_model.hpp"
#include "sim/ehd/physics/reactive_multiphase.hpp"

// Stage 4: Mesh
#include "sim/ehd/mesh/mesh_controls.hpp"

// Stage 5: Postprocessing
#include "sim/ehd/post/contour_export.hpp"
#include "sim/ehd/post/streamline_export.hpp"
#include "sim/ehd/post/section_probe.hpp"
#include "sim/ehd/post/comparison_tables.hpp"

// Run card I/O
#include "sim/ehd/run_card.hpp"
