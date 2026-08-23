#pragma once
/**
 * include/vsim/excite_module.hpp  -  IExciteModule interface
 * ===========================================================
 *
 * Abstract interface for excitation source modules (laser, xray,
 * electron_beam, thermal_spike).  Each module reads an ExciteEntry
 * configuration, then applies a force perturbation per MD step.
 *
 * Registration (in each concrete .cpp):
 *
 *   #include "vsim/module_registry.hpp"
 *   static vsim::AutoRegister<LaserExciteModule, IExciteModule> s_reg("laser");
 *
 * Runtime dispatch (in cmd_run_vsim.cpp step loop):
 *
 *   for (auto& [name, entry] : doc.excite.entries) {
 *       auto mod = vsim::ModuleRegistry<IExciteModule>::get().create(name);
 *       if (mod) { mod->configure(entry, doc.run.dt_fs); }
 *   }
 *   // ... per step: mod->apply(step_t_fs)  returns false when pulse ends
 *
 * Reference: FinalChapter/VSIM_IMPLEMENTATION_GUIDE.md §4
 * WO-75B  |  v5.0.0-main
 */

#include <string>
#include <string_view>

// Forward declarations
namespace vsim { struct ExciteEntry; }

// ============================================================================
// IExciteModule
// ============================================================================

class IExciteModule {
public:
	virtual ~IExciteModule() = default;

	// Registry key — must match the ExciteEntry type string ("laser", "xray", ...)
	[[nodiscard]] virtual std::string_view type() const = 0;

	// Called once before the run loop; reads excite entry config + timestep.
	virtual void configure(const vsim::ExciteEntry& entry, double dt_fs) = 0;

	// Called each step; returns false when the excitation pulse has ended.
	// Implementations should record what they would do — in the current
	// synthetic pipeline there are no force arrays, so this returns a
	// scalar intensity value that can be logged to KernelEventLog.
	[[nodiscard]] virtual double intensity_at(double t_fs) const = 0;

	// Human-readable summary for vsepr doctor / [visual] banner.
	[[nodiscard]] virtual std::string_view description() const { return type(); }

	// True while the excitation is active for the given time.
	[[nodiscard]] virtual bool active_at(double t_fs) const {
		return intensity_at(t_fs) > 1e-9;
	}
};
