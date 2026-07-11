#pragma once
/**
 * cpp_standard_corpus.hpp — C++23/26 Standards Corpus Metadata
 * =============================================================
 *
 * Compile-time metadata tying the C++23 and C++26 standards corpus
 * into the VSEPR-SIM version infrastructure. This header provides:
 *
 *   1. Standard edition identity and compiler baseline
 *   2. Feature adoption tracking (what we use, what we plan to use)
 *   3. WG21 paper cross-references for C++26-era material
 *   4. Compiler feature detection macros
 *
 * References:
 *   N4950  — C++23 Working Draft (WG21)
 *   N5014  — C++26 Working Draft (WG21)
 *   P2869R3, P2875R4, P3099R1 — selected C++26 papers
 *
 * GCC 15.2.0 (UCRT64) is the baseline compiler.
 */

#include <cstdint>

namespace vsepr {
namespace standards {

// ============================================================================
// Standard Edition Identity
// ============================================================================

enum class CppStandard : uint16_t {
    Cpp17 = 17,
    Cpp20 = 20,
    Cpp23 = 23,
    Cpp26 = 26,
};

struct StandardEdition {
    CppStandard     edition;
    const char*     wg21_draft;         // Primary working draft number
    const char*     draft_year;
    const char*     short_name;
};

inline constexpr StandardEdition ACTIVE_STANDARD = {
    CppStandard::Cpp23,
    "N4950",
    "2023",
    "C++23"
};

inline constexpr StandardEdition TRACKING_STANDARD = {
    CppStandard::Cpp26,
    "N5014",
    "2025",
    "C++26"
};

// ============================================================================
// Compiler Baseline
// ============================================================================

struct CompilerBaseline {
    const char* compiler;
    const char* version;
    const char* target;
    CppStandard supported_standard;
    bool        has_std_expected;
    bool        has_std_print;
    bool        has_std_flat_map;
    bool        has_std_generator;
    bool        has_std_unreachable;
    bool        has_monadic_optional;
    bool        has_deducing_this;
    bool        has_multidim_subscript;
    bool        has_consteval;
    bool        has_to_underlying;
};

// GCC 15.2.0 feature matrix — verified against cppreference compiler support
inline constexpr CompilerBaseline GCC_15_2 = {
    "GCC", "15.2.0", "x86_64-w64-mingw32 (UCRT64)",
    CppStandard::Cpp23,
    true,   // std::expected       — GCC 12+
    true,   // std::print          — GCC 14+
    true,   // std::flat_map       — GCC 15+
    true,   // std::generator      — GCC 14+ (partial)
    true,   // std::unreachable    — GCC 12+
    true,   // monadic optional    — GCC 12+
    true,   // deducing this       — GCC 14+
    true,   // multidim subscript  — GCC 12+
    true,   // consteval           — GCC 12+ (C++20, improved C++23)
    true,   // std::to_underlying  — GCC 12+
};

inline constexpr CompilerBaseline ACTIVE_COMPILER = GCC_15_2;

// ============================================================================
// Feature Detection Helpers
// ============================================================================

// C++23 feature test macros (from N4950 §15.11)
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202211L
    #define VSEPR_HAS_STD_EXPECTED 1
#else
    #define VSEPR_HAS_STD_EXPECTED 0
#endif

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
    #define VSEPR_HAS_STD_PRINT 1
#else
    #define VSEPR_HAS_STD_PRINT 0
#endif

#if defined(__cpp_lib_flat_map) && __cpp_lib_flat_map >= 202207L
    #define VSEPR_HAS_STD_FLAT_MAP 1
#else
    #define VSEPR_HAS_STD_FLAT_MAP 0
#endif

#if defined(__cpp_lib_optional) && __cpp_lib_optional >= 202110L
    #define VSEPR_HAS_MONADIC_OPTIONAL 1
#else
    #define VSEPR_HAS_MONADIC_OPTIONAL 0
#endif

#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
    #define VSEPR_HAS_STD_UNREACHABLE 1
#else
    #define VSEPR_HAS_STD_UNREACHABLE 0
#endif

#if defined(__cpp_lib_to_underlying) && __cpp_lib_to_underlying >= 202102L
    #define VSEPR_HAS_STD_TO_UNDERLYING 1
#else
    #define VSEPR_HAS_STD_TO_UNDERLYING 0
#endif

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L
    #define VSEPR_HAS_DEDUCING_THIS 1
#else
    #define VSEPR_HAS_DEDUCING_THIS 0
#endif

#if defined(__cpp_multidimensional_subscript) && __cpp_multidimensional_subscript >= 202211L
    #define VSEPR_HAS_MULTIDIM_SUBSCRIPT 1
#else
    #define VSEPR_HAS_MULTIDIM_SUBSCRIPT 0
#endif

// ============================================================================
// WG21 Paper Cross-References
// ============================================================================

struct WG21Paper {
    const char* number;         // e.g. "P2869R3"
    const char* title;
    CppStandard target_standard;
    const char* impact;         // How it affects VSEPR-SIM
};

inline constexpr WG21Paper TRACKED_PAPERS[] = {
    {"P2869R3",
     "Remove Deprecated Atomic Access APIs from C++26",
     CppStandard::Cpp26,
     "Audit: migrate shared_ptr atomic free functions to std::atomic<shared_ptr<T>>"},

    {"P2875R4",
     "Undeprecate polymorphic_allocator::destroy for C++26",
     CppStandard::Cpp26,
     "pmr allocator usage confirmed stable — destroy() no longer deprecated"},

    {"P3099R1",
     "Contracts for C++: User-defined diagnostic messages",
     CppStandard::Cpp26,
     "Future: contract annotations on kernel invariants and property search preconditions"},
};

inline constexpr size_t TRACKED_PAPER_COUNT =
    sizeof(TRACKED_PAPERS) / sizeof(WG21Paper);

// ============================================================================
// Feature Adoption Status
// ============================================================================

enum class AdoptionStatus : uint8_t {
    Active,         // Currently used in codebase
    Planned,        // Will adopt when ready
    Tracking,       // Monitoring for future use
    NotApplicable,  // Not relevant to this project
};

struct FeatureAdoption {
    const char*     feature_name;
    CppStandard     standard;
    AdoptionStatus  status;
    const char*     usage_site;     // Where in VSEPR-SIM this applies
};

inline constexpr FeatureAdoption FEATURE_PLAN[] = {
    // C++23 — Active
    {"std::expected<T,E>",        CppStandard::Cpp23, AdoptionStatus::Active,
     "nuclear_core_runner: fallible material lookups, property resolution"},
    {"std::optional monadic ops", CppStandard::Cpp23, AdoptionStatus::Active,
     "nuclear_core_runner: chain-style core lookup and property extraction"},
    {"std::to_underlying",        CppStandard::Cpp23, AdoptionStatus::Active,
     "scale_bridge: enum-to-int conversions for scale/domain arithmetic"},
    {"std::unreachable",          CppStandard::Cpp23, AdoptionStatus::Active,
     "scale_bridge: exhaustive switch statements, domain name lookup"},
    {"consteval",                 CppStandard::Cpp23, AdoptionStatus::Active,
     "cpp_standard_corpus: compile-time corpus validation"},

    // C++23 — Planned
    {"std::flat_map",             CppStandard::Cpp23, AdoptionStatus::Planned,
     "report_engine: hot-path element property lookups"},
    {"std::print/println",        CppStandard::Cpp23, AdoptionStatus::Planned,
     "all CLI output: replace iostream formatting"},
    {"deducing this",             CppStandard::Cpp23, AdoptionStatus::Planned,
     "PropertySearchEngine: CRTP-free mixin patterns"},
    {"std::generator",            CppStandard::Cpp23, AdoptionStatus::Planned,
     "report_engine: lazy case generation pipeline"},

    // C++26 — Tracking
    {"Contracts",                 CppStandard::Cpp26, AdoptionStatus::Tracking,
     "kernel APIs: preconditions on Z range, scale transitions, hash inputs"},
    {"std::execution",            CppStandard::Cpp26, AdoptionStatus::Tracking,
     "experiment_runner: parallel experiment pipelines"},
    {"Reflection",                CppStandard::Cpp26, AdoptionStatus::Tracking,
     "report serialization: automatic struct-to-CSV/JSON"},
    {"std::inplace_vector",       CppStandard::Cpp26, AdoptionStatus::Tracking,
     "hot-path containers with bounded capacity"},
    {"std::simd",                 CppStandard::Cpp26, AdoptionStatus::Tracking,
     "vectorized property computation in scale transitions"},
};

inline constexpr size_t FEATURE_PLAN_COUNT =
    sizeof(FEATURE_PLAN) / sizeof(FeatureAdoption);

// ============================================================================
// Compile-Time Corpus Validation
// ============================================================================

consteval bool validate_corpus() {
    // Verify we have both standard editions tracked
    bool has_23 = (ACTIVE_STANDARD.edition == CppStandard::Cpp23);
    bool has_26 = (TRACKING_STANDARD.edition == CppStandard::Cpp26);

    // Verify paper count
    bool papers_ok = (TRACKED_PAPER_COUNT >= 3);

    // Verify feature plan has both active and tracking entries
    bool has_active = false;
    bool has_tracking = false;
    for (size_t i = 0; i < FEATURE_PLAN_COUNT; ++i) {
        if (FEATURE_PLAN[i].status == AdoptionStatus::Active)   has_active = true;
        if (FEATURE_PLAN[i].status == AdoptionStatus::Tracking) has_tracking = true;
    }

    return has_23 && has_26 && papers_ok && has_active && has_tracking;
}

static_assert(validate_corpus(),
    "C++23/26 standards corpus validation failed — "
    "check StandardEdition entries and FeatureAdoption plan");

} // namespace standards
} // namespace vsepr
