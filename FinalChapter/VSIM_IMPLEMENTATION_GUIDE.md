# VSIM Initial Implementation Guide

**Version:** v5.1.13.5  
**Status:** Active reference — updated as modules are wired  
**Audience:** Kernel developers adding new C++ modules

---

## 1. Implementation Status Audit

Before adding anything new, know what is already real. The table below maps each major `.vsim` section to its implementation status in the C++ kernel.

### Core runtime sections

| Section | Status | C++ location | Notes |
|---|---|---|---|
| `[project]` | ✅ Wired | `VsimDocument::project` | Parsed, seed and determinism applied |
| `[material]` | ✅ Wired | `VsimDocument::material`, `VsimRuntime::resolve_material()` | Registry resolution complete (B9-1 through B9-17) |
| `[run]` | ✅ Wired | `VsimDocument::run`, `VsimRuntime::run_mode_from_string()` | All modes: relax, md, npt, nvt, nve, scan, single_point |
| `[environment]` | ✅ Wired | `VsimDocument::environment` | Parsed; PBC forwarded to integrator |
| `[simulation]` | ✅ Wired | `VsimDocument::simulation`, `FireRelaxer`, `MDIntegrator` | FIRE + Verlet + Langevin |
| `[[simulation.molecule]]` | ✅ Wired | `VsimDocument::molecules` | Multi-molecule support |
| `[cell]` | ✅ Wired | `VsimDocument::cell` | Box dimensions |
| `[boundary]` | ✅ Wired | `VsimDocument::boundary` | periodic / reflective / open per axis |
| `[pbc]` | ✅ Wired | `VsimDocument::pbc`, `PbcEngine` | minimum_image + track_images |
| `[export]` | ✅ Wired | `VsimDocument::export_section`, `ExportWriter` | All standard flags |
| `[visual]` | ✅ Wired | `VsimDocument::visual`, `RenderDispatch` | terminal + GL modes |
| `[kernel]` | ✅ Wired | `VsimDocument::kernel`, `KernelEventLog` | pass_through, symbolic_trace, event_registry |
| `[kernel.trace]` | ✅ Wired | `KernelEventLog::trace_config` | All four trace classes |
| `[report]` | ✅ Wired | `ReportWriter` | All include_* flags |
| `[batch]` | ✅ Wired | `BatchRunner` | Parameter sweep + per-run actions |
| `[while]` | ✅ Wired | `VsimRuntime::run_while_guards()` | Convergence loop |
| `[variance]` | ✅ Wired | `VarianceProbe` | Energy variance window |

### Analysis pipeline sections

| Section | Status | C++ location | Notes |
|---|---|---|---|
| `[analysis.structure]` | ✅ Wired | `structure_inference.hpp`, `S_op` | neighbor graph, coordination, contact |
| `[analysis.sampling]` | ✅ Wired | `property_sampling.hpp`, `P_op` | RDF, MSD |
| `[analysis.scale_sampling]` | ✅ Wired | `scale_sampling.hpp`, `M_op` | field_projection, RVE, emergence |
| `[analysis.inference]` | ✅ Wired | `property_inference.hpp`, `I_op` | rule_based_61b, rule_based_61d |

### Verification sections

| Section | Status | C++ location | Notes |
|---|---|---|---|
| `[verify]` | ✅ Wired | `verify_runner.hpp` | Top-level gate |
| `[verify.structure]` | ✅ Wired | `verify_structure.hpp` | coordination, nearest-neighbor |
| `[verify.rdf]` | ✅ Wired | `verify_rdf.hpp` | peak positions, two-field and list form |
| `[verify.msd]` | ✅ Wired | `verify_msd.hpp` | bounded solid, slope check |
| `[verify.mass]` | ✅ Wired | `verify_mass.hpp` | field_projection.mass_drift_fraction |

### Sections that need wiring (used in example scripts)

