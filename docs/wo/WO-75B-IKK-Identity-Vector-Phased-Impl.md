# WO-75B — Slow Implementation of fraktur-I (Identity) Vectors
## Phased Introduction of I-Vector Fields into the Particle State Pipeline

**Work order:** WO-75B
**Branch:** v5.0.0-main
**Status:** 🟡 READY (Phase 1 & 2 clear) — Phase 3+ gates on WO-75A
**Depends on:** WO-75A (D enrichment) — Phase 3+ only, WO-SM-IDENTITY, identity_sidecar.hpp (v5.1.4)
**Theory refs:** IKK I §2, IKK III Matrix-Wave Bridge, IKK Notation Registry, vsim_bridge_doc.tex

---

## Objective

Introduce I (fraktur-I, primitive identity) vectors into the VSPER-SIM pipeline in a
slow, disciplined, non-breaking sequence. The I-vector is the per-particle representation
of identity content at a given scale level Sn. It must never enter truth state (.xyz,
.xyzFull, ground-truth particle structs). It lives in the analysis, sidecar, report, and
overlay layers only.

This work order defines four phases spread across future milestones. Each phase is
independently mergeable and leaves the simulation kernel unmodified.

---

## Theory Grounding

From IKK III (Matrix-Wave Bridge):

  Pi = [xi, pi, qi, Di, Ii]       (theory form of particle state vector)
  Pi = [xi, vi, mi, qi, chi_i]    (simulation form — empirical layer only)

The I-vector is the gap between these two forms. It captures what the particle IS
at the primitive layer — the identity content that survives (or is lost during)
projection from fraktur-H_a (source identity space) to H_b (target Hilbert space).

From IKK I §2.3:
  I_vector components at S3 (atomistic scale):
    I_i = (I_existence, I_EM, I_spatial, I_temporal, I_internal)
  Each component in [0,1] — presence/strength of that identity channel.

From the particle_identity.hpp IKK grounding note:
  scale_mask : bitmask of IKK scale levels active for this particle
  r_fuzz     : fuzz proxy — spread of identity anchor at S3
  H_charge   : hidden charge intensity H = c_j^T W c_j
  H_color    : hidden color intensity
  H_spin     : hidden spin intensity

The I-vector at the simulation layer formalises these into a structured per-particle
quantity that can be tracked, visualised, and reported without touching truth state.

---

## Phase Plan

### Phase 1 — I-Vector Definition + Sidecar Extension  (v5.2.0)

Define the IKKIdentityVector struct and add it to IdentitySidecarRecord.

New file: include/vsim/analysis/ikk_identity_vector.hpp

  struct IKKIdentityVector {
      float I_existence;   // S0 — presence at scale (1.0 = fully present, 0 = null)
      float I_EM;          // S2/S3 — electromagnetic channel identity strength
      float I_spatial;     // S3 — spatial localisability (fuzz proxy inverse)
      float I_temporal;    // S4 — temporal ordering / coherence
      float I_internal;    // internal state channel (spin, color composite)
      float I_mag;         // magnitude: sqrt(sum of squares) — overall identity norm

      float scale_level;   // Sn index this vector was computed at
      bool  is_null;       // true if particle has no recoverable identity at this scale

      // Derived from IKK I §2.3:
      // I_mag = 1.0 means full identity content at this scale
      // I_mag = 0.0 means identity dissolved (maximum entropy, D = 0)
      // I_EM relates to H_charge from particle_identity.hpp
      // I_spatial = 1.0 - r_fuzz (low fuzz = high spatial identity)
  };

Extend IdentitySidecarRecord (identity_sidecar.hpp):

  std::vector<IKKIdentityVector> I_vectors;  // one per particle per frame

Population: computed in the analysis layer from existing sidecar scalars.
  I_existence = 1.0 - dataloss
  I_EM        = 1.0 - hidden_channel
  I_spatial   = recoverable_info
  I_temporal  = 1.0 - projection_loss
  I_internal  = 1.0 - identity_residual
  I_mag       = sqrt(sum of squares) / sqrt(5)  (normalised)

These are proxies. They are not first-principles quantum computations. They are
IKK-consistent derived estimates from the existing sidecar fields. The doctrine
is that they belong in the analysis layer, not the kernel.

Output: I-vectors written to the .identity.json sidecar as an additional array.
  "I_vectors": [ { "I_existence":0.98, "I_EM":0.91, ... }, ... ]

---

### Phase 2 — I-Vector Visual Overlay  (v5.2.1)

Add RENDER_PASS_IVEC to the renderer (extends WO-75A overlay infrastructure).

Two new sub-modes toggled by pressing I in the GL viewer:

  Mode I-MAG:
    Per-atom sphere size scaled by I_mag.
    Full-identity particles are large. Identity-dissolved particles are small/ghosted.
    CPK base colour retained, transparency modulated by I_mag (alpha = I_mag).

  Mode I-CHANNELS:
    Five thin concentric rings drawn around each atom (using billboard geometry):
      Ring 1 (innermost) — I_existence   blue
      Ring 2             — I_EM          cyan
      Ring 3             — I_spatial     green
      Ring 4             — I_temporal    amber
      Ring 5 (outermost) — I_internal    magenta
    Ring brightness = channel value. Dim ring = low identity in that channel.

