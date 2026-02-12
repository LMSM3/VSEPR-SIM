/**
 * Continuous Molecule Generation Manager Implementation
 * vsepr-sim v2.3.1 - Phase 3
 * 
 * NO HARDCODED ELEMENTS - All examples come from database dynamically
 * REAL PHYSICS ONLY - Uses actual thermodynamic data and chemical rules
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * License: MIT
 */

#include "gui/continuous_generation_manager.hpp"
#include "io/xyz_format.hpp"
#include "core/element_data_integrated.hpp"
#include "imgui.h"
#include "pot/periodic_db.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace vsepr {
namespace gui {

// Resolve element symbol by atomic number for export/preview
static const char* element_symbol(uint8_t Z) {
    static PeriodicTable table = [] {
        try {
            return PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        } catch (...) {
            return PeriodicTable();
        }
    }();
    const auto* elem = table.by_Z(Z);
    return elem ? elem->symbol.c_str() : "?";
}

// ============================================================================
// ContinuousGenerationManager Implementation
// ============================================================================

ContinuousGenerationManager::ContinuousGenerationManager()
    : generator_(std::make_unique<dynamic::ContinuousRealMoleculeGenerator>()) {
}

ContinuousGenerationManager::~ContinuousGenerationManager() {
    stop();
}

void ContinuousGenerationManager::start(const ContinuousGenerationState& state) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_state_ = state;
    
    // Configure generator
    generator_->enable_gpu(state.use_gpu);
    if (!state.output_path.empty()) {
        fs::path out_path(state.output_path);
        if (!out_path.parent_path().empty()) {
            fs::create_directories(out_path.parent_path());
        }
        generator_->set_output_stream(out_path.string());
    } else {
        generator_->set_output_stream("");
    }
    
    // Start generation
    generator_->start(state.target_count, state.checkpoint_interval, state.use_gpu, state.category);
}

void ContinuousGenerationManager::stop() {
    if (generator_) {
        generator_->stop();
    }
}

void ContinuousGenerationManager::pause() {
    if (generator_) {
        generator_->pause();
    }
}

void ContinuousGenerationManager::resume() {
    if (generator_) {
        generator_->resume();
    }
}

bool ContinuousGenerationManager::is_running() const {
    return generator_ && generator_->is_running();
}

bool ContinuousGenerationManager::is_paused() const {
    return generator_ && generator_->is_paused();
}

dynamic::GenerationStatistics ContinuousGenerationManager::get_statistics() const {
    if (generator_) {
        return generator_->statistics();
    }
    return dynamic::GenerationStatistics();
}

std::vector<Molecule> ContinuousGenerationManager::get_recent_molecules(size_t count) const {
    if (generator_) {
        return generator_->recent_molecules(count);
    }
    return {};
}

Molecule ContinuousGenerationManager::get_molecule(size_t index) const {
    if (generator_) {
        return generator_->get_molecule(index);
    }
    return Molecule();
}

Molecule ContinuousGenerationManager::get_latest_molecule() const {
    auto recent = get_recent_molecules(1);
    if (!recent.empty()) {
        return recent.back();
    }
    return Molecule();
}

size_t ContinuousGenerationManager::get_buffer_size() const {
    return get_recent_molecules(50).size();
}

bool ContinuousGenerationManager::is_gpu_available() const {
    if (generator_) {
        return generator_->is_gpu_available();
    }
    return false;
}

void ContinuousGenerationManager::set_new_molecule_callback(SelectionCallback callback) {
    new_molecule_callback_ = callback;
    
    // Set up internal callback to forward to user callback
    if (generator_) {
        generator_->set_checkpoint_callback([this](const dynamic::GenerationStatistics& stats) {
            if (new_molecule_callback_ && current_state_.auto_load_latest) {
                auto latest = get_latest_molecule();
                new_molecule_callback_(latest);
            }
        });
    }
}

void ContinuousGenerationManager::set_checkpoint_callback(
    std::function<void(const dynamic::GenerationStatistics&)> callback) {
    if (generator_) {
        generator_->set_checkpoint_callback(callback);
    }
}

