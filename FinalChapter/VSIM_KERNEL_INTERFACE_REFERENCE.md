# VSIM Kernel Interface Reference
## VSIM section → C++ header, struct, and runtime entry point

**Version:** v5.1.13.5  
**Purpose:** When a `.vsim` section is parsed, what C++ object owns it and what function processes it?

---

## How the pipeline works

```
.vsim file
    │
    ▼
VsimParser::parse(path)
    │  builds VsimDocument
    ▼
VsimDocument
    │  all parsed fields as plain structs
    ▼
VsimRuntime::run(doc)
    │
    ├─ resolve_material(doc)       → RegistryResolver → RegistryBundle
    ├─ apply_registry_defaults()   → fills doc gaps from RegistryBundle
    ├─ setup_run()                 → initialises integrators, excite modules
    ├─ build_geometry()            → GeometryBuilder → SimState (initial)
    ├─ dispatch_run_mode()         → FireRelaxer / MDIntegrator / SinglePoint
    │      │
    │      ├─ per-step:
    │      │    ├─ integrate()
    │      │    ├─ apply excite modules
    │      │    ├─ ObservableDispatch::tick()
    │      │    └─ PostStepInterpreter::execute_block()
    │      │
    │      └─ on convergence / end:
    │           ├─ ObservableDispatch::finalize()
    │           └─ KernelEventLog::close()
    │
    ├─ run_analysis_pipeline()
    │      ├─ S_op  (structure_inference)
    │      ├─ P_op  (property_sampling)
    │      ├─ M_op  (scale_sampling)
    │      └─ I_op  (property_inference)
    │
    ├─ VerifyRunner::run_all()
    ├─ ExportWriter::write_all()
    └─ ReportWriter::write()
```

---

## Section-by-section reference

### `[project]`

```
Struct:       VsimDocument::ProjectSection project
Fields:       name, version, seed_base, determinism
Parser key:   "project"
Runtime use:  VsimRuntime::apply_seed(doc.project.seed_base)
              RunRecord::run_name = doc.project.name
Header:       include/vsim/vsim_document.hpp
```

---

### `[material]`

```
Struct:       VsimDocument::MaterialSection material
Fields:       formula, prototype, structure, space_group, lattice, basis, cell, phase
Parser key:   "material"
Runtime use:  VsimRuntime::resolve_material(doc, log)
              → RegistryResolver::resolve(mat, log) → RegistryBundle
              → VsimRuntime::apply_registry_defaults(bundle, doc)
Header:       include/vsim/vsim_document.hpp
              include/vsim/vsim_registry.hpp   (RegistryResolver, RegistryBundle)
Entry point:  VsimRuntime::resolve_material()
```

**RegistryBundle fields populated by resolve_material:**

```cpp
struct RegistryBundle {
    std::string prototype;
    std::string space_group;
    std::string basis;
    std::string generator;
    int         coordination      = 0;
    std::string default_charge_model;
    bool        is_periodic       = false;
    bool        populated         = false;
    std::string material_class;
    std::string default_run_mode;
    std::string default_medium;
    double      default_temperature = 300.0;
    double      default_pressure    = 0.0;
    std::string default_solver;
    std::string default_forcefield;
    std::string default_observables;
    std::string default_export_profile;
    std::string geometry_source;
    std::string default_radiation;
};
```

---

### `[run]`

```
Struct:       VsimDocument::RunSection run
Fields:       mode, max_steps, dt_fs, temperature, pressure, converge, output_level
Parser key:   "run"
Runtime use:  VsimRuntime::dispatch_run_mode(doc)
              → FireRelaxer, MDIntegrator, SinglePointEval
Header:       include/vsim/vsim_document.hpp
Entry point:  VsimRuntime::dispatch_run_mode()
```

**Mode dispatch:**

```cpp
// VsimRuntime::dispatch_run_mode()
if      (doc.run.mode == "relax")        run_fire_relax(doc);
else if (doc.run.mode == "md")           run_md(doc);
else if (doc.run.mode == "single_point") run_single_point(doc);
else if (doc.run.mode == "npt")          run_npt(doc);
// ... etc
// This switch is legitimate — it is structural protocol, not module dispatch
```