ImGui panel additions (extends WO-75A D panel):

  I-VECTOR BREAKDOWN   particle #<selected>
  ─────────────────────────────────────────
  I_existence  ████████░░  0.87   S0
  I_EM         █████████░  0.91   S2/S3
  I_spatial    ████████░░  0.84   S3
  I_temporal   ███████░░░  0.76   S4
  I_internal   █████░░░░░  0.61   internal
  ─────────────────────────────────────────
  I_mag        ████████░░  0.80   (normalised)
  D (from §A)  ████████░░  0.87

  Projection chain: fraktur-H_a --[C_ab]--> K_ab --[R_b]--> H_b
  W_ab(fraktur-I_a) = psi_b^(a)   [IKK III central claim]

---

### Phase 3 — I-Vector Report Integration  (v5.3.0)

Extend the IKK End-Tag Block from WO-75A to include the I-vector breakdown table.

Per-particle I-vector statistics added to end-tag:

  I-VECTOR SUMMARY (mean over all particles)
  I_existence  : 0.93
  I_EM         : 0.88
  I_spatial    : 0.85
  I_temporal   : 0.79
  I_internal   : 0.72
  I_mag        : 0.84

  Weakest channel: I_internal (0.72) — spin/color identity partially dissolved.
  Interpretation: internal degrees of freedom experienced the greatest projection
  loss across scale S3 -> S_observable. Consistent with thermal noise at 300 K.

  Theory note: I_internal relates to H_spin and H_color from particle_identity.hpp.
  Hidden intensity H = c_j^T W c_j > 0 even when net charge = 0.

Python additions (generate_report.py):
  - plot_ivec_radar() — radar chart of 5 I-channels, mean over trajectory
  - Saved as out/<run_id>/ivec_radar.png, embedded in report.md

LaTeX additions:
  \newcommand{\Ivec}[1]{\mathbf{I}^{(#1)}}   % I-vector at scale n
  \newcommand{\Imag}{|\mathbf{I}|}            % I-vector magnitude

---

### Phase 4 — I-Vector Time Evolution Tracking  (v5.3.x — slow burn)

Track how each I-channel evolves across simulation steps. This is the first step
toward formalising the I_a(t) -> s_a(t) -> U_a s_a(t) chain from IKK III.

New sidecar output: <run_id>.ivec_series.tsv
  Columns: step | particle_id | I_existence | I_EM | I_spatial | I_temporal | I_internal | I_mag

New Python report function: plot_ivec_evolution()
  - Line plot per channel, one subplot per channel
  - Highlight steps where any channel drops below 0.5 (property activation threshold IKK IV)
  - Mark coalescence events from coalescence_events sidecar field

Viewer addition: I-vector time scrubber
  - When paused on a frame, show I-channel bars for selected atom
  - Arrow keys step through frames and animate the bar chart

Formal note added to report:
  The time evolution I_a(t) is the simulation-layer proxy for the identity state
  evolution s_a(t) under U_a in the IKK III matrix-mechanics formalism. The
  observable psi_b^(a) at each step is recovered by W_{a->b}(I_a(t)).
  This is a representation, not a first-principles quantum propagation.

---

## What Is NOT Changing (Strict Constraints)

- The simulation kernel (particle positions, velocities, forces) is NOT modified
- .xyz and .xyzFull truth files do NOT gain I-vector fields
- The I-vector is NOT a new particle property in the physics sense
- I-vector values are derived quantities only — never used as force inputs
- The w-coordinate is NOT labelled as a physical length in any new output (IKK IV)
- I-vectors are NOT claims about actual quantum wavefunction content — they are
  IKK-consistent derived estimates from simulation observables (analysis layer only)

---

## Phase Summary

  Phase 1  v5.2.0   IKKIdentityVector struct + sidecar extension + JSON output
  Phase 2  v5.2.1   I-vector visual overlays (magnitude mode + channel ring mode)
  Phase 3  v5.3.0   I-vector report integration + radar chart + LaTeX macros
  Phase 4  v5.3.x   I-vector time evolution tracking + viewer scrubber

---

## Deliverables by Phase

Phase 1:
  include/vsim/analysis/ikk_identity_vector.hpp   new struct definition
  include/vsim/analysis/identity_sidecar.hpp      add I_vectors field
  src/analysis/ikk_identity_builder.cpp           proxy computation from sidecar scalars
  src/analysis/ikk_identity_builder.hpp           header

Phase 2:
  src/vis/renderer.hpp                            RenderPass enum extension + I-vec methods
  src/vis/renderer.cpp                            I-MAG and I-CHANNELS passes
  src/vis/renderer_classic.cpp                    same

Phase 3:
  reporting/generate_report.py                    plot_ivec_radar() + embed
  reporting/report.tex                            \Ivec, \Imag macros
  src/core/report_engine.hpp/.cpp                 I-vector stats in IKKEndTag

Phase 4:
  reporting/generate_report.py                    plot_ivec_evolution()
  sidecar writer                                  .ivec_series.tsv emission
  src/vis/renderer.hpp/.cpp                       time scrubber integration

---

## Acceptance Criteria

Phase 1:
  - IKKIdentityVector populated from sidecar fields without kernel modification
  - .identity.json contains I_vectors array per frame
  - Values satisfy 0 <= I_channel <= 1 and I_mag normalised

Phase 2:
  - I key cycles I-MAG and I-CHANNELS overlay modes in GL viewer
  - Channel ring brightness visibly correlates with sidecar field values
  - ImGui panel shows per-particle I-vector breakdown on atom selection

Phase 3:
  - ivec_radar.png generated and embedded in report.md
  - I-vector summary table appears in IKK end-tag block
  - LaTeX macros compile without error

Phase 4:
  - .ivec_series.tsv written for runs with sidecar enabled
  - ivec_evolution.png generated per run
  - Viewer scrubber animates I-channel bars on frame step