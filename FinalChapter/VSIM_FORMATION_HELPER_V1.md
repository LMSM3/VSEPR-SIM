# Formation Pathway Helper — V1 Implementation Guide
## `vsim_formation_pathway_helper`

**Version:** v5.1.13.5 — V1, table-driven, no reaction engine  
**Status:** Design + implementation spec  
**Part of:** Day 75 WO-75E  
**Output:** JSON pathway report consumed by vsim-precompile and report writer

---

## 1. What it does

Given a target molecule and a list of reactant species, the helper:

1. Checks stoichiometry: do the reactants sum to the target formula?
2. Looks up the target in a known-route table
3. Ranks available routes by score
4. Outputs a JSON pathway report

It does not simulate chemistry. It does not run DFT. It ranks known routes from a table. V1 scope is explicit and intentional.

---

## 2. CLI

```
vsim_formation_pathway_helper
    --target    <formula>           required
    --reactants <comma-separated>   required
    --preset    <formation_preset>  optional (default: "molecular")
    --out       <path.json>         optional (default: stdout)
    --verbose                       optional
```

### Examples

```bash
vsim_formation_pathway_helper --target CH4 --reactants C,H,H,H,H --out pathways_CH4.json

vsim_formation_pathway_helper --target NH3 --reactants N,H,H,H --preset molecular --out pathways_NH3.json

vsim_formation_pathway_helper --target CO2 --reactants C,O,O --verbose
```

### Exit codes

| Code | Meaning |
|---|---|
| 0 | Success, pathways_found = true or false, JSON written |
| 1 | Invalid arguments |
| 2 | Stoichiometry error (reactants cannot sum to target) |
| 3 | Output file write error |

Exit 0 even when `pathways_found = false` — that is a valid result, not an error.

---

## 3. Output JSON schema

```json
{
  "target": "CH4",
  "reactants": ["C", "H", "H", "H", "H"],
  "preset": "molecular",
  "stoichiometry_valid": true,
  "pathways_found": true,
  "recommended_route": {
    "route_id": "direct_relaxation",
    "description": "Direct seeded relaxation from point-identity carriers.",
    "score": 0.92,
    "initialization": "seeded_random_near_target",
    "intermediate_species": [],
    "notes": "No intermediates required. FIRE relaxation from random seed."
  },
  "candidate_routes": [
    {
      "route_id": "direct_relaxation",
      "description": "Direct seeded relaxation.",
      "score": 0.92,
      "intermediate_species": [],
      "notes": ""
    },
    {
      "route_id": "stepwise_CH_CH2_CH3_CH4",
      "description": "Stepwise: C + H → CH, CH + H → CH2, CH2 + H → CH3, CH3 + H → CH4.",
      "score": 0.71,
      "intermediate_species": ["CH", "CH2", "CH3"],
      "notes": "Longer path. Only preferred if direct relaxation fails to converge."
    }
  ],
  "warnings": [],
  "helper_version": "V1"
}
```

### Score convention

| Range | Meaning |
|---|---|
| 0.90 – 1.00 | Direct relaxation viable; stoichiometry matches; no intermediates required |
| 0.70 – 0.89 | Stepwise route plausible; intermediates expected; more steps required |
| 0.50 – 0.69 | Uncertain; stoichiometry matches but geometry or energy unclear |
| 0.00 – 0.49 | Route unlikely or stoichiometrically mismatched |

---

## 4. C++ implementation

### File layout

```
src/tools/formation_helper/
    main.cpp                    CLI entry point
    stoichiometry_checker.hpp   Hill formula parsing and summation
    route_table.hpp             Known-route table
    route_ranker.hpp            Score and rank routes
    pathway_writer.hpp          JSON output
```

### `stoichiometry_checker.hpp`

