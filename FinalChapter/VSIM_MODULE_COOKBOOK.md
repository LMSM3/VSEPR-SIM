# VSIM Module Cookbook
## Step-by-step recipes for adding new modules

**Version:** v5.1.13.5  
**Audience:** Anyone adding a new capability to VSEPR-SIM  
**Rule:** VsimRuntime must never change when you add a module. If it does, the pattern is wrong.

---

## Recipe 0 — The Registration Header

Before any recipe works, this header must exist. Create it once.

```cpp
// include/vsim/module_registry.hpp
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

template<typename Interface>
class ModuleRegistry {
public:
    using Factory = std::function<std::unique_ptr<Interface>()>;

    static ModuleRegistry& get() {
        static ModuleRegistry inst;
        return inst;
    }

    void register_module(std::string_view name, Factory f) {
        table_.emplace(std::string(name), std::move(f));
    }

    std::unique_ptr<Interface> create(std::string_view name) const {
        auto it = table_.find(std::string(name));
        return it != table_.end() ? it->second() : nullptr;
    }

    std::vector<std::string> names() const {
        std::vector<std::string> out;
        out.reserve(table_.size());
        for (auto& [k, _] : table_) out.push_back(k);
        return out;
    }

private:
    ModuleRegistry() = default;
    std::unordered_map<std::string, Factory> table_;
};

template<typename Impl, typename Interface>
struct AutoRegister {
    explicit AutoRegister(std::string_view name) {
        ModuleRegistry<Interface>::get().register_module(name,
            [] { return std::make_unique<Impl>(); });
    }
};
```

Add to `CMakeLists.txt` as a header-only target:

```cmake
add_library(vsim_registry INTERFACE)
target_include_directories(vsim_registry INTERFACE ${PROJECT_SOURCE_DIR}/include)
```

Link everything against `vsim_registry`. Done once.

---

## Recipe 1 — New Analysis Metric

Use this when: you want a new string to appear in `[observe] metrics = [...]`.

### 1a. Declare the interface (once, shared by all metrics)

```cpp
// include/vsim/analysis_module.hpp
#pragma once
#include "module_registry.hpp"
#include "vsim_document.hpp"
#include "sim_state.hpp"
#include "analysis_record.hpp"

class IAnalysisModule {
public:
    virtual ~IAnalysisModule() = default;
    virtual std::string_view name() const = 0;
    virtual void configure(const VsimDocument&) {}
    virtual void compute(const SimState&, AnalysisRecord&) = 0;
    virtual void finalize(const SimState& s, AnalysisRecord& r) { compute(s, r); }
    virtual bool post_convergence_only() const { return false; }
};

using AnalysisModuleRegistry = ModuleRegistry<IAnalysisModule>;
```

### 1b. Implement your metric

```cpp
// src/analysis/my_metric.cpp
#include "vsim/analysis_module.hpp"

namespace {   // anonymous namespace prevents linker symbol clash

class MyMetric : public IAnalysisModule {
public:
    std::string_view name() const override { return "my_metric"; }

    void configure(const VsimDocument& doc) override {
        // read any relevant doc fields
    }

    void compute(const SimState& state, AnalysisRecord& out) override {
        // do the work
        // write results to out.my_metric_record (define this in analysis_record.hpp)
    }
};

static AutoRegister<MyMetric, IAnalysisModule> s_reg("my_metric");

} // namespace
```

### 1c. Add to CMakeLists

```cmake
target_sources(vsepr_analysis PRIVATE src/analysis/my_metric.cpp)
```

### 1d. Use it in a script

```toml
[observe]
metrics = ["energy_map", "coordination", "my_metric"]
```

### 1e. Test it

```cpp
TEST(MyMetric, IsRegistered) {
    ASSERT_NE(AnalysisModuleRegistry::get().create("my_metric"), nullptr);
}
TEST(MyMetric, KnownCase) {
    auto mod = AnalysisModuleRegistry::get().create("my_metric");
    // ... assert expected output
}
```

**Total files changed:** 1 new `.cpp` + 1 line in `CMakeLists.txt` + 1 new test file. Nothing else.

---

## Recipe 2 — New Export Format

Use this when: you want a new `write_<format> = true` flag in `[export]`.

### 2a. Interface

```cpp
// include/vsim/output_format_handler.hpp
#pragma once
#include "module_registry.hpp"
#include "vsim_document.hpp"
#include "run_record.hpp"
#include <filesystem>

class IOutputFormatHandler {
public:
    virtual ~IOutputFormatHandler() = default;
    virtual std::string_view format_key() const = 0;
    virtual bool is_enabled(const ExportSection& exp) const = 0;
    virtual void write(const RunRecord&, const ExportSection&,
                       const std::filesystem::path& out_dir) const = 0;
};

using OutputFormatRegistry = ModuleRegistry<IOutputFormatHandler>;
```

### 2b. Implement

