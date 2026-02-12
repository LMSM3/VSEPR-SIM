/**
 * VSEPR-Sim Molecular Pokedex - GUI Integration
 * Interactive molecule browser with ImGui visualization
 * Version: 2.3.1
 */

#pragma once

#include "gui/imgui_integration.hpp"
#include "gui/context_menu.hpp"
#include "gui/data_pipe.hpp"
#include "molecular/unified_types.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace vsepr {
namespace pokedex {

// Use unified molecular types (eliminates duplication)
using MoleculeEntry = molecular::PokedexEntry;
using MolecularMetadata = molecular::MolecularMetadata;

// Pokedex categories
enum class PokedexCategory {
    ALL,
    BINARY_HYDRIDES,
    HYPERVALENT,
    INTERHALOGENS,
    OXOACIDS,
    ALKANES,
    ALKENES,
    AROMATICS,
    FAVORITES
};

// Pokedex database
class PokedexDatabase {
public:
    static PokedexDatabase& instance();
    
    // Database operations
    void loadFromFile(const std::string& filename);
    void saveToFile(const std::string& filename);
    
    // Query operations
    std::vector<MoleculeEntry> getAll() const;
    std::vector<MoleculeEntry> getByCategory(PokedexCategory category) const;
    std::vector<MoleculeEntry> getByPhase(int phase) const;
    std::vector<MoleculeEntry> search(const std::string& query) const;
    MoleculeEntry* findByFormula(const std::string& formula);
    
    // Statistics
    int getTotalCount() const;
    int getTestedCount() const;
    int getSuccessCount() const;
    double getSuccessRate() const;
    std::map<std::string, int> getCategoryStats() const;
    
    // Modification
    void addMolecule(const MoleculeEntry& entry);
    void updateMolecule(const std::string& formula, const MoleculeEntry& entry);
    void markTested(const std::string& formula, bool success, double energy);
    
private:
    PokedexDatabase();
    void loadDefaultDatabase();
    
    std::vector<MoleculeEntry> molecules_;
};

// ImGui Pokedex Browser
class ImGuiPokedexBrowser {
public:
    ImGuiPokedexBrowser();
    
    void render();
    
    // Connect to data pipes
    void connectPipes(std::shared_ptr<gui::DataPipe<MoleculeEntry>> molecule_pipe,
                     std::shared_ptr<gui::DataPipe<std::string>> status_pipe);
    
    // Event handlers
    void onMoleculeSelected(const MoleculeEntry& entry);
    void onTestRequested(const std::string& formula);
    void onTestAllRequested();
    
private:
    void renderSidebar();
    void renderMoleculeList();
    void renderDetailPanel();
    void renderStatsPanel();
    void renderSearchBar();
    void renderCategoryFilter();
    
    // State
    PokedexCategory current_category_;
    std::string search_query_;
    MoleculeEntry* selected_molecule_;
    
    // Statistics
    int total_molecules_;
    int tested_molecules_;
    int successful_tests_;
    double success_rate_;
    
    // Data pipes
    std::shared_ptr<gui::DataPipe<MoleculeEntry>> molecule_pipe_;
    std::shared_ptr<gui::DataPipe<std::string>> status_pipe_;
};

// Pokedex testing system
class PokedexTester {
public:
    PokedexTester();
    
    // Test operations
    void testMolecule(const std::string& formula);
    void testCategory(PokedexCategory category);
    void testPhase(int phase);
    void testAll();
    
    // Results
    struct TestResult {
        std::string formula;
        bool success;
        double energy;
        std::string error_message;
        double test_time;
    };
    
    std::vector<TestResult> getResults() const { return results_; }
    void clearResults() { results_.clear(); }
    
    // Progress callback
    using ProgressCallback = std::function<void(int current, int total, const std::string& formula)>;
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = callback; }
    
private:
    bool runVSEPRTest(const std::string& formula, double& energy);
    
    std::vector<TestResult> results_;
    ProgressCallback progress_callback_;
};

// Complete Pokedex Application
class PokedexApp {
public:
    PokedexApp();
    
    void render();
    
    // Data pipe connections
    void setupPipes();
    
private:
    void renderMenuBar();
    void renderBrowser();
    void renderViewer();
    void renderTestPanel();
    void renderReportPanel();
    
    // Components
    std::unique_ptr<ImGuiPokedexBrowser> browser_;
    std::unique_ptr<PokedexTester> tester_;
    std::unique_ptr<gui::ImGuiVSEPRWindow> viewer_;
    
    // Data pipes
    std::shared_ptr<gui::DataPipe<MoleculeEntry>> molecule_pipe_;
    std::shared_ptr<gui::DataPipe<std::string>> status_pipe_;
    std::shared_ptr<gui::DataPipe<double>> energy_pipe_;
    
    // State
    bool show_browser_ = true;
    bool show_viewer_ = true;
    bool show_test_panel_ = false;
    bool testing_in_progress_ = false;
};

} // namespace pokedex
} // namespace vsepr