```cpp
#pragma once
#include <map>
#include <string>
#include <vector>

// Parse Hill formula into element count map
// "CH4" → {C:1, H:4}
// "NH3" → {N:1, H:3}
inline std::map<std::string, int> parse_hill(const std::string& formula) {
    std::map<std::string, int> counts;
    for (size_t i = 0; i < formula.size(); ) {
        if (!std::isupper(formula[i])) { ++i; continue; }
        std::string elem;
        elem += formula[i++];
        while (i < formula.size() && std::islower(formula[i]))
            elem += formula[i++];
        int n = 0;
        while (i < formula.size() && std::isdigit(formula[i]))
            n = n * 10 + (formula[i++] - '0');
        counts[elem] += (n == 0 ? 1 : n);
    }
    return counts;
}

// Sum reactant species into element count map
// ["C", "H", "H", "H", "H"] → {C:1, H:4}
inline std::map<std::string, int> sum_reactants(
    const std::vector<std::string>& reactants)
{
    std::map<std::string, int> counts;
    for (const auto& r : reactants) {
        auto parsed = parse_hill(r);
        for (auto& [el, n] : parsed)
            counts[el] += n;
    }
    return counts;
}

// Returns true if reactants sum to target formula
inline bool stoichiometry_matches(
    const std::string& target,
    const std::vector<std::string>& reactants)
{
    return parse_hill(target) == sum_reactants(reactants);
}
```

### `route_table.hpp`

```cpp
#pragma once
#include <string>
#include <vector>

struct RouteEntry {
    std::string target;
    std::string route_id;
    std::string description;
    double      score;
    std::vector<std::string> intermediate_species;
    std::string initialization;
    std::string notes;
};

// V1 known-route table
// Extend this table to add new molecules — no code changes elsewhere
inline const std::vector<RouteEntry>& get_route_table() {
    static const std::vector<RouteEntry> table = {

        // CH4 — methane
        {"CH4", "direct_relaxation",
         "Direct seeded relaxation from C and H carriers.", 0.92,
         {}, "seeded_random_near_target", ""},
        {"CH4", "stepwise_CH_CH2_CH3_CH4",
         "Stepwise: C+H→CH, +H→CH2, +H→CH3, +H→CH4.", 0.71,
         {"CH","CH2","CH3"}, "seeded_random", "Preferred if direct fails."},

        // NH3 — ammonia
        {"NH3", "direct_relaxation",
         "Direct seeded relaxation from N and H carriers.", 0.91,
         {}, "seeded_random_near_target", ""},
        {"NH3", "stepwise_NH_NH2_NH3",
         "Stepwise: N+H→NH, +H→NH2, +H→NH3.", 0.73,
         {"NH","NH2"}, "seeded_random", ""},

        // H2O — water
        {"H2O", "direct_relaxation",
         "Direct seeded relaxation from O and H carriers.", 0.93,
         {}, "seeded_random_near_target", ""},
        {"H2O", "stepwise_OH_H2O",
         "Stepwise: O+H→OH, +H→H2O.", 0.78,
         {"OH"}, "seeded_random", ""},

        // CO2 — carbon dioxide
        {"CO2", "direct_relaxation",
         "Direct seeded relaxation from C and O carriers.", 0.89,
         {}, "seeded_random_near_target", ""},
        {"CO2", "stepwise_CO_CO2",
         "Stepwise: C+O→CO, +O→CO2.", 0.74,
         {"CO"}, "seeded_random", "CO is a stable intermediate."},

        // C2H6 — ethane
        {"C2H6", "direct_relaxation",
         "Direct seeded relaxation from 2C and 6H carriers.", 0.85,
         {}, "seeded_random_near_target", "Requires good seed — larger molecule."},
        {"C2H6", "stepwise_CH3_couple",
         "Form two CH3 radicals, couple at C-C bond.", 0.76,
         {"CH3"}, "seeded_two_fragment", "Two-step: build CH3 × 2, then couple."},
        {"C2H6", "stepwise_C2H4_H2",
         "Build C2H4 (ethylene), add H2.", 0.65,
         {"C2H4"}, "seeded_random", "Requires ethylene intermediate."},

        // C6H6 — benzene
        {"C6H6", "direct_relaxation",
         "Direct seeded relaxation from 6C and 6H carriers.", 0.72,
         {}, "seeded_random_near_target", "Low score: ring closure non-trivial from random seed."},
        {"C6H6", "stepwise_acetylene_trimerize",
         "Form C2H2 × 3, cyclotrimerize.", 0.68,
         {"C2H2"}, "seeded_three_fragment", "Three acetylene units, ring closure step."},

        // N2 — dinitrogen
        {"N2", "direct_relaxation",
         "Direct seeded relaxation from 2N carriers.", 0.95,
         {}, "seeded_random_near_target", "Simplest diatomic."},

        // H2 — dihydrogen
        {"H2", "direct_relaxation",
         "Direct seeded relaxation from 2H carriers.", 0.97,
         {}, "seeded_random_near_target", ""},
    };
    return table;
}
```

