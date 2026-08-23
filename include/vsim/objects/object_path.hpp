/**
 * object_path.hpp  -  Dot-path object references for VSIM constructor objects
 *
 * WO-66N  |  Constructor Objects + Upper-Block References
 * v5.1.4  |  v5.0.0-main
 *
 * An ObjectPath is a dotted string reference that uniquely identifies a named
 * object declared in a [objects] section or as a constructor expression.
 *
 * Examples:
 *   system.surface.wall
 *   system.geometry.pipe
 *   system.source.inlet
 *   system.sink.outlet
 *   environment.ambient
 *
 * Resolution rules:
 *   1. Exact match against named objects in the ConstructorObjectRegistry.
 *   2. Prefix match: "system.surface" resolves to any surface object whose
 *      canonical path starts with that prefix (used for type-only references).
 *   3. Unresolved paths are stored as-is and reported as VSIM-W090 at validate().
 */

#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// ObjectPath
// ============================================================================

struct ObjectPath {
	std::string path;   // raw dotted path string, e.g. "system.surface.wall"

	ObjectPath() = default;
	explicit ObjectPath(std::string p) : path(std::move(p)) {}
	explicit ObjectPath(const char* p) : path(p ? p : "") {}

	bool empty()    const noexcept { return path.empty(); }
	bool is_set()   const noexcept { return !path.empty(); }

	// Return the top-level segment ("system", "environment", …)
	std::string root() const {
		auto pos = path.find('.');
		return (pos == std::string::npos) ? path : path.substr(0, pos);
	}

	// Return all path segments split by '.'
	std::vector<std::string> segments() const {
		std::vector<std::string> out;
		std::string seg;
		for (char c : path) {
			if (c == '.') { out.push_back(seg); seg.clear(); }
			else          { seg += c; }
		}
		if (!seg.empty()) out.push_back(seg);
		return out;
	}

	// True if this path starts with the given prefix (segment-aligned)
	bool has_prefix(const std::string& prefix) const {
		if (path == prefix) return true;
		if (path.size() > prefix.size() && path[prefix.size()] == '.')
			return path.substr(0, prefix.size()) == prefix;
		return false;
	}

	// Last segment, e.g. "system.surface.wall" → "wall"
	std::string leaf() const {
		auto pos = path.rfind('.');
		return (pos == std::string::npos) ? path : path.substr(pos + 1);
	}

	bool operator==(const ObjectPath& o) const noexcept { return path == o.path; }
	bool operator!=(const ObjectPath& o) const noexcept { return path != o.path; }
	bool operator< (const ObjectPath& o) const noexcept { return path <  o.path; }
};

// ============================================================================
// ObjectPathRef  -  a named binding: "name = some.object.path"
// ============================================================================

struct ObjectPathRef {
	std::string name;   // local binding name (e.g. "from", "target", "inlet")
	ObjectPath  path;   // resolved path

	bool is_set() const noexcept { return path.is_set(); }
};

} // namespace vsim
