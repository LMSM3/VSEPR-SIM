/**
 * VSEPR-Sim Molecular Pokedex - GUI Implementation
 * Interactive molecule browser with testing capabilities
 */

#include "gui/pokedex_gui.hpp"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>

namespace vsepr {
namespace pokedex {

// ============================================================================
// PokedexDatabase Implementation
// ============================================================================

PokedexDatabase& PokedexDatabase::instance() {
    static PokedexDatabase instance;
    return instance;
}

PokedexDatabase::PokedexDatabase() {
    loadDefaultDatabase();
}

void PokedexDatabase::loadDefaultDatabase() {
    // Phase 1: Binary Hydrides
    molecules_.push_back({"001", "H2", "Hydrogen", "Binary Hydrides", 1});
    molecules_.push_back({"002", "H2O", "Water", "Binary Hydrides", 1});
    molecules_.push_back({"003", "NH3", "Ammonia", "Binary Hydrides", 1});
    molecules_.push_back({"004", "CH4", "Methane", "Binary Hydrides", 1});
    molecules_.push_back({"005", "H2S", "Hydrogen Sulfide", "Binary Hydrides", 1});
    molecules_.push_back({"006", "HCl", "Hydrogen Chloride", "Binary Hydrides", 1});
    molecules_.push_back({"007", "HF", "Hydrogen Fluoride", "Binary Hydrides", 1});
    
    // Phase 1: Hypervalent
    molecules_.push_back({"008", "SF6", "Sulfur Hexafluoride", "Hypervalent", 1});
    molecules_.push_back({"009", "PF5", "Phosphorus Pentafluoride", "Hypervalent", 1});
    molecules_.push_back({"010", "PCl5", "Phosphorus Pentachloride", "Hypervalent", 1});
    molecules_.push_back({"011", "IF7", "Iodine Heptafluoride", "Hypervalent", 1});
    
    // Phase 1: Interhalogens
    molecules_.push_back({"012", "ClF3", "Chlorine Trifluoride", "Interhalogens", 1});
    molecules_.push_back({"013", "BrF5", "Bromine Pentafluoride", "Interhalogens", 1});
    molecules_.push_back({"014", "XeF2", "Xenon Difluoride", "Interhalogens", 1});
    molecules_.push_back({"015", "XeF4", "Xenon Tetrafluoride", "Interhalogens", 1});
    molecules_.push_back({"016", "XeF6", "Xenon Hexafluoride", "Interhalogens", 1});
    
    // Phase 1: Oxoacids
    molecules_.push_back({"017", "H2SO4", "Sulfuric Acid", "Oxoacids", 1});
    molecules_.push_back({"018", "H3PO4", "Phosphoric Acid", "Oxoacids", 1});
    molecules_.push_back({"019", "HNO3", "Nitric Acid", "Oxoacids", 1});
    
    // Phase 2: Alkanes
    molecules_.push_back({"020", "C2H6", "Ethane", "Alkanes", 2});
    molecules_.push_back({"021", "C3H8", "Propane", "Alkanes", 2});
    molecules_.push_back({"022", "C4H10", "n-Butane", "Alkanes", 2});
    molecules_.push_back({"023", "C5H12", "n-Pentane", "Alkanes", 2});
    molecules_.push_back({"024", "C6H14", "n-Hexane", "Alkanes", 2});
    molecules_.push_back({"025", "C8H18", "n-Octane", "Alkanes", 2});
    molecules_.push_back({"026", "C10H22", "n-Decane", "Alkanes", 2});
}

std::vector<MoleculeEntry> PokedexDatabase::getAll() const {
    return molecules_;
}

std::vector<MoleculeEntry> PokedexDatabase::getByCategory(PokedexCategory category) const {
    if (category == PokedexCategory::ALL) {
        return molecules_;
    }
    
    std::string cat_name;
    switch (category) {
        case PokedexCategory::BINARY_HYDRIDES: cat_name = "Binary Hydrides"; break;
        case PokedexCategory::HYPERVALENT: cat_name = "Hypervalent"; break;
        case PokedexCategory::INTERHALOGENS: cat_name = "Interhalogens"; break;
        case PokedexCategory::OXOACIDS: cat_name = "Oxoacids"; break;
        case PokedexCategory::ALKANES: cat_name = "Alkanes"; break;
        default: return {};
    }
    
    std::vector<MoleculeEntry> result;
    for (const auto& mol : molecules_) {
        if (mol.category == cat_name) {
            result.push_back(mol);
        }
    }
    return result;
}

std::vector<MoleculeEntry> PokedexDatabase::search(const std::string& query) const {
    std::vector<MoleculeEntry> result;
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    for (const auto& mol : molecules_) {
        std::string lower_name = mol.name;
        std::string lower_formula = mol.formula;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        std::transform(lower_formula.begin(), lower_formula.end(), lower_formula.begin(), ::tolower);
        
        if (lower_name.find(lower_query) != std::string::npos ||
            lower_formula.find(lower_query) != std::string::npos) {
            result.push_back(mol);
        }
    }
    return result;
}

int PokedexDatabase::getTotalCount() const {
    return molecules_.size();
}

int PokedexDatabase::getTestedCount() const {
    return std::count_if(molecules_.begin(), molecules_.end(),
                        [](const MoleculeEntry& m) { return m.tested; });
}

int PokedexDatabase::getSuccessCount() const {
    return std::count_if(molecules_.begin(), molecules_.end(),
                        [](const MoleculeEntry& m) { return m.tested && m.success; });
}

double PokedexDatabase::getSuccessRate() const {
    int tested = getTestedCount();
    if (tested == 0) return 0.0;
    return (100.0 * getSuccessCount()) / tested;
}

// ============================================================================
// ImGuiPokedexBrowser Implementation
// ============================================================================

ImGuiPokedexBrowser::ImGuiPokedexBrowser()
    : current_category_(PokedexCategory::ALL),
      selected_molecule_(nullptr),
      total_molecules_(0),
      tested_molecules_(0),
      successful_tests_(0),
      success_rate_(0.0) {
    
    // Update stats
    auto& db = PokedexDatabase::instance();
    total_molecules_ = db.getTotalCount();
    tested_molecules_ = db.getTestedCount();
    successful_tests_ = db.getSuccessCount();
    success_rate_ = db.getSuccessRate();
}

void ImGuiPokedexBrowser::render() {
    ImGui::Begin("Molecular Pokedex", nullptr, ImGuiWindowFlags_NoCollapse);
    
    renderSearchBar();
    ImGui::Separator();
    
    // Split view: List on left, details on right
    ImGui::BeginChild("List", ImVec2(300, 0), true);
    renderCategoryFilter();
    renderMoleculeList();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("Details", ImVec2(0, 0), true);
    if (selected_molecule_) {
        renderDetailPanel();
    } else {
        renderStatsPanel();
    }
    ImGui::EndChild();
    
    ImGui::End();
}

void ImGuiPokedexBrowser::connectPipes(
    std::shared_ptr<gui::DataPipe<MoleculeEntry>> molecule_pipe,
    std::shared_ptr<gui::DataPipe<std::string>> status_pipe) {
    
    molecule_pipe_ = molecule_pipe;
    status_pipe_ = status_pipe;
}

void ImGuiPokedexBrowser::renderSearchBar() {
    static char search_buf[128] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "🔍 Search molecules...", 
                                 search_buf, sizeof(search_buf))) {
        search_query_ = search_buf;
    }
}