### `route_ranker.hpp`

```cpp
#pragma once
#include "route_table.hpp"
#include "stoichiometry_checker.hpp"
#include <algorithm>

struct RankedRoute {
    RouteEntry entry;
    bool stoichiometry_valid;
};

inline std::vector<RankedRoute> rank_routes(
    const std::string& target,
    const std::vector<std::string>& reactants,
    int max_routes = 5)
{
    const auto& table = get_route_table();
    bool stoi_ok = stoichiometry_matches(target, reactants);

    std::vector<RankedRoute> matches;
    for (const auto& row : table) {
        if (row.target == target)
            matches.push_back({row, stoi_ok});
    }

    // Sort by score descending
    std::sort(matches.begin(), matches.end(),
        [](const RankedRoute& a, const RankedRoute& b) {
            return a.entry.score > b.entry.score;
        });

    if ((int)matches.size() > max_routes)
        matches.resize(max_routes);

    return matches;
}
```

### `pathway_writer.hpp`

```cpp
#pragma once
#include "route_ranker.hpp"
#include <fstream>
#include <sstream>
#include <string>

inline std::string to_json_array(const std::vector<std::string>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        s += "\"" + v[i] + "\"";
        if (i + 1 < v.size()) s += ", ";
    }
    return s + "]";
}

inline std::string write_pathway_json(
    const std::string& target,
    const std::vector<std::string>& reactants,
    const std::string& preset,
    const std::vector<RankedRoute>& routes)
{
    bool stoi_ok = !routes.empty() && routes[0].stoichiometry_valid;
    bool found   = !routes.empty();

    std::ostringstream j;
    j << "{\n";
    j << "  \"target\": \"" << target << "\",\n";
    j << "  \"reactants\": " << to_json_array(reactants) << ",\n";
    j << "  \"preset\": \"" << preset << "\",\n";
    j << "  \"stoichiometry_valid\": " << (stoi_ok ? "true" : "false") << ",\n";
    j << "  \"pathways_found\": " << (found ? "true" : "false") << ",\n";

    if (found) {
        const auto& r = routes[0].entry;
        j << "  \"recommended_route\": {\n";
        j << "    \"route_id\": \""      << r.route_id        << "\",\n";
        j << "    \"description\": \""   << r.description     << "\",\n";
        j << "    \"score\": "           << r.score           << ",\n";
        j << "    \"initialization\": \"" << r.initialization << "\",\n";
        j << "    \"intermediate_species\": " << to_json_array(r.intermediate_species) << ",\n";
        j << "    \"notes\": \"" << r.notes << "\"\n";
        j << "  },\n";
    } else {
        j << "  \"recommended_route\": null,\n";
    }

    j << "  \"candidate_routes\": [\n";
    for (size_t i = 0; i < routes.size(); ++i) {
        const auto& r = routes[i].entry;
        j << "    {\n";
        j << "      \"route_id\": \""    << r.route_id    << "\",\n";
        j << "      \"description\": \"" << r.description << "\",\n";
        j << "      \"score\": "         << r.score       << ",\n";
        j << "      \"intermediate_species\": " << to_json_array(r.intermediate_species) << ",\n";
        j << "      \"notes\": \"" << r.notes << "\"\n";
        j << "    }";
        if (i + 1 < routes.size()) j << ",";
        j << "\n";
    }
    j << "  ],\n";
    j << "  \"warnings\": [],\n";
    j << "  \"helper_version\": \"V1\"\n";
    j << "}\n";
    return j.str();
}
```

### `main.cpp`

