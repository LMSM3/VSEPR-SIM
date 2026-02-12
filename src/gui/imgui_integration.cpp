/**
 * VSEPR-Sim GUI - ImGui Integration Implementation
 * Real visual rendering of context menus and data
 */

#include "gui/imgui_integration.hpp"
#include "imgui.h"
#include <algorithm>

namespace vsepr {
namespace gui {

// ============================================================================
// Context Menu Renderer
// ============================================================================

void ImGuiContextMenuRenderer::render(const ContextMenu& menu) {
    for (const auto& item : menu.items()) {
        renderMenuItem(item);
    }
}

void ImGuiContextMenuRenderer::renderMenuItem(const MenuItem& item) {
    switch (item.type) {
        case MenuItemType::ACTION:
            renderAction(item);
            break;
        case MenuItemType::INFO:
            renderInfo(item);
            break;
        case MenuItemType::TOGGLE:
            renderToggle(item);
            break;
        case MenuItemType::SUBMENU:
            renderSubmenu(item);
            break;
        case MenuItemType::SEPARATOR:
            renderSeparator();
            break;
    }
}

void ImGuiContextMenuRenderer::renderAction(const MenuItem& item) {
    std::string label = item.label;
    if (!item.shortcut.empty()) {
        label += "\t" + item.shortcut;
    }
    
    if (ImGui::MenuItem(label.c_str(), nullptr, false, item.enabled)) {
        if (item.action) {
            item.action();
        }
    }
    
    if (!item.tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", item.tooltip.c_str());
    }
}

void ImGuiContextMenuRenderer::renderInfo(const MenuItem& item) {
    ImGui::TextDisabled("%s", item.label.c_str());
}

void ImGuiContextMenuRenderer::renderToggle(const MenuItem& item) {
    bool checked = item.checked;
    if (ImGui::MenuItem(item.label.c_str(), nullptr, &checked, item.enabled)) {
        if (item.action) {
            item.action();
        }
    }
}

void ImGuiContextMenuRenderer::renderSubmenu(const MenuItem& item) {
    if (ImGui::BeginMenu(item.label.c_str(), item.enabled)) {
        for (const auto& sub_item : item.submenu) {
            renderMenuItem(sub_item);
        }
        ImGui::EndMenu();
    }
}

void ImGuiContextMenuRenderer::renderSeparator() {
    ImGui::Separator();
}

// ============================================================================
// Data Display Widgets
// ============================================================================

void ImGuiDataWidgets::moleculeInfoPanel(const std::string& title,
                                        const std::string& formula,
                                        double energy,
                                        int atom_count,
                                        int bond_count) {
    if (ImGui::Begin(title.c_str())) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Molecule Information");
        ImGui::Separator();
        
        ImGui::Text("Formula:");
        ImGui::SameLine(120);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", formula.c_str());
        
        ImGui::Text("Energy:");
        ImGui::SameLine(120);
        ImGui::Text("%.2f kcal/mol", energy);
        
        ImGui::Text("Atoms:");
        ImGui::SameLine(120);
        ImGui::Text("%d", atom_count);
        
        ImGui::Text("Bonds:");
        ImGui::SameLine(120);
        ImGui::Text("%d", bond_count);
        
        ImGui::Spacing();
        
        if (ImGui::Button("Optimize Geometry", ImVec2(-1, 0))) {
            // Action
        }
        
        if (ImGui::Button("Export XYZ", ImVec2(-1, 0))) {
            // Action
        }
    }
    ImGui::End();
}

void ImGuiDataWidgets::energyPlot(const std::string& title,
                                 const std::vector<double>& energies,
                                 const std::vector<double>& times) {
    if (ImGui::Begin(title.c_str())) {
        if (!energies.empty()) {
            float* values = new float[energies.size()];
            for (size_t i = 0; i < energies.size(); ++i) {
                values[i] = static_cast<float>(energies[i]);
            }
            
            ImGui::PlotLines("Energy (kcal/mol)", 
                           values, 
                           energies.size(),
                           0,
                           nullptr,
                           FLT_MAX, FLT_MAX,
                           ImVec2(0, 80));
            
            delete[] values;
            
            ImGui::Text("Time: %.2f ps", times.empty() ? 0.0 : times.back());
        } else {
            ImGui::TextDisabled("No data yet...");
        }
    }
    ImGui::End();
}

void ImGuiDataWidgets::statusBar(const std::string& status, bool is_error) {
    ImVec4 color = is_error ? 
        ImVec4(1.0f, 0.4f, 0.4f, 1.0f) :  // Red
        ImVec4(0.4f, 1.0f, 0.4f, 1.0f);   // Green
    
    ImGui::TextColored(color, "%s", status.c_str());
}

void ImGuiDataWidgets::propertiesTable(const std::vector<std::pair<std::string, std::string>>& props) {
    if (ImGui::BeginTable("Properties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        for (const auto& [key, value] : props) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", key.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", value.c_str());
        }
        
        ImGui::EndTable();
    }
}

void ImGuiDataWidgets::viewer3D(const std::string& title,
                               int atom_count,
                               int bond_count) {
    if (ImGui::Begin(title.c_str())) {
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        
        // Placeholder 3D viewer
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_size.x, 
                                 canvas_p0.y + canvas_size.y);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(100, 100, 100, 255));
        
        // Center text
        ImVec2 text_size = ImGui::CalcTextSize("3D Viewer");
        ImVec2 text_pos = ImVec2(
            canvas_p0.x + (canvas_size.x - text_size.x) * 0.5f,
            canvas_p0.y + (canvas_size.y - text_size.y) * 0.5f
        );
        draw_list->AddText(text_pos, IM_COL32(200, 200, 200, 255), "3D Viewer");
        
