#pragma once

#include "xyz/xyz_vec3.hpp"   // XYZVec3, Int3, and conversion helpers

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace vsim {

// ── VsimRuntimeError ─────────────────────────────────────────────────────────
// Thrown by value accessors on type mismatch, and by interpreter builtins.

struct VsimRuntimeError : std::runtime_error {
	explicit VsimRuntimeError(const std::string& msg)
		: std::runtime_error(msg) {}
};

// ── Value variant ─────────────────────────────────────────────────────────────
// Ordered: bool before int64 before double before string before list before
// XYZVec3 before Int3.
// parse_value() probes in this order.

using Value = std::variant<
	bool,
	int64_t,
	double,
	std::string,
	std::vector<std::string>,
	XYZVec3,
	Int3
>;

// Convenience type-test accessors -------------------------------------------

inline bool value_is_bool  (const Value& v) { return std::holds_alternative<bool>(v); }
inline bool value_is_int   (const Value& v) { return std::holds_alternative<int64_t>(v); }
inline bool value_is_double(const Value& v) { return std::holds_alternative<double>(v); }
inline bool value_is_string(const Value& v) { return std::holds_alternative<std::string>(v); }
inline bool value_is_list  (const Value& v) { return std::holds_alternative<std::vector<std::string>>(v); }
inline bool value_is_xyz   (const Value& v) { return std::holds_alternative<XYZVec3>(v); }
inline bool value_is_int3  (const Value& v) { return std::holds_alternative<Int3>(v); }

// Raw getters (throw std::bad_variant_access on wrong type) ------------------

inline bool                           as_bool  (const Value& v) { return std::get<bool>(v); }
inline int64_t                        as_int   (const Value& v) { return std::get<int64_t>(v); }
inline double                         as_double(const Value& v) { return std::get<double>(v); }
inline const std::string&             as_string(const Value& v) { return std::get<std::string>(v); }
inline const std::vector<std::string>& as_list (const Value& v) { return std::get<std::vector<std::string>>(v); }

// Typed accessors with VsimRuntimeError on mismatch -------------------------

inline XYZVec3 as_xyz_vec3(const Value& v) {
	if (!value_is_xyz(v))
		throw VsimRuntimeError(
			"Type error: expected XYZVec3, got "
			+ std::string(value_is_double(v) ? "double" :
						  value_is_int(v)    ? "int"    :
						  value_is_bool(v)   ? "bool"   :
						  value_is_string(v) ? "string" :
						  value_is_int3(v)   ? "Int3"   : "list"));
	return std::get<XYZVec3>(v);
}

inline Int3 as_int3(const Value& v) {
	if (!value_is_int3(v))
		throw VsimRuntimeError(
			"Type error: expected Int3, got "
			+ std::string(value_is_double(v)  ? "double"   :
						  value_is_int(v)     ? "int"      :
						  value_is_bool(v)    ? "bool"     :
						  value_is_string(v)  ? "string"   :
						  value_is_xyz(v)     ? "XYZVec3"  : "list"));
	return std::get<Int3>(v);
}

// Numeric coercion (int → double promotion) ----------------------------------
inline double numeric(const Value& v) {
	if (value_is_int(v))    return static_cast<double>(as_int(v));
	if (value_is_double(v)) return as_double(v);
	return 0.0;
}

// String coercion (any scalar → string representation) ----------------------
inline std::string to_string(const Value& v) {
	if (value_is_bool(v))   return as_bool(v) ? "true" : "false";
	if (value_is_int(v))    return std::to_string(as_int(v));
	if (value_is_double(v)) return std::to_string(as_double(v));
	if (value_is_string(v)) return as_string(v);
	if (value_is_xyz(v)) {
		const auto& r = std::get<XYZVec3>(v);
		return "XYZVec3(" + std::to_string(r.x) + "," +
							std::to_string(r.y) + "," +
							std::to_string(r.z) + ")";
	}
	if (value_is_int3(v)) {
		const auto& i = std::get<Int3>(v);
		return "Int3(" + std::to_string(i.x) + "," +
						 std::to_string(i.y) + "," +
						 std::to_string(i.z) + ")";
	}
	if (value_is_list(v)) {
		std::string out;
		for (const auto& s : as_list(v)) {
			if (!out.empty()) out += ',';
			out += s;
		}
		return out;
	}
	return {};
}

} // namespace vsim
