# VSEPR-SIM Bridge Architecture - Version Beta

> **Status:** Beta scaffold
> **Date:** 2025-06-28
> **Version:** 0.1.0-beta

## Purpose

Staging ground for the next-generation bridge architecture prerequisites.

### dnf_sim/ - Dependency Simulation Layer

Mock package resolver that enumerates, validates, and acquires the
prerequisite modules required by the bridge before any kernel op proceeds.

| Module | Min Version | Provides |
|--------|-------------|----------|
| atomistic_core | 2.0.0 | State, IModel, FIRE/Verlet |
| scene_document | 1.3.0 | SceneDocument, FrameData |
| glass_pipeline | 1.0.0 | Topology->Layout->SVG |
| coarse_grain | 0.8.0 | BeadSystem, SH descriptors |
| bridge_adapter | 1.1.0 | EngineAdapter, KernelRequest |

### coarsegrain_3d_new/ - CG Bead to Glass 3D Pipeline

Bridges CG bead representation into the glass molecule prerender pipeline:
BeadSystem -> GlassMolecule -> TopologyPrerender3D -> PrerenderBuffers -> SVG

## Build

add_subdirectory(bridge_beta)