        // Info text
        ImVec2 info_pos = ImVec2(canvas_p0.x + 10, canvas_p0.y + 10);
        char info_buf[128];
        snprintf(info_buf, sizeof(info_buf), "Atoms: %d | Bonds: %d", 
                atom_count, bond_count);
        draw_list->AddText(info_pos, IM_COL32(150, 150, 150, 255), info_buf);
        
        // Right-click context menu
        if (ImGui::BeginPopupContextItem("viewer_context")) {
            if (ImGui::MenuItem("Reset View")) { }
            if (ImGui::MenuItem("Center Molecule")) { }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Image")) { }
            ImGui::EndPopup();
        }
        
        ImGui::Dummy(canvas_size);
    }
    ImGui::End();
}

// ============================================================================
// Theme Manager
// ============================================================================

void ImGuiThemeManager::apply(Theme theme) {
    switch (theme) {
        case Theme::LIGHT:
            applyLight();
            break;
        case Theme::DARK:
            applyDark();
            break;
        case Theme::VSEPR_BLUE:
            applyVSEPRBlue();
            break;
    }
}

void ImGuiThemeManager::applyLight() {
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

void ImGuiThemeManager::applyDark() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

void ImGuiThemeManager::applyVSEPRBlue() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // VSEPR custom blue theme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.2f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.2f, 0.4f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.3f, 0.5f, 1.0f);
    
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 6.0f;
}

// ============================================================================
// Main Application Window
// ============================================================================

ImGuiVSEPRWindow::ImGuiVSEPRWindow() 
    : current_status_("Ready"),
      current_theme_(ImGuiThemeManager::Theme::VSEPR_BLUE) {
    
    ImGuiThemeManager::apply(current_theme_);
}

void ImGuiVSEPRWindow::render() {
    renderMenuBar();
    renderMainView();
    renderInfoPanel();
    renderStatusBar();
    renderContextMenus();
}

void ImGuiVSEPRWindow::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open XYZ", "Ctrl+O")) { }
            if (ImGui::MenuItem("Save", "Ctrl+S")) { }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) { }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Light Theme")) {
                current_theme_ = ImGuiThemeManager::Theme::LIGHT;
                ImGuiThemeManager::apply(current_theme_);
            }
            if (ImGui::MenuItem("Dark Theme")) {
                current_theme_ = ImGuiThemeManager::Theme::DARK;
                ImGuiThemeManager::apply(current_theme_);
            }
            if (ImGui::MenuItem("VSEPR Blue")) {
                current_theme_ = ImGuiThemeManager::Theme::VSEPR_BLUE;
                ImGuiThemeManager::apply(current_theme_);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Compute")) {
            if (ImGui::MenuItem("Optimize Geometry", "Ctrl+R")) { }
            if (ImGui::MenuItem("Calculate Energy")) { }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) { }
            if (ImGui::MenuItem("About")) { }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void ImGuiVSEPRWindow::renderMainView() {
    ImGuiDataWidgets::viewer3D("Molecular Viewer", 3, 2);
}

void ImGuiVSEPRWindow::renderInfoPanel() {
    ImGuiDataWidgets::moleculeInfoPanel("Properties", "H₂O", -57.8, 3, 2);
    ImGuiDataWidgets::energyPlot("Energy History", energy_history_, time_history_);
}

void ImGuiVSEPRWindow::renderStatusBar() {
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 25));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 25));
    
    if (ImGui::Begin("StatusBar", nullptr, 
                    ImGuiWindowFlags_NoTitleBar | 
                    ImGuiWindowFlags_NoResize | 
                    ImGuiWindowFlags_NoMove)) {
        ImGuiDataWidgets::statusBar(current_status_);
    }
    ImGui::End();
}

void ImGuiVSEPRWindow::renderContextMenus() {
    if (show_molecule_menu_) {
        if (ImGui::BeginPopupContextVoid("molecule_context")) {
            ImGuiContextMenuRenderer::render(active_menu_);
            ImGui::EndPopup();
        } else {
            show_molecule_menu_ = false;
        }
    }
}

void ImGuiVSEPRWindow::connectPipes(
    std::shared_ptr<DataPipe<std::string>> status_pipe,
    std::shared_ptr<DataPipe<double>> energy_pipe) {
    
    if (status_pipe) {
        status_pipe->subscribe([this](const std::string& status) {
            current_status_ = status;
        });
    }
    
    if (energy_pipe) {
        energy_pipe->subscribe([this](double energy) {
            energy_history_.push_back(energy);
            time_history_.push_back(time_history_.size() * 0.1);
            
            // Keep last 100 points
            if (energy_history_.size() > 100) {
                energy_history_.erase(energy_history_.begin());
                time_history_.erase(time_history_.begin());
            }
        });
    }
}

void ImGuiVSEPRWindow::onMoleculeRightClick(
    const std::string& id,
    const std::string& formula,
    double energy,
    int atoms, int bonds) {
    
    active_menu_ = MoleculeContextMenu::build(id, formula, energy, atoms, bonds);
    show_molecule_menu_ = true;
    ImGui::OpenPopup("molecule_context");
}

void ImGuiVSEPRWindow::onAtomRightClick(
    int index, const std::string& element,
    double x, double y, double z, double charge) {
    
    active_menu_ = AtomContextMenu::build(index, element, x, y, z, charge);
    show_molecule_menu_ = true;
    ImGui::OpenPopup("molecule_context");
}

} // namespace gui
} // namespace vsepr