| Section | Used in | Status | What's missing |
|---|---|---|---|
| `[excite.laser]` | ikk3, scale_ladder | ⚠️ Parsed, not dispatched | `LaserExciteModule` needs to push perturbation to integrator |
| `[observe] metrics = ["interference"]` | ikk3, wic1 | ⚠️ Key parsed, analysis stub | `InterferenceAnalysis` pass not implemented |
| `[observe] metrics = ["spectral_response"]` | ikk1, ikk3, day75 | ⚠️ Key parsed, analysis stub | `SpectralResponseAnalysis` pass not implemented |
| `[observe] metrics = ["bond_angles"]` | day75 | ⚠️ Key parsed, not computed | `BondAngleAnalysis` — exists in structure pass, not exposed |
| `[observe] metrics = ["formation_trace"]` | day75 modular | ⚠️ Design-only | Requires modular syntax support |
| `[post_step] script_block` | wic1 | ⚠️ Parsed, interpreter stub | `PostStepInterpreter::execute_block()` needs pbc.* builtins |
| `[chemistry]` domain/sequence | — | ✅ Wired | `expand_organic_formula()` complete |
| `[variables]` flat block | day75 flat | ⚠️ Captured in raw_sections | Not a runtime feature; ignored safely |
| `[room]` | gallery | ⚠️ Partially wired | `RoomSolver` exists; `show` directive wiring incomplete |

---

## 2. Priority Wiring Queue

These are the sections actively used in the Day 74–75 examples, ordered by how much of the kernel they touch:

```
Priority 1 (low touch — observation only):
  bond_angles metric          → expose existing structure_inference result
  spectral_response stub      → FFT of energy_map over time window; post-convergence gate

Priority 2 (medium touch — new analysis pass):
  interference metric         → pairwise phase-difference field from velocity vectors
  LaserExciteModule           → apply sinusoidal force perturbation to integrator

Priority 3 (higher touch — interpreter extension):
  post_step pbc.* builtins    → register pbc.distance(), particle.position() in PostStepInterpreter
```

---

## 3. Adding a New Analysis Module

The pattern for every new analysis module is identical. Four steps.

### Step 1 — Define the interface

All analysis modules implement `IAnalysisModule` from `include/vsim/analysis_module.hpp`:

```cpp
// include/vsim/analysis_module.hpp
#pragma once
#include "vsim_document.hpp"
#include "sim_state.hpp"
#include "analysis_record.hpp"

class IAnalysisModule {
public:
    virtual ~IAnalysisModule() = default;

    // Unique key — must match the string used in [observe] metrics list
    virtual std::string_view name() const = 0;

    // Read configuration from the document before the run starts
    virtual void configure(const VsimDocument& doc) {}

    // Called at each observation cadence (every_n_steps)
    virtual void compute(const SimState& state, AnalysisRecord& out) = 0;

    // Called once after the run ends (for post-convergence-only metrics)
    virtual void finalize(const SimState& state, AnalysisRecord& out) {}

    // True = this metric is valid only post-convergence
    virtual bool post_convergence_only() const { return false; }
};
```

### Step 2 — Implement it

Create one `.cpp` file in `src/analysis/`:

```cpp
// src/analysis/bond_angle_analysis.cpp
#include "vsim/analysis_module.hpp"
#include "vsim/module_registry.hpp"
#include "structure_inference.hpp"   // already computes angles; we just expose them
#include <cmath>

class BondAngleAnalysis : public IAnalysisModule {
public:
    std::string_view name() const override { return "bond_angles"; }

    void configure(const VsimDocument& doc) override {
        cutoff_A_ = doc.analysis_structure.neighbor_cutoff_A;
    }

    void compute(const SimState& state, AnalysisRecord& out) override {
        // Walk neighbor graph already built by S_op
        // Compute angle A-center-B for each unique triplet
        // Write mean, stddev, and histogram to out.bond_angle_record
        auto& nbrs = state.neighbor_graph;
        for (int center = 0; center < state.n_atoms; ++center) {
            const auto& ns = nbrs.neighbors_of(center);
            for (int i = 0; i < (int)ns.size(); ++i)
            for (int j = i+1; j < (int)ns.size(); ++j) {
                double angle_deg = compute_angle(
                    state.positions[ns[i]],
                    state.positions[center],
                    state.positions[ns[j]]
                );
                out.bond_angle_record.push(center, angle_deg);
            }
        }
    }

private:
    double cutoff_A_ = 3.5;

    static double compute_angle(
        const Vec3& a, const Vec3& center, const Vec3& b)
    {
        Vec3 u = (a - center).normalized();
        Vec3 v = (b - center).normalized();
        return std::acos(std::clamp(u.dot(v), -1.0, 1.0)) * 180.0 / M_PI;
    }
};

// Self-registration — this is the only line that hooks into the runtime
static AutoRegister<BondAngleAnalysis, IAnalysisModule> s_reg("bond_angles");
```

