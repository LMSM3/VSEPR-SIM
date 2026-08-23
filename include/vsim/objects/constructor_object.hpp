/**
 * constructor_object.hpp  -  Constructor-style named object base and registry
 *
 * WO-66N  |  Constructor Objects + Batching + Upper-Block References
 * v5.1.4  |  v5.0.0-main
 *
 * A ConstructorObject is a named, typed object declared in a [objects] section
 * using a functional constructor expression:
 *
 *   [objects]
 *   system.surface.wall = WallSurface(...)
 *   system.geometry.pipe = PipeGeometry(radius = 0.05, length = 2.0)
 *   dem.pipe_packing = DEMBridge(from = system.surface.wall, ...)
 *
 * Objects are stored in a ConstructorObjectRegistry keyed by their ObjectPath.
 * Batching: multiple instances may be declared with a shared prefix and a
 * per-instance suffix, e.g. system.surface.wall[0], system.surface.wall[1].
 *
 * Upper-block references: a constructor argument may reference any ObjectPath
 * already registered in the same [objects] block, forming a DAG.  Circular
 * references are rejected at validate().
 */

#pragma once

#include "object_path.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// ConstructorObjectKind  -  type tag for every known constructor
// ============================================================================

enum class ConstructorObjectKind : int {
	Unknown         = 0,
	// 66Q — non-molecular geometry / flow objects
	PipeGeometry    = 10,
	BoxGeometry     = 11,
	SphereGeometry  = 12,
	GenericGeometry = 13,
	WallSurface     = 20,
	InletSource     = 30,
	OutletSink      = 31,
	AmbientEnv      = 40,
	// 66N — bridge objects (field schema frozen for WO-67N/67O)
	DEMBridge       = 50,
	FEABridge       = 51,
	// 66P — crystal/PBC constructor
	CrystalModule   = 60,
};

// String → kind lookup (case-insensitive)
inline ConstructorObjectKind constructor_kind_from_string(const std::string& s) {
	if (s == "PipeGeometry"    || s == "pipegeometry")    return ConstructorObjectKind::PipeGeometry;
	if (s == "BoxGeometry"     || s == "boxgeometry")     return ConstructorObjectKind::BoxGeometry;
	if (s == "SphereGeometry"  || s == "spheregeometry")  return ConstructorObjectKind::SphereGeometry;
	if (s == "GenericGeometry" || s == "genericgeometry") return ConstructorObjectKind::GenericGeometry;
	if (s == "WallSurface"     || s == "wallsurface")     return ConstructorObjectKind::WallSurface;
	if (s == "InletSource"     || s == "inletsource")     return ConstructorObjectKind::InletSource;
	if (s == "OutletSink"      || s == "outletsink")      return ConstructorObjectKind::OutletSink;
	if (s == "AmbientEnv"      || s == "ambientenv")      return ConstructorObjectKind::AmbientEnv;
	if (s == "DEMBridge"       || s == "dembridge")       return ConstructorObjectKind::DEMBridge;
	if (s == "FEABridge"       || s == "feabridge")       return ConstructorObjectKind::FEABridge;
	if (s == "CrystalModule"   || s == "crystalmodule")   return ConstructorObjectKind::CrystalModule;
	return ConstructorObjectKind::Unknown;
}

inline const char* constructor_kind_name(ConstructorObjectKind k) {
	switch (k) {
		case ConstructorObjectKind::PipeGeometry:    return "PipeGeometry";
		case ConstructorObjectKind::BoxGeometry:     return "BoxGeometry";
		case ConstructorObjectKind::SphereGeometry:  return "SphereGeometry";
		case ConstructorObjectKind::GenericGeometry: return "GenericGeometry";
		case ConstructorObjectKind::WallSurface:     return "WallSurface";
		case ConstructorObjectKind::InletSource:     return "InletSource";
		case ConstructorObjectKind::OutletSink:      return "OutletSink";
		case ConstructorObjectKind::AmbientEnv:      return "AmbientEnv";
		case ConstructorObjectKind::DEMBridge:       return "DEMBridge";
		case ConstructorObjectKind::FEABridge:       return "FEABridge";
		case ConstructorObjectKind::CrystalModule:   return "CrystalModule";
		default:                                     return "Unknown";
	}
}

