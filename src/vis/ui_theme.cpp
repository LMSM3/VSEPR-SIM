#include "ui_theme.hpp"

namespace vsepr {
namespace render {

void Windows11Theme::apply() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // ========================================================================
    // Windows 11 Light Theme Colors
    // ========================================================================
    
    // Base colors
    colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);  // Black text
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);  // Gray
    colors[ImGuiCol_WindowBg]               = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);  // Very light gray
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);  // Transparent
    colors[ImGuiCol_PopupBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);  // White popup
    colors[ImGuiCol_Border]                 = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);  // Light gray border
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);  // Subtle shadow
    
    // Frame (inputs, buttons)
    colors[ImGuiCol_FrameBg]                = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);  // Light gray
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);  // Slightly darker
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);  // Active state
    
    // Title bar
    colors[ImGuiCol_TitleBg]                = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);  // Light gray
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);  // Windows 11 blue
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    
    // Menu bar
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    
    // Checkmark
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);  // Blue
    
    // Slider
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.48f, 0.80f, 0.78f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    
    // Button
    colors[ImGuiCol_Button]                 = ImVec4(0.00f, 0.48f, 0.80f, 0.40f);  // Blue tint
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.48f, 0.80f, 0.60f);  // Brighter blue
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.48f, 0.80f, 0.80f);  // Solid blue
    
    // Header (collapsing header, tree node)
    colors[ImGuiCol_Header]                 = ImVec4(0.00f, 0.48f, 0.80f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.48f, 0.80f, 0.50f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.48f, 0.80f, 0.70f);
    
    // Separator
    colors[ImGuiCol_Separator]              = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.00f, 0.48f, 0.80f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.48f, 0.80f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.48f, 0.80f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.48f, 0.80f, 0.95f);
    
    // Tab
    colors[ImGuiCol_Tab]                    = ImVec4(0.90f, 0.90f, 0.90f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.00f, 0.48f, 0.80f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.92f, 0.92f, 0.92f, 0.98f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    
    // Plot
    colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    
    // Table
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.07f);
    
    // Text selection
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 0.48f, 0.80f, 0.35f);
    
    // Drag drop
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.00f, 0.48f, 0.80f, 0.95f);
    
    // Nav highlight
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.00f, 0.48f, 0.80f, 0.80f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    
    // Modal window dim
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    
    // ========================================================================
    // Windows 11 Style Parameters
    // ========================================================================
    
    style.WindowRounding    = 8.0f;      // Rounded corners
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;      // No border on frames
    style.TabBorderSize     = 0.0f;
    
    style.WindowPadding     = ImVec2(12.0f, 12.0f);
    style.FramePadding      = ImVec2(8.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.IndentSpacing     = 20.0f;
    
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 8.0f;
    
    style.WindowTitleAlign  = ImVec2(0.50f, 0.50f);  // Centered title
    style.ButtonTextAlign   = ImVec2(0.50f, 0.50f);
    
    // Anti-aliasing
    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
}

void Windows11Theme::apply_dark() {
    // Dark variant (for night mode)
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Dark background with blue accents
    colors[ImGuiCol_Text]           = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_WindowBg]       = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    // ... (rest similar to light theme but inverted)
    
    // Keep same style parameters as light theme
    apply();  // Apply light theme first
    
    // Then override colors for dark mode
    colors[ImGuiCol_Text]           = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_WindowBg]       = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
}

void Windows11Theme::reset() {
    ImGui::StyleColorsDark();  // ImGui default dark
}

ImVec4 Windows11Theme::get_accent_color() {
    return ImVec4(0.00f, 0.48f, 0.80f, 1.00f);  // Windows 11 blue
}

ImVec4 Windows11Theme::get_success_color() {
    return ImVec4(0.10f, 0.70f, 0.30f, 1.00f);  // Green
}

ImVec4 Windows11Theme::get_warning_color() {
    return ImVec4(1.00f, 0.60f, 0.00f, 1.00f);  // Orange
}

ImVec4 Windows11Theme::get_error_color() {
    return ImVec4(0.90f, 0.10f, 0.20f, 1.00f);  // Red
}

void Windows11Theme::tooltip(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool Windows11Theme::begin_styled_window(const char* name, bool* p_open, 
                                        ImGuiWindowFlags flags) {
    // Add shadow effect via window background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.98f, 0.98f, 0.98f, 0.95f));
    bool result = ImGui::Begin(name, p_open, flags);
    ImGui::PopStyleColor();
    return result;
}

void Windows11Theme::separator() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void Windows11Theme::section_header(const char* text) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.48f, 0.80f, 1.00f));  // Blue
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

} // namespace render
} // namespace vsepr