```cpp
// src/export/my_format_handler.cpp
#include "vsim/output_format_handler.hpp"
#include <fstream>

namespace {

class MyFormatHandler : public IOutputFormatHandler {
public:
    std::string_view format_key() const override { return "my_format"; }

    bool is_enabled(const ExportSection& exp) const override {
        return exp.write_my_format;   // add this bool to ExportSection
    }

    void write(const RunRecord& rec, const ExportSection& exp,
               const std::filesystem::path& out_dir) const override
    {
        auto path = out_dir / (exp.output_prefix + ".my_format");
        std::ofstream f(path);
        // write rec fields to f
    }
};

static AutoRegister<MyFormatHandler, IOutputFormatHandler> s_reg("my_format");

} // namespace
```

### 2c. Update ExportSection

Add `bool write_my_format = false;` to `ExportSection` in `vsim_document.hpp`. Add the parser key `"write_my_format"` in `VsimParser`. That's the only change to shared infrastructure.

### 2d. Wire into ExportWriter

```cpp
// src/export/export_writer.cpp
void ExportWriter::write_all(const RunRecord& rec, const ExportSection& exp,
                             const fs::path& out_dir)
{
    for (auto& name : OutputFormatRegistry::get().names()) {
        auto handler = OutputFormatRegistry::get().create(name);
        if (handler && handler->is_enabled(exp))
            handler->write(rec, exp, out_dir);
    }
}
```

This loop already exists or is the one to write. It never changes regardless of how many formats are registered.

---

## Recipe 3 — New Excitation Type

Use this when: you want a new `[excite.<type>]` block.

### 3a. Interface

```cpp
// include/vsim/excite_module.hpp
#pragma once
#include "module_registry.hpp"
#include "vsim_document.hpp"
#include "sim_state.hpp"

class IExciteModule {
public:
    virtual ~IExciteModule() = default;
    virtual std::string_view type() const = 0;
    virtual void configure(const ExciteSection& sec, double dt_fs) = 0;
    // Returns false when the excitation is finished
    virtual bool apply(SimState& state, double t_fs) = 0;
};

using ExciteModuleRegistry = ModuleRegistry<IExciteModule>;
```

### 3b. Implement

```cpp
// src/excite/xray_excite_module.cpp
#include "vsim/excite_module.hpp"

namespace {

class XrayExciteModule : public IExciteModule {
public:
    std::string_view type() const override { return "xray"; }

    void configure(const ExciteSection& sec, double dt_fs) override {
        photon_eV_   = sec.photon_energy_eV;
        fluence_     = sec.fluence;
        pulse_width_ = sec.pulse_width_fs;
    }

    bool apply(SimState& state, double t_fs) override {
        if (t_fs > pulse_width_) return false;
        // Apply core-electron excitation proxy forces
        // (simplified: momentum kick proportional to Z and fluence)
        for (int i = 0; i < state.n_atoms; ++i) {
            double kick = fluence_ * state.atomic_number[i] * 0.001;
            state.velocities[i][0] += kick;  // isotropic proxy
        }
        return true;
    }

private:
    double photon_eV_   = 1000.0;
    double fluence_     = 0.0;
    double pulse_width_ = 50.0;
};

static AutoRegister<XrayExciteModule, IExciteModule> s_reg("xray");

} // namespace
```

### 3c. Wire in VsimRuntime (one-time setup, not per-module)

```cpp
// VsimRuntime::setup_run() — written once, never touched again
void VsimRuntime::setup_run(const VsimDocument& doc) {
    excite_modules_.clear();
    for (auto& [type, sec] : doc.excite_sections) {
        auto mod = ExciteModuleRegistry::get().create(type);
        if (mod) {
            mod->configure(sec, doc.run.dt_fs);
            excite_modules_.push_back(std::move(mod));
        } else {
            log_.warn("[EXCITE] Unknown excitation type: {}", type);
        }
    }
}
```

---

## Recipe 4 — New Verify Check

Use this when: you want a new `[verify.<name>]` block.

### 4a. Interface

```cpp
// include/vsim/verify_check.hpp
#pragma once
#include "module_registry.hpp"
#include "vsim_document.hpp"
#include "run_record.hpp"

struct VerifyResult {
    std::string check_name;
    bool        passed   = false;
    std::string detail;
};

class IVerifyCheck {
public:
    virtual ~IVerifyCheck() = default;
    virtual std::string_view name() const = 0;
    virtual bool is_enabled(const VsimDocument& doc) const = 0;
    virtual VerifyResult run(const RunRecord& rec,
                             const VsimDocument& doc) const = 0;
};

using VerifyRegistry = ModuleRegistry<IVerifyCheck>;
```

### 4b. Implement (example: bond angle check)