```cpp
#include "stoichiometry_checker.hpp"
#include "route_ranker.hpp"
#include "pathway_writer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::string target, reactants_str, preset = "molecular", out_path;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--target"    && i+1 < argc) target        = argv[++i];
        else if (a == "--reactants" && i+1 < argc) reactants_str = argv[++i];
        else if (a == "--preset"    && i+1 < argc) preset        = argv[++i];
        else if (a == "--out"       && i+1 < argc) out_path      = argv[++i];
        else if (a == "--verbose") verbose = true;
    }

    if (target.empty() || reactants_str.empty()) {
        std::cerr << "Usage: vsim_formation_pathway_helper "
                     "--target <formula> --reactants <A,B,C> "
                     "[--preset <preset>] [--out <path>]\n";
        return 1;
    }

    // Parse reactants comma-separated
    std::vector<std::string> reactants;
    std::istringstream ss(reactants_str);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) reactants.push_back(tok);

    // Stoichiometry check
    if (!stoichiometry_matches(target, reactants)) {
        if (verbose)
            std::cerr << "[WARN] Stoichiometry mismatch: "
                      << reactants_str << " does not sum to " << target << "\n";
        // Still output JSON with stoichiometry_valid = false
    }

    // Rank routes
    auto routes = rank_routes(target, reactants, 5);

    if (verbose) {
        std::cerr << "Target: " << target << "\n";
        std::cerr << "Routes found: " << routes.size() << "\n";
        for (auto& r : routes)
            std::cerr << "  " << r.entry.route_id << " score=" << r.entry.score << "\n";
    }

    // Write JSON
    std::string json = write_pathway_json(target, reactants, preset, routes);

    if (out_path.empty()) {
        std::cout << json;
    } else {
        std::ofstream f(out_path);
        if (!f) {
            std::cerr << "Error: cannot write to " << out_path << "\n";
            return 3;
        }
        f << json;
    }

    return 0;
}
```

---

## 5. CMakeLists entry

```cmake
add_executable(vsim_formation_pathway_helper
    src/tools/formation_helper/main.cpp
)
target_include_directories(vsim_formation_pathway_helper PRIVATE
    src/tools/formation_helper
    include
)
install(TARGETS vsim_formation_pathway_helper DESTINATION bin)
```

No dependencies on vsepr_core or any simulation library. Self-contained.

---

## 6. Tests

```cpp
// tests/test_formation_helper.cpp
#include <gtest/gtest.h>
#include "stoichiometry_checker.hpp"
#include "route_ranker.hpp"

TEST(Stoichiometry, CH4Matches) {
    EXPECT_TRUE(stoichiometry_matches("CH4", {"C","H","H","H","H"}));
}
TEST(Stoichiometry, CH4DoesNotMatchNH3Reactants) {
    EXPECT_FALSE(stoichiometry_matches("CH4", {"N","H","H","H"}));
}
TEST(Stoichiometry, H2OMatches) {
    EXPECT_TRUE(stoichiometry_matches("H2O", {"O","H","H"}));
}

TEST(RouteRanker, CH4HasDirectRelaxation) {
    auto routes = rank_routes("CH4", {"C","H","H","H","H"});
    ASSERT_FALSE(routes.empty());
    EXPECT_EQ(routes[0].entry.route_id, "direct_relaxation");
    EXPECT_GT(routes[0].entry.score, 0.90);
}
TEST(RouteRanker, UnknownMoleculeReturnsEmpty) {
    auto routes = rank_routes("XeF6", {"Xe","F","F","F","F","F","F"});
    EXPECT_TRUE(routes.empty());
}
TEST(RouteRanker, BenzeneHasAtLeastTwoRoutes) {
    auto routes = rank_routes("C6H6", {"C","C","C","C","C","C","H","H","H","H","H","H"});
    EXPECT_GE((int)routes.size(), 2);
}
```

---

## 7. Adding a new molecule (V1 extension protocol)

Add one or more rows to `route_table.hpp`'s static table:

```cpp
// Example: adding H2S — hydrogen sulfide
{"H2S", "direct_relaxation",
 "Direct seeded relaxation from S and H carriers.", 0.90,
 {}, "seeded_random_near_target", ""},
{"H2S", "stepwise_SH_H2S",
 "Stepwise: S+H→SH, +H→H2S.", 0.75,
 {"SH"}, "seeded_random", ""},
```

No other files change. The CLI, the stoichiometry checker, and the JSON writer are all molecule-agnostic.

---

## 8. V2 scope (not V1)

- Multi-product reactions (A + B → C + D)
- Temperature-dependent route scores (Arrhenius weighting)
- Reaction energy lookup (stub to DFT table)
- Confidence intervals on scores
- Interactive route selection (stdin prompt)
