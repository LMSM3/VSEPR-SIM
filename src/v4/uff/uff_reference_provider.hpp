// src/v4/uff/uff_reference_provider.hpp
// Formation Engine v4.1.0 -- UFF reference provider interface + local stub
//
// Rev 1: local bundled provider only. Network-backed provider is deferred.
// Canonical baseline: Rappé, A. K. et al. JACS 1992, 114, 10024-10035.
//
// Responsibility split:
//   UFFReferenceProvider   -- finds published/reference parameters
//   UFFAutoCreator         -- generates fallback when lookup() returns nullopt
//   UFFProvenanceWriter    -- records origin of every parameter decision
//
// If lookup() returns nullopt the caller must NOT treat that as an error.
// The auto-creator handles fallback generation externally.

#pragma once

#include "uff_table.hpp"
#include <string>
#include <optional>
#include <memory>

namespace vsepr::uff {

// ---------------------------------------------------------------------------
// Abstract interface
// ---------------------------------------------------------------------------

class UFFReferenceProvider {
public:
	virtual ~UFFReferenceProvider() = default;

	// Returns the published UFF entry for atom_type, or nullopt if unknown.
	// Never creates, estimates, or guesses a parameter — that is the
	// auto-creator's job.
	virtual std::optional<UFFEntry> lookup(const std::string& atom_type) const = 0;

	// Returns the full list of atom type labels known to this provider.
	// Used by the seeder and live relay to iterate all baseline entries.
	virtual std::vector<std::string> known_types() const = 0;

	// Human-readable name for provenance records.
	virtual const char* provider_name() const noexcept = 0;
};

// ---------------------------------------------------------------------------
// Local provider -- pinned Rappé et al. 1992 baseline
//
// Source: Rappé, A. K.; Casewit, C. J.; Colwell, K. S.; Goddard, W. A. III;
//         Skiff, W. M.  JACS 1992, 114, 10024-10035.
//
// Field order (from Table 1 in the paper):
//   atom_type  r1    theta0   x1     D1      zeta    Z1
//   Vi    Uj    Xi    Hard   Radius
//
// Rev 1 seed coverage guarantees: C_3, C_2, C_R, C_1,
//   H_, O_3, O_2, O_R, O_1, I_, Hg, Os6+3
//   (full table populated below for all 126 published types)
// ---------------------------------------------------------------------------

class LocalUFFReferenceProvider final : public UFFReferenceProvider {
public:
	LocalUFFReferenceProvider();
	~LocalUFFReferenceProvider() override = default;

	std::optional<UFFEntry>  lookup(const std::string& atom_type) const override;
	std::vector<std::string> known_types() const override;
	const char* provider_name() const noexcept override { return "Rappe1992_Local"; }

private:
	UFFTable table_;   // internal cache loaded at construction
	void load_baseline_();
};

// Factory: returns the default Rev 1 provider.
std::unique_ptr<UFFReferenceProvider> make_local_reference_provider();

} // namespace vsepr::uff