### Step 3 — Add to CMakeLists

```cmake
# In src/analysis/CMakeLists.txt
target_sources(vsepr_analysis PRIVATE
    bond_angle_analysis.cpp
    # ... existing files
)
```

That is the entire change to the build system. No other file needs touching.

### Step 4 — Verify

The module is now available as `"bond_angles"` in any `[observe]` metrics list. The runtime dispatch in `ObservableDispatch::run_metrics()` iterates `ModuleRegistry<IAnalysisModule>::get()` — it will find the new module automatically.

Write one test:

```cpp
// tests/test_bond_angle_analysis.cpp
TEST(BondAngleAnalysis, CH4TetrahedralAngle) {
    auto mod = AnalysisModuleRegistry::get().create("bond_angles");
    ASSERT_NE(mod, nullptr);

    SimState state = build_ch4_ground_state();   // test fixture
    AnalysisRecord rec;
    mod->configure(make_ch4_doc());
    mod->compute(state, rec);

    double mean_angle = rec.bond_angle_record.mean();
    EXPECT_NEAR(mean_angle, 109.47, 1.0);
}
```

---

## 4. Wiring `[excite.laser]`

The laser section is parsed correctly (ExciteSection populated). It is not yet dispatched to the integrator. Here is the minimum viable wiring.

### What needs to happen

At each MD timestep `t`, if `t` falls within the pulse window, add a sinusoidal force to each atom along the excitation axis, modulated by the pulse profile.

### Interface

```cpp
// include/vsim/excite_module.hpp
#pragma once
#include "vsim_document.hpp"
#include "sim_state.hpp"

class IExciteModule {
public:
    virtual ~IExciteModule() = default;
    virtual std::string_view type() const = 0;

    // Called once before the run; reads ExciteSection config
    virtual void configure(const ExciteSection& sec, double dt_fs) = 0;

    // Called at each step; adds forces to state.forces[]
    // Returns false when the excitation has ended (pulse finished)
    virtual bool apply(SimState& state, double t_fs) = 0;
};
```

### Laser implementation

```cpp
// src/excite/laser_excite_module.cpp
#include "vsim/excite_module.hpp"
#include "vsim/module_registry.hpp"
#include <cmath>

class LaserExciteModule : public IExciteModule {
public:
    std::string_view type() const override { return "laser"; }

    void configure(const ExciteSection& sec, double dt_fs) override {
        axis_         = axis_index(sec.axis);           // 0=x,1=y,2=z
        intensity_    = sec.intensity;
        pulse_width_  = sec.pulse_width_fs;
        photon_eV_    = sec.photon_energy_eV;
        profile_      = sec.profile;                    // "gaussian","flat","sech2"
        start_t_      = 0.0;                            // fires at t=0
    }

    bool apply(SimState& state, double t_fs) override {
        double envelope = pulse_envelope(t_fs);
        if (envelope < 1e-9) return t_fs < (start_t_ + pulse_width_ * 4.0);

        // Sinusoidal carrier: ω = photon_eV_ * eV_to_rad_per_fs
        double omega    = photon_eV_ * 0.15193;        // eV → rad/fs
        double carrier  = std::cos(omega * t_fs);
        double force_au = intensity_ * envelope * carrier;

        for (int i = 0; i < state.n_atoms; ++i)
            state.forces[i][axis_] += force_au;

        return true;
    }

private:
    int    axis_        = 2;
    double intensity_   = 1.0;
    double pulse_width_ = 100.0;
    double photon_eV_   = 0.0;
    double start_t_     = 0.0;
    std::string profile_ = "gaussian";

    double pulse_envelope(double t_fs) const {
        double center = start_t_ + pulse_width_ * 2.0;
        double sigma  = pulse_width_ / 2.355;   // FWHM → sigma
        if (profile_ == "flat")
            return (t_fs >= start_t_ && t_fs <= start_t_ + pulse_width_) ? 1.0 : 0.0;
        if (profile_ == "sech2") {
            double x = (t_fs - center) / sigma;
            return 1.0 / std::cosh(x) / std::cosh(x);
        }
        // gaussian (default)
        double x = (t_fs - center) / sigma;
        return std::exp(-0.5 * x * x);
    }

    static int axis_index(const std::string& s) {
        if (s == "x") return 0;
        if (s == "y") return 1;
        return 2;   // "z" default
    }
};

static AutoRegister<LaserExciteModule, IExciteModule> s_reg("laser");
```

