#pragma once
/*
periodic_db.hpp
---------------
Lightweight periodic table database for the VSEPR sim.

Design goals:
- deterministic
- serializable input (JSON file)
- hashable-ish element records (stable fields)
- NO chemistry heuristics: only facts + lookup

Dependency:
- nlohmann/json (single header): https://github.com/nlohmann/json
  Put it at: third_party/nlohmann/json.hpp (or adjust include)

Dataset:
- Use Bowserinator/Periodic-Table-JSON PeriodicTableJSON.json
  Vendor it into: data/PeriodicTableJSON.json
*/

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <algorithm>

#include "../../third_party/nlohmann/json.hpp"

namespace vsepr {

struct Element final {
    uint8_t  Z = 0;                 // atomic number
    std::string symbol;             // "C"
    std::string name;               // "Carbon"

    double atomic_mass = 0.0;       // u
    std::optional<double> en_pauling; // electronegativity (Pauling), if present

    uint8_t period = 0;             // 1..7
    uint8_t group  = 0;             // 1..18, 0 if unknown
    std::string block;              // "s","p","d","f" (or empty)

    // Electron shells as provided by dataset: e.g. [2, 8, 4] for Si
    std::vector<uint8_t> shells;

    // Convenience: valence electrons from shells (last shell count), if available.
    uint8_t valence_electrons() const {
        return shells.empty() ? 0 : shells.back();
    }

    // Convenience: total electrons for neutral atom (should equal Z if dataset is consistent)
    uint16_t total_shell_electrons() const {
        uint16_t s = 0;
        for (auto v : shells) s += v;
        return s;
    }
};

class PeriodicTable final {
public:
    // Load from Bowserinator PeriodicTableJSON.json
    // Throws std::runtime_error on parse / IO issues.
    static PeriodicTable load_from_json_file(const std::string& path) {
        std::ifstream f(path);
        std::vector<std::string> fallbacks;
        if (!f) {
            fallbacks = {
                "data/elements.physics.json",
                "../data/elements.physics.json",
                "data/PeriodicTableJSON.json"
            };
            for (const auto& alt : fallbacks) {
                f.open(alt);
                if (f) break;
            }
        }
        if (!f) throw std::runtime_error("PeriodicTable: cannot open JSON: " + path);

        nlohmann::json j;
        try {
            f >> j;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("PeriodicTable: JSON parse error: ") + e.what());
        }

        if (!j.contains("elements") || !j["elements"].is_array())
            throw std::runtime_error("PeriodicTable: JSON missing 'elements' array");

        PeriodicTable pt;
        pt.elements_.reserve(j["elements"].size());

        for (const auto& e : j["elements"]) {
            Element el;

            // Required-ish fields
            el.Z      = static_cast<uint8_t>(e.value("number", e.value("Z", 0)));
            el.symbol = e.value("symbol", "");
            el.name   = e.value("name", "");

            el.atomic_mass = e.value("atomic_mass", e.value("atomic_weight", 0.0));

            // Optional fields
            if (e.contains("electronegativity_pauling") && !e["electronegativity_pauling"].is_null())
                el.en_pauling = e["electronegativity_pauling"].get<double>();
            else if (e.contains("en_pauling") && !e["en_pauling"].is_null())
                el.en_pauling = e["en_pauling"].get<double>();

            el.period = static_cast<uint8_t>(e.value("period", 0));
            el.group  = static_cast<uint8_t>(e.value("group", 0));
            el.block  = e.value("block", "");

            if (e.contains("shells") && e["shells"].is_array()) {
                for (const auto& sh : e["shells"]) {
                    int v = sh.get<int>();
                    if (v < 0) v = 0;
                    if (v > 255) v = 255;
                    el.shells.push_back(static_cast<uint8_t>(v));
                }
            }

            // Basic integrity checks (non-fatal but you can turn these into throws)
            if (el.Z == 0 || el.symbol.empty() || el.name.empty()) {
                // skip junk records
                continue;
            }

            pt.elements_.push_back(std::move(el));
        }

        // Sort by Z for stable lookup
        std::sort(pt.elements_.begin(), pt.elements_.end(),
                  [](const Element& a, const Element& b){ return a.Z < b.Z; });

        // Build indices
        pt.by_symbol_.reserve(pt.elements_.size());
        pt.by_Z_.assign(pt.elements_.back().Z + 1, -1);

        for (int i = 0; i < (int)pt.elements_.size(); ++i) {
            const auto& el = pt.elements_[i];
            pt.by_symbol_[normalize_symbol(el.symbol)] = i;
            if (el.Z < pt.by_Z_.size()) pt.by_Z_[el.Z] = i;
        }

        return pt;
    }

    // Lookup: returns nullptr if missing
    const Element* by_Z(uint32_t Z) const {
        if (Z >= by_Z_.size()) return nullptr;
        int idx = by_Z_[Z];
        return (idx < 0) ? nullptr : &elements_[idx];
    }

    const Element* by_symbol(std::string_view sym) const {
        auto key = normalize_symbol(sym);
        auto it = by_symbol_.find(key);
        if (it == by_symbol_.end()) return nullptr;
        return &elements_[it->second];
    }

    const std::vector<Element>& all() const { return elements_; }

    // ========================================================================
    // Compatibility shims for older code expecting separate physics/visual
    // ========================================================================
    
    // Old code expected ElementPhysics*, but Element contains all data now
    const Element* physics_by_Z(uint8_t Z) const {
        return by_Z(Z);
    }
    
    // Old code expected ElementVisual*, but Element contains all data now  
    const Element* visual_by_Z(uint8_t Z) const {
        return by_Z(Z);
    }
    
    // Old code expected ElementPhysics* by symbol
    const Element* physics_by_symbol(const std::string& symbol) const {
        return by_symbol(symbol);
    }
    
    // Old code used load_separated(), forward to load_from_json_file()
    // (Assuming both paths point to the same JSON file for now)
    static PeriodicTable load_separated(
        const std::string& physics_path,
        [[maybe_unused]] const std::string& visual_path
    ) {
        // Use the first path (both should be the same JSON file)
        return load_from_json_file(physics_path);
    }

private:
    static std::string normalize_symbol(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
            out.push_back(c);
        }
        if (out.empty()) return out;
        // "fe" -> "Fe"
        out[0] = (char)std::toupper((unsigned char)out[0]);
        for (size_t i = 1; i < out.size(); ++i)
            out[i] = (char)std::tolower((unsigned char)out[i]);
        return out;
    }

private:
    std::vector<Element> elements_;
    std::unordered_map<std::string, int> by_symbol_;
    std::vector<int> by_Z_;
};

} // namespace vsepr
