/**
 * VSEPR-Sim GUI - ImGui Integration Layer
 * Elevates abstract GUI system to real visual interface
 * Version: 2.3.1
 */

#pragma once

#include "gui/context_menu.hpp"
#include "gui/data_pipe.hpp"
#include "imgui.h"
#include <string>
#include <vector>
#include <memory>

namespace vsepr {
namespace gui {

// ImGui Context Menu Renderer
class ImGuiContextMenuRenderer {
public:
    static void render(const ContextMenu& menu);
    static void renderMenuItem(const MenuItem& item);
    
private:
    static void renderAction(const MenuItem& item);
    static void renderInfo(const MenuItem& item);
    static void renderToggle(const MenuItem& item);
    static void renderSubmenu(const MenuItem& item);
    static void renderSeparator();
};

// ImGui Data Display Widgets
class ImGuiDataWidgets {
public:
    // Molecule info panel
    static void moleculeInfoPanel(const std::string& title, 
                                  const std::string& formula,
                                  double energy,
                                  int atom_count,
                                  int bond_count);
    
    // Energy plot
    static void energyPlot(const std::string& title,
                          const std::vector<double>& energies,
                          const std::vector<double>& times);
    
    // Status bar
    static void statusBar(const std::string& status,
                         bool is_error = false);
    
    // Properties table
    static void propertiesTable(const std::vector<std::pair<std::string, std::string>>& props);
    
    // 3D viewer placeholder
    static void viewer3D(const std::string& title,
                        int atom_count,
                        int bond_count);
};

// ImGui Theme Manager
class ImGuiThemeManager {
public:
    enum class Theme {
        LIGHT,
        DARK,
        VSEPR_BLUE
    };
    
    static void apply(Theme theme);
    static void applyLight();
    static void applyDark();
    static void applyVSEPRBlue();
    
private:
    static void setColors(const ImVec4& bg, const ImVec4& fg, 
                         const ImVec4& accent, const ImVec4& warn);
};

// Main ImGui Application Window
class ImGuiVSEPRWindow {
public:
    ImGuiVSEPRWindow();
    
    void render();
    
    // Connect to data pipes
    void connectPipes(std::shared_ptr<DataPipe<std::string>> status_pipe,
                     std::shared_ptr<DataPipe<double>> energy_pipe);
    
    // Event handlers
    void onMoleculeRightClick(const std::string& id, 
                             const std::string& formula,
                             double energy,
                             int atoms, int bonds);
    
    void onAtomRightClick(int index, const std::string& element,
                         double x, double y, double z, double charge);
    
private:
    void renderMenuBar();
    void renderMainView();
    void renderInfoPanel();
    void renderStatusBar();
    void renderContextMenus();
    
    // State
    std::string current_status_;
    std::vector<double> energy_history_;
    std::vector<double> time_history_;
    
    // Context menus
    bool show_molecule_menu_ = false;
    ContextMenu active_menu_;
    
    // Theme
    ImGuiThemeManager::Theme current_theme_;
};

} // namespace gui
} // namespace vsepr