---

### `[environment]`

```
Struct:       VsimDocument::EnvironmentSection environment
Fields:       periodic, temperature, pressure, medium, humidity, field_x/y/z
Parser key:   "environment"
Runtime use:  Forwarded to MDIntegrator thermostat and PbcEngine
Header:       include/vsim/vsim_document.hpp
```

---

### `[pbc]`

```
Struct:       VsimDocument::PbcSection pbc
Fields:       minimum_image, track_images
Parser key:   "pbc"
Runtime use:  PbcEngine::configure(doc.pbc)
Header:       include/vsim/vsim_document.hpp
              include/vsim/pbc_engine.hpp
Entry point:  PbcEngine::configure()
Key method:   PbcEngine::minimum_image_distance(a, b)
              PbcEngine::wrap(position, box)
              PbcEngine::image_count(particle_id)
```

---

### `[simulation]`

```
Struct:       VsimDocument::SimulationSection simulation
Fields:       fire_max_steps, fire_dt_fs, box_size_ang, periodic,
              formation_preset, use_ewald, ewald_alpha, ewald_rcut,
              ewald_kmax, step_delay_ms
Parser key:   "simulation"
Runtime use:  FireRelaxer::configure(doc.simulation)
              EwaldSummation::configure(doc.simulation)
Header:       include/vsim/vsim_document.hpp
              include/vsim/fire_relaxer.hpp
              include/vsim/ewald_summation.hpp
```

---

### `[[simulation.molecule]]`

```
Struct:       std::vector<VsimDocument::MoleculeEntry> molecules
Fields:       formula, count, lattice, temperature, layer_mode, n_layers
Parser key:   "simulation.molecule" (array-of-tables)
Runtime use:  GeometryBuilder::build(doc.molecules, doc.simulation)
Header:       include/vsim/vsim_document.hpp
              include/vsim/geometry_builder.hpp
Entry point:  GeometryBuilder::build() → SimState (positions, species, masses, charges)
```

---

### `[excite.<type>]`

```
Struct:       std::map<std::string, ExciteSection> excite_sections
              Key = type string ("laser", "xray")
Fields:       axis, polarization, intensity, pulse_width_fs,
              photon_energy_eV, fluence, profile
Parser key:   "excite.*" (wildcard subsection)
Runtime use:  VsimRuntime::setup_run() iterates excite_sections,
              looks up each type in ExciteModuleRegistry,
              calls configure() + stores modules in excite_modules_
              MDIntegrator::step() calls apply() on each module per step
Header:       include/vsim/vsim_document.hpp
              include/vsim/excite_module.hpp
Entry point:  VsimRuntime::setup_run()   (wires modules)
              MDIntegrator::step()        (fires per step)
Status:       ⚠️ Parsed; module dispatch loop needs implementation
```

---

### `[observe]`

```
Struct:       VsimDocument::ObserveSection observe
Fields:       metrics (list), output_format, every_n_steps
Parser key:   "observe"
Runtime use:  ObservableDispatch::configure(doc.observe)
              Iterates metrics list → AnalysisModuleRegistry::get().create(metric)
              Calls compute() every every_n_steps steps
              Calls finalize() on post_convergence_only modules after run ends
Header:       include/vsim/vsim_document.hpp
              include/vsim/observable_dispatch.hpp
              include/vsim/analysis_module.hpp
Entry point:  ObservableDispatch::configure()   (pre-run)
              ObservableDispatch::tick(step)     (per step)
              ObservableDispatch::finalize()     (post-run)
```

**ObservableDispatch tick logic:**

```cpp
void ObservableDispatch::tick(int step, const SimState& state) {
    if (step % cadence_ != 0) return;
    for (auto& mod : modules_) {
        if (!mod->post_convergence_only())
            mod->compute(state, record_);
    }
}
void ObservableDispatch::finalize(const SimState& state) {
    for (auto& mod : modules_)
        mod->finalize(state, record_);
}
```

---

### `[kernel]` and `[kernel.trace]`