void ImGuiPokedexBrowser::renderCategoryFilter() {
    ImGui::Text("Categories:");
    
    const char* categories[] = {
        "All", "Binary Hydrides", "Hypervalent", 
        "Interhalogens", "Oxoacids", "Alkanes"
    };
    
    static int current = 0;
    if (ImGui::ListBox("##categories", &current, categories, 6, 6)) {
        current_category_ = static_cast<PokedexCategory>(current);
    }
}

void ImGuiPokedexBrowser::renderMoleculeList() {
    auto& db = PokedexDatabase::instance();
    
    std::vector<MoleculeEntry> molecules;
    if (!search_query_.empty()) {
        molecules = db.search(search_query_);
    } else {
        molecules = db.getByCategory(current_category_);
    }
    
    ImGui::Separator();
    ImGui::Text("Molecules (%zu)", molecules.size());
    ImGui::Separator();
    
    for (auto& mol : molecules) {
        ImGui::PushID(mol.id.c_str());
        
        // Color based on test status
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (mol.tested) {
            color = mol.success ? 
                ImVec4(0.4f, 1.0f, 0.4f, 1.0f) :  // Green
                ImVec4(1.0f, 0.4f, 0.4f, 1.0f);   // Red
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        
        bool is_selected = (selected_molecule_ && selected_molecule_->id == mol.id);
        if (ImGui::Selectable(mol.formula.c_str(), is_selected)) {
            selected_molecule_ = db.findByFormula(mol.formula);
            if (molecule_pipe_) {
                molecule_pipe_->push(*selected_molecule_);
            }
        }
        
        ImGui::PopStyleColor();
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", mol.name.c_str());
            ImGui::Text("Phase %d: %s", mol.phase, mol.category.c_str());
            if (mol.tested) {
                ImGui::Text("Tested: %s", mol.success ? "✓ PASS" : "✗ FAIL");
            }
            ImGui::EndTooltip();
        }
        
        ImGui::PopID();
    }
}

