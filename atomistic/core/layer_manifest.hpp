#pragma once
//
// layer_manifest.hpp
// ------------------
// Authoritative layer map for the formation engine architecture.
//
// Layer 1 - Formation Priors
//   Purpose: produce initialized candidate structures before physics runs.
//   Contents: VSEPR seeds, bond detection, crystal presets, heuristic init
//   Key types: UnitCell, BondedTopology, XYZMolecule
//
// Layer 2 - Deterministic Physical Kernel
//   Purpose: evaluate energy and forces; time-integrate or minimize.
//   Contents: LJ 12-6, bonded model, Coulomb (DEFERRED), SCF, FIRE, Verlet, Langevin, PBC
//   Key types: State, IModel, ModelParams, EnergyTerms, FIRE, VelocityVerlet
//
// Layer 3 - Verification and Benchmarking
//   Purpose: validate kernel correctness against fixed references.
//   Contents: phase executables, session artifacts, force-energy checks
//   Key files: apps/phase*.cpp, verification/
//
// Layer 4 - Analysis and Ranking
//   Purpose: score and classify kernel outputs.
//   Contents: crystal metrics, topology scoring (future), ranking (future)
//
// Layer 5 - Bridge and Artifact Output
//   Purpose: connect engine to desktop GUI; export artifacts.
//   Contents: EngineAdapter, SceneDocument, KernelRequest, XYZ writers
//

namespace atomistic {

enum class EngineLayer {
    FormationPriors       = 1,
    PhysicalKernel        = 2,
    Verification          = 3,
    AnalysisRanking       = 4,
    BridgeArtifact        = 5,
};

struct LayerEntry {
    EngineLayer  layer;
    const char*  name;
    const char*  status;   // "complete", "partial", "absent"
    const char*  note;
};

inline constexpr LayerEntry LAYER_MANIFEST[] = {
    { EngineLayer::FormationPriors,  "Formation Priors",       "partial",
      "Crystal presets complete. VSEPR seeds present. Learned topology prior absent." },
    { EngineLayer::PhysicalKernel,   "Deterministic Physical Kernel", "partial",
      "LJ+PBC active. Bonded model implemented but not composed. Coulomb deferred." },
    { EngineLayer::Verification,     "Verification & Benchmarking",  "complete",
      "Phases 1-7 all pass. 194 checks. Session artifacts on disk." },
    { EngineLayer::AnalysisRanking,  "Analysis & Ranking",     "partial",
      "Crystal metrics implemented. Structural ranking and learned prior absent." },
    { EngineLayer::BridgeArtifact,   "Bridge & Artifact Output","partial",
      "EngineAdapter wires LoadXYZ/Save/Relax/MD/SinglePoint. EmitCrystal wired." },
};

} // namespace atomistic