### Wiring into the MD loop

In `MDIntegrator::step()`, before the force integration:

```cpp
// MDIntegrator.cpp
void MDIntegrator::step(SimState& state, double t_fs) {
    // Apply any active excitation modules
    for (auto& excite : excite_modules_) {
        bool still_active = excite->apply(state, t_fs);
        if (!still_active) mark_for_removal(excite);
    }

    // Normal Verlet / Langevin integration
    integrate(state);
}
```

`excite_modules_` is populated in `VsimRuntime::setup_run()` by iterating `doc.excite_sections` and looking up each type in `ExciteModuleRegistry`.

---

## 5. Wiring `spectral_response`

The minimum viable implementation is an FFT of the total kinetic energy signal over a rolling time window, sampled at each observation cadence. This produces a frequency spectrum that reflects the phonon structure — which is the wave-observable post-convergence claim in IKK-I and IKK-III.

### Requirements

- Only valid when `compute_rdf = true` (needs a converged structure)
- Should be gated: compute only after convergence flag is set
- Output: frequency bins (THz) + amplitude array written to `AnalysisRecord`

### Implementation sketch

```cpp
// src/analysis/spectral_response_analysis.cpp
#include "vsim/analysis_module.hpp"
#include "vsim/module_registry.hpp"
#include <complex>
#include <vector>

class SpectralResponseAnalysis : public IAnalysisModule {
public:
    std::string_view name() const override { return "spectral_response"; }
    bool post_convergence_only() const override { return true; }

    void configure(const VsimDocument& doc) override {
        window_steps_ = 256;    // FFT window; configurable later
        dt_fs_        = doc.run.dt_fs;
    }

    void compute(const SimState& state, AnalysisRecord& out) override {
        // Accumulate kinetic energy buffer
        double ke = state.total_kinetic_energy();
        ke_buffer_.push_back(ke);
        if ((int)ke_buffer_.size() > window_steps_)
            ke_buffer_.erase(ke_buffer_.begin());
    }

    void finalize(const SimState& state, AnalysisRecord& out) override {
        if ((int)ke_buffer_.size() < window_steps_ / 2) return;
        auto spectrum = fft_real(ke_buffer_);

        double df_THz = 1000.0 / (dt_fs_ * ke_buffer_.size());
        out.spectral_record.freq_THz.clear();
        out.spectral_record.amplitude.clear();
        for (int k = 0; k < (int)spectrum.size() / 2; ++k) {
            out.spectral_record.freq_THz.push_back(k * df_THz);
            out.spectral_record.amplitude.push_back(std::abs(spectrum[k]));
        }
    }

private:
    int    window_steps_ = 256;
    double dt_fs_        = 1.0;
    std::vector<double> ke_buffer_;

    // Simple DFT (replace with FFTW or kissfft for production)
    static std::vector<std::complex<double>>
    fft_real(const std::vector<double>& x) {
        int N = x.size();
        std::vector<std::complex<double>> out(N);
        for (int k = 0; k < N; ++k)
            for (int n = 0; n < N; ++n)
                out[k] += x[n] * std::polar(1.0, -2.0 * M_PI * k * n / N);
        return out;
    }
};

static AutoRegister<SpectralResponseAnalysis, IAnalysisModule> s_reg("spectral_response");
```