```
Struct:       VsimDocument::KernelSection kernel
Fields:       pass_through, symbolic_trace, event_registry, continual_reporting
              trace.formation_events, trace.defect_events,
              trace.transport_events, trace.dynamic_energy
Parser key:   "kernel", "kernel.trace"
Runtime use:  KernelEventLog::configure(doc.kernel)
              KernelEventLog::record_formation_event(step, data)
              KernelEventLog::record_dynamic_energy(step, E)
              ... etc
Header:       include/vsim/kernel_event_log.hpp
Entry point:  KernelEventLog::configure()
Key methods:  KernelEventLog::record_*()
              KernelEventLog::write_events_json(path)
              KernelEventLog::write_symbolic_trace_json(path)
```

---

### `[post_step]`

```
Struct:       VsimDocument::PostStepSection post_step
Fields:       enabled, script_block
Parser key:   "post_step"
Runtime use:  PostStepInterpreter::register_builtins(state, pbc)  (per step)
              PostStepInterpreter::execute_block(script_block)      (per step)
              Results appended to KernelEventLog
Header:       include/vsim/post_step_interpreter.hpp
Entry point:  PostStepInterpreter::register_builtins()
              PostStepInterpreter::execute_block()
Status:       ⚠️ Parsed; pbc.* and particle.* builtins not yet registered
```

---

### `[analysis.structure]`

```
Struct:       VsimDocument::AnalysisStructureSection analysis_structure
Fields:       enabled, neighbor_cutoff_A, contact_cutoff_A
Parser key:   "analysis.structure"
C++ operator: S_op — StructureInference
Header:       include/vsim/structure_inference.hpp
Record:       StructureInferenceResult
  - coordination_numbers[]
  - neighbor_graph
  - nearest_neighbor_distances[]
  - contact_pairs
Entry point:  StructureInference::run(state, doc.analysis_structure)
```

---

### `[analysis.sampling]`

```
Struct:       VsimDocument::AnalysisSamplingSection analysis_sampling
Fields:       enabled, compute_rdf, compute_msd, min_frames_for_motion,
              min_frames_for_msd, unwrap_pbc
Parser key:   "analysis.sampling"
C++ operator: P_op — PropertySampling
Header:       include/vsim/property_sampling.hpp
Record:       PropertySampleRecord
  - rdf_bins[], rdf_values[]
  - msd_values[]
  - diffusion_proxy_A2_per_frame
Entry point:  PropertySampling::run(trajectory, doc.analysis_sampling)
```

---

### `[analysis.scale_sampling]`

```
Struct:       VsimDocument::AnalysisScaleSamplingSection analysis_scale_sampling
Fields:       enabled, compute_field_projection, compute_rve_sampling,
              compute_emergence_metrics, field_grid[3], rve_window_lengths_A[],
              rve_windows_per_level, rve_window_placement,
              min_particles_for_scale_sampling,
              spatial_cv_threshold, temporal_drift_threshold,
              scale_drift_threshold, temporal_drift_metric, scale_drift_metric
Parser key:   "analysis.scale_sampling"
C++ operator: M_op — ScaleSampling
Header:       include/vsim/scale_sampling.hpp
Record:       ScaleSampleRecord
  - field_projection.mass_conserved
  - field_projection.mass_drift_fraction
  - rve_windows[]: { length_A, spatial_cv, temporal_drift, scale_drift }
  - emergence_candidate
Entry point:  ScaleSampling::run(trajectory, doc.analysis_scale_sampling)
```

---

### `[analysis.inference]`

```
Struct:       VsimDocument::AnalysisInferenceSection analysis_inference
Fields:       enabled, mode
Parser key:   "analysis.inference"
C++ operator: I_op — PropertyInference
Header:       include/vsim/property_inference.hpp
Record:       PropertyInferenceRecord
  - macro_ready
  - macro_proxy_ready (deprecated)
  - inference_mode
  - blocking_reasons[]
Entry point:  PropertyInference::run(struct_result, sample_result,
                                     scale_result, doc.analysis_inference)
```

---

### `[verify]` family