void ContinuousGenerationManager::export_buffer_xyz(const std::string& path) const {
    fs::path out_path(path);
    if (!out_path.parent_path().empty()) {
        fs::create_directories(out_path.parent_path());
    }
    auto molecules = get_recent_molecules(50);
    
    // Use proper XYZ format library - never encode by hand!
    std::ofstream file(out_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    
    for (size_t i = 0; i < molecules.size(); ++i) {
        const auto& mol = molecules[i];
        
        io::XYZMolecule xyz_mol;
        xyz_mol.comment = "Continuous generation buffer entry " + std::to_string(i);
        
        // Copy atoms
        for (size_t j = 0; j < mol.num_atoms(); ++j) {
            double x, y, z;
            mol.get_position(j, x, y, z);
            
            const char* symbol = element_symbol(mol.atoms[j].Z);
            xyz_mol.atoms.emplace_back(symbol, x, y, z);
        }
        
        // Write using proper library
        io::XYZWriter writer;
        writer.set_precision(6);
        writer.write_stream(file, xyz_mol);
    }
}

void ContinuousGenerationManager::export_statistics_csv(const std::string& path) const {
    fs::path out_path(path);
    if (!out_path.parent_path().empty()) {
        fs::create_directories(out_path.parent_path());
    }
    auto stats = get_statistics();
    
    std::ofstream file(out_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    
    // CSV header
    file << "Metric,Value\n";
    
    // Write statistics (no hardcoded formulas!)
    file << "Total Generated," << stats.total_generated << "\n";
    file << "Unique Formulas," << stats.unique_formulas << "\n";
    file << "Rate (mol/s)," << std::fixed << std::setprecision(2) 
         << stats.rate_mol_per_sec << "\n";
    file << "Avg Atoms/Molecule," << std::setprecision(1) 
         << stats.avg_atoms_per_molecule << "\n";
    file << "Elapsed Time (s)," << std::setprecision(3)
         << std::chrono::duration<double>(
                std::chrono::steady_clock::now() - stats.start_time).count() << "\n";
    
    // Category breakdown (dynamic, based on actual generation)
    file << "\nCategory,Count\n";
    for (const auto& [cat, count] : stats.category_counts) {
        file << get_category_name(cat) << "," << count << "\n";
    }
}

// ============================================================================
// Category Helpers (NO HARDCODED FORMULAS!)
// ============================================================================

const char* get_category_name(dynamic::MoleculeCategory category) {
    using Cat = dynamic::MoleculeCategory;
    
    switch (category) {
        case Cat::SmallInorganic: return "Small Inorganics";
        case Cat::Hydrocarbons: return "Hydrocarbons";
        case Cat::Alcohols: return "Alcohols";
        case Cat::OrganicAcids: return "Organic Acids";
        case Cat::Aromatics: return "Aromatics";
        case Cat::Biomolecules: return "Biomolecules";
        case Cat::Drugs: return "Common Drugs";
        case Cat::All: return "All Categories";
        default: return "Unknown";
    }
}

const char* get_category_description(dynamic::MoleculeCategory category) {
    using Cat = dynamic::MoleculeCategory;
    
    // NO HARDCODED FORMULAS! Just describe the category
    switch (category) {
        case Cat::SmallInorganic:
            return "Simple inorganic molecules (oxides, hydrides, etc.)";
        
        case Cat::Hydrocarbons:
            return "Carbon-hydrogen compounds (alkanes, alkenes, cyclic)";
        
        case Cat::Alcohols:
            return "Organic compounds with hydroxyl groups";
        
        case Cat::OrganicAcids:
            return "Carboxylic acids and derivatives";
        
        case Cat::Aromatics:
            return "Benzene derivatives and aromatic compounds";
        
        case Cat::Biomolecules:
            return "Amino acids, sugars, and biological building blocks";
        
        case Cat::Drugs:
            return "Pharmaceutical compounds and active ingredients";
        
        case Cat::All:
            return "Random selection from all categories";
        
        default:
            return "Unknown category";
    }
}

// ============================================================================
// ImGui Rendering Helpers
// ============================================================================

bool render_continuous_controls(ContinuousGenerationState& state, 
                                ContinuousGenerationManager& manager) {
    bool state_changed = false;
    
    // Category selection (NO HARDCODED EXAMPLES!)
    const char* categories[] = {
        "All Categories",
        "Small Inorganics",
        "Hydrocarbons",
        "Alcohols",
        "Organic Acids",
        "Aromatics",
        "Biomolecules",
        "Common Drugs"
    };
    
    int current_cat = static_cast<int>(state.category);
    if (ImGui::Combo("Category", &current_cat, categories, 8)) {
        state.category = static_cast<dynamic::MoleculeCategory>(current_cat);
        state_changed = true;
    }
    
    // Show category description (dynamic, no formulas!)
    ImGui::TextWrapped("%s", get_category_description(state.category));
    
    ImGui::Separator();
    
    // Generation parameters
    if (ImGui::InputInt("Target Count (0 = infinite)", &state.target_count)) {
        if (state.target_count < 0) state.target_count = 0;
        state_changed = true;
    }
    
    if (ImGui::InputInt("Checkpoint Every N", &state.checkpoint_interval)) {
        if (state.checkpoint_interval < 1) state.checkpoint_interval = 1;
        state_changed = true;
    }
    
    // GPU option (if available)
    if (manager.is_gpu_available()) {
        if (ImGui::Checkbox("Use GPU Acceleration", &state.use_gpu)) {
            state_changed = true;
        }
    }
    
    ImGui::Separator();
    
    // Control buttons
    if (!state.is_running) {
        if (ImGui::Button("Start Generation", ImVec2(150, 0))) {
            manager.start(state);
            state.is_running = true;
        }
    } else {
        if (!state.is_paused) {
            if (ImGui::Button("Pause", ImVec2(70, 0))) {
                manager.pause();
                state.is_paused = true;
            }
        } else {
            if (ImGui::Button("Resume", ImVec2(70, 0))) {
                manager.resume();
                state.is_paused = false;
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(70, 0))) {
            manager.stop();
            state.is_running = false;
            state.is_paused = false;
        }
    }
    
    ImGui::Separator();
    
    // Display options
    ImGui::Checkbox("Show Statistics", &state.show_statistics);
    ImGui::Checkbox("Show Gallery", &state.show_gallery);
    ImGui::Checkbox("Auto-load Latest", &state.auto_load_latest);
    
    if (state.show_gallery) {
        ImGui::SliderInt("Gallery Columns", &state.gallery_columns, 3, 10);
    }
    
    return state_changed;
}

void render_statistics_panel(const dynamic::GenerationStatistics& stats) {
    ImGui::BeginGroup();
    
    ImGui::Text("Generation Statistics");
    ImGui::Separator();
    
    // Total generated
    ImGui::Text("Total Generated: %zu", stats.total_generated);
    
    // Unique formulas (would need Molecule::formula() implemented)
    ImGui::Text("Unique Formulas: %zu", stats.unique_formulas);
    
    // Generation rate
    ImGui::Text("Rate: %.1f mol/sec", stats.rate_mol_per_sec);
    
    // Average atoms per molecule
    ImGui::Text("Avg Atoms: %.1f", stats.avg_atoms_per_molecule);
    
    // Elapsed time
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - stats.start_time).count();
    ImGui::Text("Elapsed: %.1f seconds", elapsed);
    
    // Progress bar (if target set)
    if (stats.total_generated > 0) {
        // Show rate as visual bar
        float rate_normalized = std::min(1.0f, static_cast<float>(stats.rate_mol_per_sec / 1000.0));
        ImGui::ProgressBar(rate_normalized, ImVec2(-1, 0), "");
        ImGui::SameLine(0, 5);
        ImGui::Text("Generation Speed");
    }
    
    ImGui::EndGroup();
}

int render_molecule_gallery(const std::vector<Molecule>& molecules,
                            ContinuousGenerationState& state,
                            int thumbnail_size) {
    int clicked_index = -1;
    
    ImGui::BeginChild("MoleculeGallery", ImVec2(0, 0), true);
    
    if (molecules.empty()) {
        ImGui::TextDisabled("No molecules generated yet...");
        ImGui::TextWrapped("Select a category and click Start Generation");
    } else {
        // Render thumbnails in grid
        for (size_t i = 0; i < molecules.size(); ++i) {
            // Start new row if needed
            if (i % state.gallery_columns != 0) {
                ImGui::SameLine();
            }
            
            // Thumbnail button/frame
            ImGui::PushID(static_cast<int>(i));
            
            bool is_selected = (state.selected_index == i);
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
            }
            
            // Thumbnail button (would render actual 3D preview here)
            char label[32];
            snprintf(label, sizeof(label), "Mol %zu\n%zu atoms", 
                    i, molecules[i].num_atoms());
            
            if (ImGui::Button(label, ImVec2(thumbnail_size, thumbnail_size))) {
                state.selected_index = i;
                clicked_index = static_cast<int>(i);
            }
            
            if (is_selected) {
                ImGui::PopStyleColor();
            }
            
            // Tooltip with more info (NO HARDCODED FORMULAS!)
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Molecule #%zu", i);
                ImGui::Text("Atoms: %zu", molecules[i].num_atoms());
                ImGui::Text("Bonds: %zu", molecules[i].bonds.size());
                // Would show formula if Molecule::formula() implemented
                ImGui::EndTooltip();
            }
            
            ImGui::PopID();
        }
    }
    
    ImGui::EndChild();
    
    return clicked_index;
}

} // namespace gui
} // namespace vsepr