The `post_convergence_only()` override ensures `ObservableDispatch` only calls `finalize()` on this module, never `compute()` mid-run. That enforces the IKK-I rule automatically.

---

## 6. Wiring `[post_step]` Builtins

`[post_step]` is parsed; `PostStepInterpreter::execute_block()` exists but the `pbc.*` and `particle.*` builtins are not registered. Used in `wic1_identity_vector_trajectories_graphene.vsim`.

### Builtin registration

```cpp
// src/runtime/post_step_interpreter.cpp

void PostStepInterpreter::register_builtins(
    const SimState& state, const PbcEngine& pbc)
{
    // pbc.distance(pos_a, pos_b) → double (Å)
    register_function("pbc.distance",
        [&pbc](const Vec3& a, const Vec3& b) -> double {
            return pbc.minimum_image_distance(a, b);
        });

    // particle.position(id) → Vec3 (1-indexed, as in script)
    register_function("particle.position",
        [&state](int id) -> Vec3 {
            return state.positions.at(id - 1);   // 1-indexed → 0-indexed
        });

    // particle.velocity(id) → Vec3
    register_function("particle.velocity",
        [&state](int id) -> Vec3 {
            return state.velocities.at(id - 1);
        });

    // particle.charge(id) → double
    register_function("particle.charge",
        [&state](int id) -> double {
            return state.charges.at(id - 1);
        });
}
```

### Interpreter execution per step

```cpp
// In MDIntegrator::step(), after integration:
if (doc_.post_step.enabled && !doc_.post_step.script_block.empty()) {
    post_step_interp_.register_builtins(state, pbc_engine_);
    post_step_interp_.execute_block(doc_.post_step.script_block);
    // Results stored in PostStepRecord, appended to event log
}
```

### Script block result capture

The interpreter should write scalar results to `KernelEventLog` with step index and variable name. That makes `d_12` and `d_13` (from the WIC-I script) appear in the `events_json` output automatically.

---

## 7. Build System Integration

### Adding a new analysis module

```cmake
# src/analysis/CMakeLists.txt
add_library(vsepr_analysis STATIC
    structure_inference.cpp
    property_sampling.cpp
    scale_sampling.cpp
    property_inference.cpp
    bond_angle_analysis.cpp         # new
    spectral_response_analysis.cpp  # new
    interference_analysis.cpp       # new (when implemented)
)
target_include_directories(vsepr_analysis PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
```

### Adding a new excite module

```cmake
# src/excite/CMakeLists.txt
add_library(vsepr_excite STATIC
    laser_excite_module.cpp    # new
    xray_excite_module.cpp     # future
)
target_link_libraries(vsepr_excite PRIVATE vsepr_core)
```

Link order in the main executable:

```cmake
target_link_libraries(vsper PRIVATE
    vsepr_core
    vsepr_continual
    vsepr_analysis
    vsepr_excite        # add when excite modules exist
    vsepr_verify
    vsepr_export
)
```

### No changes to VsimRuntime

This is the invariant to maintain. `VsimRuntime` dispatches through the registries. It must never contain `if (section == "bond_angles")` branches. If you find yourself adding one, add the module to its registry instead.

---

## 8. Test Harness Pattern

Every new module needs one test file. The pattern is uniform:

```cpp
// tests/test_<module_name>.cpp
#include <gtest/gtest.h>
#include "vsim/module_registry.hpp"

// --- Registration check ---
TEST(<ModuleName>, RegisteredInRegistry) {
    auto mod = AnalysisModuleRegistry::get().create("<module_key>");
    ASSERT_NE(mod, nullptr) << "Module not registered: <module_key>";
    EXPECT_EQ(mod->name(), "<module_key>");
}

// --- Known-good case ---
TEST(<ModuleName>, KnownGoodCase) {
    auto mod = AnalysisModuleRegistry::get().create("<module_key>");
    SimState state = TestFixtures::build_<canonical_system>();
    AnalysisRecord rec;
    mod->configure(TestFixtures::make_<canonical_system>_doc());
    mod->compute(state, rec);
    // Assert expected output values
    EXPECT_NEAR(rec.<relevant_field>, <expected_value>, <tolerance>);
}

// --- Null/empty input ---
TEST(<ModuleName>, EmptyStateDoesNotCrash) {
    auto mod = AnalysisModuleRegistry::get().create("<module_key>");
    SimState empty;
    AnalysisRecord rec;
    EXPECT_NO_THROW(mod->compute(empty, rec));
}
```

