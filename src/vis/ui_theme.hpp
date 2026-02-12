#pragma once

#include <imgui.h>

namespace vsepr {
namespace render {

/**
 * Windows 11 Light Theme for ImGui
 * 
 * Modern, clean UI with:
 * - Light background (white/light gray)
 * - Subtle shadows and borders
 * - Rounded corners
 * - Blue accent colors (Windows 11 style)
 * - High contrast for readability
 */
class Windows11Theme {
public:
    /**
     * Apply Windows 11 light theme to ImGui
     */
    static void apply();
    
    /**
     * Apply dark theme variant (for night mode)
     */
    static void apply_dark();
    
    /**
     * Reset to default ImGui theme
     */
    static void reset();
    
    /**
     * Get accent color (Windows 11 blue)
     */
    static ImVec4 get_accent_color();
    
    /**
     * Get success color (green)
     */
    static ImVec4 get_success_color();
    
    /**
     * Get warning color (orange)
     */
    static ImVec4 get_warning_color();
    
    /**
     * Get error color (red)
     */
    static ImVec4 get_error_color();
    
    /**
     * Utility: Draw tooltip with Windows 11 style
     * 
     * Call this inside ImGui::IsItemHovered() block
     */
    static void tooltip(const char* text);
    
    /**
     * Utility: Begin a styled window
     */
    static bool begin_styled_window(const char* name, bool* p_open = nullptr,
                                   ImGuiWindowFlags flags = 0);
    
    /**
     * Utility: Draw separator with subtle styling
     */
    static void separator();
    
    /**
     * Utility: Draw section header
     */
    static void section_header(const char* text);
};

} // namespace render
} // namespace vsepr
