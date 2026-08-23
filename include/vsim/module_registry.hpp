#pragma once
/**
 * module_registry.hpp  —  WO-74: Self-registering module registry
 * ================================================================
 * Provides a singleton registry that maps string keys to factory functions,
 * plus an AutoRegister<T,Interface> shim for static-initialisation-time
 * self-registration.
 *
 * Usage pattern (one per module .cpp file):
 *
 *   class MyModule : public IAnalysisModule { ... };
 *
 *   // At file scope in MyModule.cpp — runs before main():
 *   static AutoRegister<MyModule, IAnalysisModule> s_reg("my_module");
 *
 * Dispatch in VsimRuntime (no switch/if-chain needed):
 *
 *   for (auto& name : enabled_modules) {
 *       auto mod = ModuleRegistry<IAnalysisModule>::get().create(name);
 *       if (mod) { mod->configure(doc); mod->run(state, record); }
 *   }
 *
 * Thread-safety: registration happens at static-init time (single-threaded);
 * create() is read-only after that, so concurrent lookups are safe.
 */

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vsim {

// ============================================================================
// ModuleRegistry<Interface>
// ============================================================================

template<typename Interface>
class ModuleRegistry {
public:
	using Factory = std::function<std::unique_ptr<Interface>()>;

	static ModuleRegistry& get() {
		static ModuleRegistry inst;
		return inst;
	}

	// Register a factory under a canonical name.
	// Called by AutoRegister at static-init time — must not throw.
	void register_module(std::string_view name, Factory f) {
		table_[std::string(name)] = std::move(f);
	}

	// Create an instance by name; returns nullptr if the name is unknown.
	[[nodiscard]] std::unique_ptr<Interface> create(std::string_view name) const {
		auto it = table_.find(std::string(name));
		return it != table_.end() ? it->second() : nullptr;
	}

	// True iff a module with this name is registered.
	[[nodiscard]] bool has(std::string_view name) const {
		return table_.count(std::string(name)) > 0;
	}

	// All registered names, in unspecified order.
	[[nodiscard]] std::vector<std::string_view> names() const {
		std::vector<std::string_view> out;
		out.reserve(table_.size());
		for (const auto& [k, _] : table_) out.emplace_back(k);
		return out;
	}

	// Total number of registered modules.
	[[nodiscard]] std::size_t size() const { return table_.size(); }

private:
	ModuleRegistry() = default;
	std::unordered_map<std::string, Factory> table_;
};

// ============================================================================
// AutoRegister<T, Interface>
// ============================================================================
// Place one of these at file scope in each module's .cpp:
//
//   static AutoRegister<MyModule, IMyInterface> s_reg("my_module");
//
// The constructor fires at static-init time and inserts the factory into
// ModuleRegistry<IMyInterface>.

template<typename T, typename Interface>
struct AutoRegister {
	explicit AutoRegister(std::string_view name) {
		ModuleRegistry<Interface>::get().register_module(
			name, [] { return std::make_unique<T>(); });
	}
};

} // namespace vsim