### Canonical test fixtures

Define these once in `tests/test_fixtures.hpp` and reuse across all module tests:

| Fixture | Description | Key values |
|---|---|---|
| `TestFixtures::build_ch4_ground_state()` | CH4 tetrahedral, ground state | C-H = 1.09 Å, angle = 109.47° |
| `TestFixtures::build_nacl_rocksalt_4x4x4()` | NaCl 4×4×4, relaxed | coord = 6, peak = 2.82 Å |
| `TestFixtures::build_si_diamond_6x6x6()` | Si 6×6×6, 300 K | coord = 4, peak = 2.35 Å |
| `TestFixtures::build_fe_bcc_8x8x8()` | Fe BCC 8×8×8, 300 K | coord = 8, peak = 2.48 Å |
| `TestFixtures::build_graphene_8x8x1()` | Graphene monolayer | coord = 3, peak = 1.42 Å |
| `TestFixtures::build_n2_gas_box()` | 4×N2 in 15 Å box | coord = 1, bond = 1.10 Å |

These fixtures are also used directly by the `.vsim` example scripts' verification passes — keeping test values consistent across the C++ unit tests and the integration test scripts.

---

## 9. Common Failure Modes

| Symptom | Most likely cause | Fix |
|---|---|---|
| Module returns `nullptr` from registry | `AutoRegister` in `.cpp` was never compiled (translation unit not in CMakeLists) | Add `.cpp` to `target_sources` |
| Observable key appears in output JSON with empty data | `compute()` called but record field never populated | Check that `AnalysisRecord` field is initialized before appending |
| `post_convergence_only` metric appears mid-run | `ObservableDispatch` calling `compute()` instead of only `finalize()` | Check `post_convergence_only()` override returns `true` |
| `excite.laser` parsed but no force applied | `ExciteModuleRegistry` never queried in `setup_run()` | Wire `doc.excite_sections` → registry lookup in `VsimRuntime::setup_run()` |
| `pbc.distance()` in post_step throws | Builtins not registered before `execute_block()` | Call `register_builtins(state, pbc)` at top of step dispatch |
| Duplicate symbol linker error after adding module | Two `.cpp` files define same `AutoRegister` variable name `s_reg` | Wrap in anonymous namespace: `namespace { static AutoRegister... s_reg; }` |
| Scale sampling `min_particles_for_scale_sampling` gate fails silently | System smaller than threshold, `ScaleSampleRecord` not produced, `macro_ready` blocked | Lower threshold for small systems in the test fixture doc |

---

## 10. Day 74 / Day 75 Wiring Checklist

### Day 74 — WO-74A/B/C prework

```
[ ] Run dumpbin/nm on gap_classifier.cpp.obj and gap_classifier_wo74c.cpp.obj
    → Confirm symbol overlap (Scenario A vs B)
[ ] Define IGapClassifier interface
[ ] Wrap both classifiers as registered implementations
[ ] Update test_gap_classifier to iterate registry
[ ] Define IOutputFormatHandler interface
[ ] Migrate xyz, json, svg, tsv handlers to registered implementations
[ ] Add scale-layer gating as a property of IOutputFormatHandler
[ ] Register 3 analytic length_scale_fitter cases as test fixtures
[ ] All 3 fitter cases pass test_length_scale_fitter.cpp
```

### Day 75 — WO-75A gate

```
[ ] day75_ch4_flat_supported.vsim runs without parser errors
[ ] verify.structure passes: coord=4, bond=1.09±0.06, angle=109.47±5°
[ ] verify.mass passes
[ ] spectral_response present in output JSON (even if stub with empty bins)
[ ] determinism: 3 runs same seed → identical verify hash
[ ] symbolic_trace_json written
[ ] WO-75A accepted → begin WO-75B modular syntax work
```