```
Interface:    IVerifyCheck (include/vsim/verify_check.hpp)
Registry:     VerifyRegistry
Entry point:  VerifyRunner::run_all(rec, doc)
Output:       VerifyReport → write_verify_report.json, write_verify_summary.tsv

Check key     → Struct                    → Header
"structure"   → VerifyStructureSection    → verify_structure.hpp
"rdf"         → VerifyRdfSection          → verify_rdf.hpp
"msd"         → VerifyMsdSection          → verify_msd.hpp
"mass"        → VerifyMassSection         → verify_mass.hpp
```

---

### `[export]`

```
Struct:       VsimDocument::ExportSection export_section
Header:       include/vsim/vsim_document.hpp
Interface:    IOutputFormatHandler (include/vsim/output_format_handler.hpp)
Registry:     OutputFormatRegistry
Entry point:  ExportWriter::write_all(rec, doc.export_section, out_dir)

Flag                           → Format handler key
write_xyz                      → "xyz"
write_xyzf                     → "xyzf"
write_analysis_json            → "analysis_json"
write_metrics_tsv              → "metrics_tsv"
write_report_md                → "report_md"
write_events_json              → "events_json"
write_symbolic_trace_json      → "symbolic_trace_json"
write_dashboard_svg            → "dashboard_svg"
write_pipeline_audit_jsonl     → "pipeline_audit_jsonl"
write_manifest_json            → "manifest_json"
write_scale_sampling_json      → "scale_sampling_json"
write_verify_report            → "verify_report_json"
write_verify_tsv               → "verify_tsv"
```

---

### `[batch]` and `[[batch.job]]`

```
Struct:       VsimDocument::BatchSection batch
Header:       include/vsim/vsim_document.hpp
Entry point:  BatchRunner::run(doc)
              Iterates doc.batch.jobs
              For each job: merges job-local sections over base doc
              Calls VsimRuntime::run(merged_doc)
              Collects results into BatchRecord
```

---

### `[while]`

```
Struct:       VsimDocument::WhileSection while_section
Fields:       name, condition, max_iters, body_steps, measure, iter_delay_ms
Parser key:   "while"
Entry point:  VsimRuntime::run_while_guards(doc)
Condition     evaluates against VarianceProbe results
Header:       include/vsim/vsim_runtime.hpp
```

---

### `[variance]`

```
Struct:       std::map<std::string, VarianceSpec> variance_specs
Fields:       probe name → { metric, window, threshold }
Parser key:   "variance"
Entry point:  VarianceProbe::evaluate(name, event_log)
Header:       include/vsim/variance_probe.hpp
```

---

## SimState — the live simulation object

All kernel operations read from and write to `SimState`. This is the in-memory representation of the running system.

```cpp
// include/vsim/sim_state.hpp
struct SimState {
    int                      n_atoms = 0;
    std::vector<Vec3>        positions;
    std::vector<Vec3>        velocities;
    std::vector<Vec3>        forces;
    std::vector<double>      masses;
    std::vector<double>      charges;
    std::vector<int>         atomic_number;
    std::vector<std::string> species;

    // Box
    Vec3  box_lengths  = {0,0,0};
    bool  periodic[3]  = {false,false,false};

    // Live scalars
    double total_energy          = 0.0;
    double total_kinetic_energy  = 0.0;
    double total_potential_energy = 0.0;
    double temperature_K         = 0.0;
    double pressure_GPa          = 0.0;
    int    current_step          = 0;
    bool   converged             = false;

    // Neighbor graph (built by S_op, available to all later modules)
    NeighborGraph neighbor_graph;
};
```

---

## AnalysisRecord — the analysis output accumulator

All analysis modules write into `AnalysisRecord`. It is passed by reference through the full analysis pipeline and serialized to JSON by `ExportWriter`.

```cpp
// include/vsim/analysis_record.hpp  (selected fields)
struct AnalysisRecord {
    StructureInferenceResult   structure;
    PropertySampleRecord       sampling;
    ScaleSampleRecord          scale_sampling;
    PropertyInferenceRecord    inference;

    // Observable dispatch results
    BondAngleRecord            bond_angle_record;   // Recipe 1 example
    SpectralRecord             spectral_record;
    InterferenceRecord         interference_record;

    // Post-step interpreter results
    std::vector<PostStepEntry> post_step_log;
};
```

When you add a new analysis module (Recipe 1), add its record type here and populate it in `compute()` / `finalize()`.
