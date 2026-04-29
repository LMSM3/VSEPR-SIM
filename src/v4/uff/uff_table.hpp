// src/v4/uff/uff_table.hpp
// Formation Engine v4.1.0 -- Live in-memory UFF parameter table
//
// Rev 1 architecture: runtime table + CSV/JSONL audit logs.
// Canonical baseline source: Rappé et al. 1992 (JACS 114, 10024-10035).
//
// UFF atom types encode element + coordination + environment.
// "C" is not a universal carbon blob; C_3, C_2, C_R, etc. are distinct entries.

#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace vsepr::uff {

// ---------------------------------------------------------------------------
// Key: identifies a UFF atom type entry (element + UFF label + environment)
// ---------------------------------------------------------------------------

struct UFFAtomTypeKey {
	std::string element;          // "C", "O", "Hg", "Os", etc.
	std::string atom_type;        // UFF label: "C_3", "O_2", "Hg", "Os6+3", etc.
	int         coordination_number = 0;
	std::string geometry_tag;     // "tetrahedral", "octahedral", "linear", etc.

	bool operator==(const UFFAtomTypeKey& o) const noexcept {
		return atom_type == o.atom_type;
	}
};

struct UFFAtomTypeKeyHash {
	std::size_t operator()(const UFFAtomTypeKey& k) const noexcept {
		return std::hash<std::string>{}(k.atom_type);
	}
};

// ---------------------------------------------------------------------------
// Entry: one row of UFF parameters (Rappé et al. 1992 field set)
// ---------------------------------------------------------------------------

// Confidence tag tracks parameter origin for provenance output.
enum class ParamConfidence : std::uint8_t {
	Published,   // from a peer-reviewed source (e.g., Rappé 1992)
	Derived,     // computed from published rules (UFF mixing / interpolation)
	Estimated,   // auto-creator fallback: extrapolated or heuristic
	Missing      // placeholder; not physically usable
};

constexpr const char* to_string(ParamConfidence c) noexcept {
	switch (c) {
		case ParamConfidence::Published:  return "published";
		case ParamConfidence::Derived:    return "derived";
		case ParamConfidence::Estimated:  return "estimated";
		case ParamConfidence::Missing:    return "missing";
	}
	return "unknown";
}

struct UFFEntry {
	// Key fields
	std::string element;
	std::string atom_type;
	int         coordination_number = 0;
	std::string geometry_tag;

	// UFF force-field parameters (Rappé et al. 1992 notation)
	double r1     = 0.0;   // bond radius (Å)
	double theta0 = 0.0;   // equilibrium valence angle (degrees)
	double x1     = 0.0;   // vdW distance parameter (Å)
	double D1     = 0.0;   // vdW well depth (kcal/mol)
	double zeta   = 0.0;   // vdW scale / GMP exponential decay
	double Z1     = 0.0;   // effective charge parameter
	double Vi     = 0.0;   // torsion barrier (kcal/mol)
	double Uj     = 0.0;   // torsion constant
	double Xi     = 0.0;   // electronegativity (Pauling scale)
	double Hard   = 0.0;   // chemical hardness
	double Radius = 0.0;   // covalent/atomic radius

	// Provenance
	ParamConfidence confidence = ParamConfidence::Missing;
	std::string     source_id;    // citation key, e.g., "rappe1992"
	std::string     source_note;  // free-text annotation
};

// ---------------------------------------------------------------------------
// UFFTable: thread-safe live in-memory parameter store
// ---------------------------------------------------------------------------

class UFFTable {
public:
	UFFTable() = default;

	// Insert a fully specified entry (overwrites if atom_type already present).
	void insert(UFFEntry entry);

	// Look up by UFF atom type label (primary key).
	// Returns nullopt if not found.
	std::optional<UFFEntry> lookup(const std::string& atom_type) const;

	// Look up by element symbol — returns first match, or nullopt.
	std::optional<UFFEntry> lookup_by_element(const std::string& element) const;

	// Returns true if the atom type is present.
	bool contains(const std::string& atom_type) const;

	// Returns the total number of entries.
	std::size_t size() const;

	// Returns a snapshot of all entries (for logging/export).
	std::vector<UFFEntry> all_entries() const;

	// Remove a single entry by atom type label.
	bool erase(const std::string& atom_type);

	// Clear all entries.
	void clear();

private:
	mutable std::mutex                                               mutex_;
	std::unordered_map<std::string, UFFEntry>                        entries_;  // keyed by atom_type
};

} // namespace vsepr::uff
