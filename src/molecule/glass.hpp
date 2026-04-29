#pragma once
// ============================================================================
// glass.hpp â€” Glass Module: Unified Include
// ============================================================================
// The glass module is the molecule prerender pipeline:
//
//   Topology -> Layout -> Prerender Buffers -> Draw
//
// This header pulls in all glass module components for convenience.
// Individual headers can also be included directly.
//
// Minimal usage:
//
//   #include "molecule/glass.hpp"
//   using namespace vsepr::glass;
//
//   auto mol    = build_topology(atomic_numbers, bond_pairs);
//   auto rings  = detect_rings(mol);
//
//   TopologyPrerender3D engine;
//   auto layout = engine.build_layout(mol);
//
//   MoleculePrerenderBuilder builder;
//   auto buffers = builder.build(mol, layout, rings);
//
//   // Upload buffers.atom_instances / buffers.bond_instances to GPU.
//   // Draw instanced sphere mesh + instanced cylinder mesh.
//
// ============================================================================

#include "molecule_types.hpp"
#include "topology_graph.hpp"
#include "ring_detect.hpp"
#include "layout_prerender3d.hpp"
#include "prerender_buffers.hpp"
#include "render_style_keys.hpp"
#include "report_prerender.hpp"
