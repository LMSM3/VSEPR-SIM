#pragma once
// =============================================================================
// src/core/math/vec3.hpp — Canonical Vec3 header (Day #56)
// =============================================================================
// This is the single authoritative include path for vsepr::Vec3.
// All subsystems must include this header and alias, not re-define.
//
// Rule (Day #56):
//   Allowed:  using Vec3 = vsepr::Vec3;
//   Forbidden: struct Vec3 { ... };  (in any module other than this one)
//
// The implementation lives in the sibling file:
//   src/core/math_vec3.hpp
// That file is stable; this header is the forward-facing canonical path.
// Both are safe to include — pragma once prevents double-definition.
// =============================================================================

#include "core/math_vec3.hpp"