// ============================================================================
// ConstructorObject  -  one declared object instance
// ============================================================================

struct ConstructorObject {
	ObjectPath              path;          // canonical path, e.g. "system.surface.wall"
	ConstructorObjectKind   kind          = ConstructorObjectKind::Unknown;
	std::string             constructor;  // raw constructor name as written in .vsim
	int                     batch_index   = -1;  // -1 = singleton; ≥0 = batch member

	// Raw key-value arguments from the constructor expression
	// (typed objects below populate these and then parse into typed fields)
	std::map<std::string, std::string> args;

	// Object references declared as arguments (upper-block references)
	std::vector<ObjectPathRef> refs;

	// Source location for diagnostics
	int source_line = 0;

	bool is_batch_member() const noexcept { return batch_index >= 0; }
};

// ============================================================================
// ConstructorObjectRegistry  -  flat map of all declared objects in a script
// ============================================================================

struct ConstructorObjectRegistry {
	// Ordered insertion list (preserves declaration order)
	std::vector<ConstructorObject>       objects;

	// Fast lookup by path string
	std::map<std::string, std::size_t>   index;  // path.path → objects index

	// Insert or overwrite (last declaration wins, matching .vsim semantics)
	void insert(ConstructorObject obj) {
		auto it = index.find(obj.path.path);
		if (it != index.end()) {
			objects[it->second] = std::move(obj);
		} else {
			index[obj.path.path] = objects.size();
			objects.push_back(std::move(obj));
		}
	}

	// Lookup — returns nullptr if not found
	const ConstructorObject* find(const ObjectPath& p) const {
		auto it = index.find(p.path);
		if (it == index.end()) return nullptr;
		return &objects[it->second];
	}

	ConstructorObject* find(const ObjectPath& p) {
		auto it = index.find(p.path);
		if (it == index.end()) return nullptr;
		return &objects[it->second];
	}

	bool has(const ObjectPath& p) const { return find(p) != nullptr; }

	bool empty() const noexcept { return objects.empty(); }
	std::size_t size() const noexcept { return objects.size(); }

	// Return all objects of a given kind
	std::vector<const ConstructorObject*> by_kind(ConstructorObjectKind k) const {
		std::vector<const ConstructorObject*> out;
		for (const auto& o : objects)
			if (o.kind == k) out.push_back(&o);
		return out;
	}

	// Return all objects whose path starts with a given prefix
	std::vector<const ConstructorObject*> by_prefix(const std::string& prefix) const {
		std::vector<const ConstructorObject*> out;
		for (const auto& o : objects)
			if (o.path.has_prefix(prefix)) out.push_back(&o);
		return out;
	}

	void clear() { objects.clear(); index.clear(); }
};

// ============================================================================
// BatchGroup  -  helper for batched object declarations
//
// A batch group is a set of ConstructorObjects sharing a base path and
// differing only by batch_index.  Declared in .vsim as:
//
//   system.surface.wall[0] = WallSurface(...)
//   system.surface.wall[1] = WallSurface(...)
//
// or via a batch block:
//
//   [objects.batch]
//   base = system.surface.wall
//   count = 3
//   constructor = WallSurface
//   pressure = [101325, 105000, 98000]
// ============================================================================

struct BatchGroup {
	std::string              base_path;   // e.g. "system.surface.wall"
	ConstructorObjectKind    kind        = ConstructorObjectKind::Unknown;
	int                      count       = 0;

	// Scalar defaults shared by all members
	std::map<std::string, std::string> defaults;

	// Per-member overrides: overrides[i][key] = value
	std::vector<std::map<std::string, std::string>> per_member;

	// Expand into individual ConstructorObjects for insertion into the registry
	std::vector<ConstructorObject> expand() const {
		std::vector<ConstructorObject> out;
		for (int i = 0; i < count; ++i) {
			ConstructorObject obj;
			obj.path        = ObjectPath(base_path + "[" + std::to_string(i) + "]");
			obj.kind        = kind;
			obj.constructor = constructor_kind_name(kind);
			obj.batch_index = i;
			obj.args        = defaults;
			if (i < static_cast<int>(per_member.size())) {
				for (const auto& [k, v] : per_member[i])
					obj.args[k] = v;
			}
			out.push_back(std::move(obj));
		}
		return out;
	}
};

} // namespace vsim