```cpp
// src/verify/verify_bond_angle.cpp
#include "vsim/verify_check.hpp"
#include <cmath>

namespace {

class VerifyBondAngle : public IVerifyCheck {
public:
    std::string_view name() const override { return "bond_angle"; }

    bool is_enabled(const VsimDocument& doc) const override {
        return doc.verify_bond_angle.enabled;
    }

    VerifyResult run(const RunRecord& rec, const VsimDocument& doc) const override {
        VerifyResult r;
        r.check_name = "bond_angle";

        double mean_angle = rec.analysis.bond_angle_record.mean();
        double expected   = doc.verify_bond_angle.expected_bond_angle_deg;
        double tol        = doc.verify_bond_angle.bond_angle_tolerance_deg;

        r.passed = std::abs(mean_angle - expected) <= tol;
        r.detail = fmt::format(
            "mean={:.2f}° expected={:.2f}° tol=±{:.1f}° → {}",
            mean_angle, expected, tol, r.passed ? "PASS" : "FAIL");
        return r;
    }
};

static AutoRegister<VerifyBondAngle, IVerifyCheck> s_reg("bond_angle");

} // namespace
```

### 4c. Wire VerifyRunner (one-time loop)

```cpp
// src/verify/verify_runner.cpp
VerifyReport VerifyRunner::run_all(const RunRecord& rec, const VsimDocument& doc) {
    VerifyReport report;
    for (auto& name : VerifyRegistry::get().names()) {
        auto check = VerifyRegistry::get().create(name);
        if (check && check->is_enabled(doc))
            report.results.push_back(check->run(rec, doc));
    }
    report.empirical_pass = std::all_of(
        report.results.begin(), report.results.end(),
        [](const VerifyResult& r) { return r.passed; });
    return report;
}
```

---

## Recipe 5 — New `[post_step]` Builtin

Use this when: you want a new function available inside `[post_step] script_block`.

### 5a. Register in PostStepInterpreter

```cpp
// Called once per step, before execute_block()
void PostStepInterpreter::register_builtins(
    const SimState& state, const PbcEngine& pbc)
{
    // Existing builtins
    register_fn("pbc.distance", [&](const Vec3& a, const Vec3& b) {
        return pbc.minimum_image_distance(a, b);
    });
    register_fn("particle.position", [&](int id) {
        return state.positions.at(id - 1);
    });

    // New builtin — add here
    register_fn("particle.kinetic_energy", [&](int id) -> double {
        const Vec3& v = state.velocities.at(id - 1);
        double mass   = state.masses.at(id - 1);
        return 0.5 * mass * v.dot(v);
    });
}
```

The interpreter's `execute_block()` calls registered functions by name. No parser changes needed. The new builtin is available immediately in script blocks:

```toml
[post_step]
enabled = true
script_block = """
ke_1 = particle.kinetic_energy(1)
ke_2 = particle.kinetic_energy(2)
"""
```

---

## Recipe 6 — New Material Registry Entry

Use this when: you want `structure = "my_material"` to resolve in `[material]`.

### 6a. Add to RegistryResolver

```cpp
// src/registry/vsim_registry.cpp
// In the prototype table initializer:
add_proto("my_material", {
    .prototype        = "MY_material",
    .space_group      = "Pm-3m",
    .basis            = "My:0,0,0",
    .generator        = "lattice_builder",
    .coordination     = 6,
    .default_charge   = "formal",
    .is_periodic      = true,
    .material_class   = "ionic",
    .default_forcefield = "ewald_formal",
    .default_radiation  = "xray"
});

// Add alias so "my_alias" resolves to the same prototype
add_alias("my_alias", "MY_material");
```

### 6b. Test the resolution

```cpp
TEST(RegistryResolver, MyMaterialResolves) {
    RegistryResolver r;
    auto bundle = r.resolve("my_material", log_);
    EXPECT_TRUE(bundle.populated);
    EXPECT_EQ(bundle.prototype, "MY_material");
    EXPECT_EQ(bundle.coordination, 6);
}
```

---

## Quick Reference: Which recipe for what

| Goal | Recipe |
|---|---|
| New observable metric (any string in `metrics = [...]`) | Recipe 1 |
| New export file format (`write_X = true`) | Recipe 2 |
| New excitation type (`[excite.X]`) | Recipe 3 |
| New verification check (`[verify.X]`) | Recipe 4 |
| New `post_step` function (`particle.X()`, `pbc.Y()`) | Recipe 5 |
| New material prototype (`structure = "X"`) | Recipe 6 |
| New `run.mode` value | Extend `VsimRuntime::dispatch_run_mode()` switch — this one legitimately touches the runtime |
| New `[section]` with new semantics | Define a new interface + registry; wire the dispatch loop once |

---

## The Rule, Restated

> **VsimRuntime dispatches to registries. It does not know the names of modules.**

If you find yourself writing `if (name == "new_thing") { ... }` inside `VsimRuntime`, `ObservableDispatch`, `ExportWriter`, or `VerifyRunner` — stop. Add a recipe instead. The switch belongs in the new module's `is_enabled()` or in the registry lookup. Not in the dispatcher.

The only legitimate switches in the runtime are for things that are structurally part of the runtime protocol: `run.mode`, parse error handling, and lifecycle events. Everything domain-specific lives in a registered module.