void ImGuiPokedexBrowser::renderDetailPanel() {
    if (!selected_molecule_) return;
    
    const auto& mol = *selected_molecule_;
    
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Pokedex #%s", mol.id.c_str());
    ImGui::Separator();
    
    ImGui::Text("Formula:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", mol.formula.c_str());
    
    ImGui::Text("Name:");
    ImGui::SameLine(120);
    ImGui::Text("%s", mol.name.c_str());
    
    ImGui::Text("Category:");
    ImGui::SameLine(120);
    ImGui::Text("%s", mol.category.c_str());
    
    ImGui::Text("Phase:");
    ImGui::SameLine(120);
    ImGui::Text("%d", mol.phase);
    
    ImGui::Separator();
    
    if (mol.tested) {
        ImGui::Text("Test Status:");
        ImGui::SameLine(120);
        if (mol.success) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✓ PASS");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "✗ FAIL");
        }
        
        if (mol.success) {
            ImGui::Text("Energy:");
            ImGui::SameLine(120);
            ImGui::Text("%.2f kcal/mol", mol.energy);
            
            ImGui::Text("Atoms:");
            ImGui::SameLine(120);
            ImGui::Text("%d", mol.atom_count);
            
            ImGui::Text("Bonds:");
            ImGui::SameLine(120);
            ImGui::Text("%d", mol.bond_count);
            
            if (!mol.geometry.empty()) {
                ImGui::Text("Geometry:");
                ImGui::SameLine(120);
                ImGui::Text("%s", mol.geometry.c_str());
            }
        }
        
        ImGui::Text("Test Date:");
        ImGui::SameLine(120);
        ImGui::Text("%s", mol.test_date.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Not yet tested");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Test This Molecule", ImVec2(-1, 0))) {
        onTestRequested(mol.formula);
    }
    
    if (ImGui::Button("View in 3D", ImVec2(-1, 0))) {
        // Trigger 3D view
    }
}

void ImGuiPokedexBrowser::renderStatsPanel() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Pokedex Statistics");
    ImGui::Separator();
    
    ImGui::Text("Total Molecules:");
    ImGui::SameLine(180);
    ImGui::Text("%d", total_molecules_);
    
    ImGui::Text("Tested:");
    ImGui::SameLine(180);
    ImGui::Text("%d", tested_molecules_);
    
    ImGui::Text("Successful:");
    ImGui::SameLine(180);
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%d", successful_tests_);
    
    ImGui::Text("Success Rate:");
    ImGui::SameLine(180);
    ImGui::Text("%.1f%%", success_rate_);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Test All Molecules", ImVec2(-1, 0))) {
        onTestAllRequested();
    }
    
    ImGui::Spacing();
    ImGui::Text("Select a molecule to view details");
}

void ImGuiPokedexBrowser::onTestRequested(const std::string& formula) {
    if (status_pipe_) {
        status_pipe_->push("Testing " + formula + "...");
    }
}

void ImGuiPokedexBrowser::onTestAllRequested() {
    if (status_pipe_) {
        status_pipe_->push("Testing all molecules...");
    }
}

MoleculeEntry* PokedexDatabase::findByFormula(const std::string& formula) {
    for (auto& mol : molecules_) {
        if (mol.formula == formula) {
            return &mol;
        }
    }
    return nullptr;
}

} // namespace pokedex
} // namespace vsepr
