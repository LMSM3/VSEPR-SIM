// Card Catalog Viewer - ImGui-based run browser
// Displays simulation runs as interactive cards

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <fstream>

// JSON parsing (use nlohmann/json or similar)
#include <nlohmann/json.hpp>

namespace catalog {

using json = nlohmann::json;
namespace fs = std::filesystem;

// Run card data structure
struct RunCard {
    std::string run_id;
    std::string title;
    std::string formula;
    std::string domain;  // @molecule, @gas, @bulk, @crystal
    int size;  // Number of atoms
    std::string model;  // LJ, LJ+Coulomb
    float score;  // Priority score (0-100)
    std::string health;  // converged, bounded, exploded, invalid
    std::string timestamp;
    
    // Metrics
    double energy_per_atom;
    double max_force;
    int iterations;
    
    // Validation
    bool is_known;
    bool is_novel;
    float confidence;
    
    // Paths (relative to catalog root)
    std::string structure_xyz;
    std::string summary_json;
    
    // Tags
    std::vector<std::string> tags;

    // Score breakdown (transparent scoring)
    struct ScoreBreakdown {
        double wN = 0.0;     // Size preference
        double wQ = 0.0;     // Charge neutrality
        double wM = 0.0;     // Metal richness
        double wD = 0.0;     // Element diversity
        double wS = 0.0;     // Stability gate
        double wC = 0.0;     // Classification bonus
        double cost = 0.0;   // Computational cost
        double value = 0.0;  // Scientific value
        std::vector<std::string> classifications;  // Applied labels
    } score_breakdown;

    // Full path to run directory
    fs::path run_dir;
};

// Card grouping categories
enum class CardGroup {
    Small,       // ≤10 atoms
    Medium,      // 11-50 atoms
    Large,       // 51-200 atoms
    Heavy,       // 201-2000 atoms
    Exploded,    // Failed runs
    TopPicks     // High score
};

class CardCatalog {
public:
    CardCatalog(const fs::path& catalog_root)
        : catalog_root_(catalog_root) {}
    
    // Load all cards from catalog directory
    bool load() {
        cards_.clear();
        
        auto index_file = catalog_root_ / "cards_index.json";
        if (!fs::exists(index_file)) {
            return false;
        }
        
        std::ifstream ifs(index_file);
        json j;
        ifs >> j;
        
        for (const auto& card_json : j) {
            RunCard card;
            
            // Parse JSON
            card.run_id = card_json.value("run_id", "");
            card.title = card_json.value("title", "");
            card.formula = card_json.value("formula", "");
            card.domain = card_json.value("domain", "@molecule");
            card.size = card_json.value("size", 1);
            card.model = card_json.value("model", "LJ");
            card.score = card_json.value("score", 0.0f);
            card.health = card_json.value("health", "invalid");
            card.timestamp = card_json.value("timestamp", "");
            
            // Metrics
            if (card_json.contains("metrics")) {
                auto metrics = card_json["metrics"];
                card.energy_per_atom = metrics.value("energy_per_atom", 0.0);
                card.max_force = metrics.value("max_force", 0.0);
                card.iterations = metrics.value("iterations", 0);
            }
            
            // Validation
            if (card_json.contains("validation")) {
                auto val = card_json["validation"];
                card.is_known = val.value("is_known", false);
                card.is_novel = val.value("is_novel", false);
                card.confidence = val.value("confidence", 0.0f);
            }
            
            // Paths
            if (card_json.contains("paths")) {
                auto paths = card_json["paths"];
                card.structure_xyz = paths.value("structure_xyz", "structure.xyz");
                card.summary_json = paths.value("summary_json", "summary.json");
            }
            
            // Tags
            if (card_json.contains("tags")) {
                for (const auto& tag : card_json["tags"]) {
                    card.tags.push_back(tag.get<std::string>());
                }
            }

            // Score breakdown
            if (card_json.contains("score_breakdown")) {
                auto sb = card_json["score_breakdown"];
                card.score_breakdown.wN = sb.value("wN", 0.0);
                card.score_breakdown.wQ = sb.value("wQ", 0.0);
                card.score_breakdown.wM = sb.value("wM", 0.0);
                card.score_breakdown.wD = sb.value("wD", 0.0);
                card.score_breakdown.wS = sb.value("wS", 0.0);
                card.score_breakdown.wC = sb.value("wC", 0.0);
                card.score_breakdown.cost = sb.value("cost", 0.0);
                card.score_breakdown.value = sb.value("value", 0.0);

                // Classifications
                if (sb.contains("classifications")) {
                    for (const auto& cls : sb["classifications"]) {
                        card.score_breakdown.classifications.push_back(cls.get<std::string>());
                    }
                }
            }

            // Set run directory
            card.run_dir = catalog_root_ / card.run_id;
            
            cards_.push_back(card);
        }
        
        // Group cards
        group_cards();
        
        return true;
    }
    
    // Get all cards
    const std::vector<RunCard>& get_cards() const {
        return cards_;
    }
    
    // Get cards by group
    std::vector<RunCard> get_group(CardGroup group) const {
        std::vector<RunCard> result;
        
        for (const auto& card : cards_) {
            bool matches = false;
            
            switch (group) {
            case CardGroup::Small:
                matches = card.size <= 10;
                break;
            case CardGroup::Medium:
                matches = card.size >= 11 && card.size <= 50;
                break;
            case CardGroup::Large:
                matches = card.size >= 51 && card.size <= 200;
                break;
            case CardGroup::Heavy:
                matches = card.size >= 201;
                break;
            case CardGroup::Exploded:
                matches = card.health == "exploded" || card.health == "invalid";
                break;
            case CardGroup::TopPicks:
                matches = card.score >= 80.0f;
                break;
            }
            
            if (matches) {
                result.push_back(card);
            }
        }
        
        return result;
    }
    
    // Search cards by formula or tag
    std::vector<RunCard> search(const std::string& query) const {
        std::vector<RunCard> result;
        
        for (const auto& card : cards_) {
            // Search in formula
            if (card.formula.find(query) != std::string::npos) {
                result.push_back(card);
                continue;
            }
            
            // Search in title
            if (card.title.find(query) != std::string::npos) {
                result.push_back(card);
                continue;
            }
            
            // Search in tags
            for (const auto& tag : card.tags) {
                if (tag.find(query) != std::string::npos) {
                    result.push_back(card);
                    break;
                }
            }
        }
        
        return result;
    }
    
private:
    fs::path catalog_root_;
    std::vector<RunCard> cards_;
    
    void group_cards() {
        // Sort by score (descending)
        std::sort(cards_.begin(), cards_.end(), [](const auto& a, const auto& b) {
            return a.score > b.score;
        });
    }
};

} // namespace catalog
