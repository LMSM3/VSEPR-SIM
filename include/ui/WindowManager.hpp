// WindowManager.hpp - Formal contract for workspace layout engine
// NO toy window manager - this is a microscope-grade tiling system

#pragma once
#include <array>
#include <vector>
#include <optional>
#include <cstdint>

namespace vsepr::ui {

// ════════════════════════════════════════════════════════════════════════════
// Core types
// ════════════════════════════════════════════════════════════════════════════

struct Rect {
    float x, y, w, h;
    
    bool contains(float px, float py) const {
        return px >= x && px < x+w && py >= y && py < y+h;
    }
};

enum class WindowMode {
    Free,       // Draggable, constrained by workspace
    Snapped,    // Anchored to corner slot
    Fullscreen  // Fills workspace_rect
};

enum class Corner {
    None = -1,
    TopLeft = 0,
    TopRight = 1,
    BottomLeft = 2,
    BottomRight = 3
};

struct WindowState {
    uint32_t id;
    WindowMode mode;
    Corner corner;          // Only valid if mode == Snapped
    Rect rect;              // Current position/size
    int z_order;
    
    // Previous state (for fullscreen restore)
    WindowMode prev_mode;
    Rect prev_rect;
    Corner prev_corner;
    
    bool visible;
    bool focused;
};

// ════════════════════════════════════════════════════════════════════════════
// ViewModel: 8 presets + per-preset tuning
// ════════════════════════════════════════════════════════════════════════════

struct ViewModel {
    // Layout ratios
    float workspace_ratio;           // 0.60 - 0.70
    float snap_padding_px;           // Corner snap zones
    float min_frac_w, min_frac_h;    // Window size constraints
    float max_frac_w, max_frac_h;
    
    // UI density
    float font_scale;                // 0.8 - 1.2
    float ui_density;                // Padding multiplier
    float corner_snap_threshold_px;  // Snap detection zone
    
    // Behavior
    int default_grid_mode;           // 1, 2, or 4 panes
    bool fullscreen_workspace_only;  // true = workspace, false = whole app
    
    // Tuning deltas (applied iteratively, ±10% per iteration)
    struct Deltas {
        float workspace_ratio_delta;
        float padding_delta;
        float min_size_delta;
        float font_delta;
    } deltas;
    
    void apply_deltas(int iterations) {
        for (int i = 0; i < iterations; ++i) {
            workspace_ratio *= (1.0f + deltas.workspace_ratio_delta * 0.10f);
            snap_padding_px *= (1.0f + deltas.padding_delta * 0.10f);
            min_frac_w *= (1.0f + deltas.min_size_delta * 0.10f);
            min_frac_h *= (1.0f + deltas.min_size_delta * 0.10f);
            font_scale *= (1.0f + deltas.font_delta * 0.10f);
            
            // Clamp to sanity
            workspace_ratio = std::clamp(workspace_ratio, 0.55f, 0.75f);
            snap_padding_px = std::clamp(snap_padding_px, 4.0f, 32.0f);
            min_frac_w = std::clamp(min_frac_w, 0.20f, 0.50f);
            min_frac_h = std::clamp(min_frac_h, 0.20f, 0.50f);
            font_scale = std::clamp(font_scale, 0.70f, 1.50f);
        }
    }
};

// 8 presets (VM0..VM7)
constexpr std::array<ViewModel, 8> DEFAULT_VIEWMODELS = {{
    // VM0: Default microscope
    {0.65f, 8.0f, 0.25f, 0.25f, 1.00f, 1.00f, 1.00f, 1.00f, 32.0f, 4, true, {0,0,0,0}},
    
    // VM1: Wide workspace (70%)
    {0.70f, 8.0f, 0.25f, 0.25f, 1.00f, 1.00f, 1.05f, 1.00f, 32.0f, 4, true, {0,0,0,0}},
    
    // VM2: Compact (60%)
    {0.60f, 8.0f, 0.30f, 0.30f, 1.00f, 1.00f, 0.95f, 1.10f, 32.0f, 2, true, {0,0,0,0}},
    
    // VM3: Dense UI (small fonts)
    {0.65f, 6.0f, 0.25f, 0.25f, 1.00f, 1.00f, 0.85f, 1.20f, 24.0f, 4, true, {0,0,0,0}},
    
    // VM4: Spacious UI (large fonts)
    {0.65f, 12.0f, 0.25f, 0.25f, 1.00f, 1.00f, 1.15f, 0.90f, 40.0f, 4, true, {0,0,0,0}},
    
    // VM5: Single pane focus
    {0.70f, 8.0f, 0.40f, 0.40f, 1.00f, 1.00f, 1.00f, 1.00f, 32.0f, 1, false, {0,0,0,0}},
    
    // VM6: Quad split default
    {0.65f, 8.0f, 0.20f, 0.20f, 1.00f, 1.00f, 1.00f, 1.00f, 32.0f, 4, true, {0,0,0,0}},
    
    // VM7: Ultra-wide (for 21:9 monitors)
    {0.75f, 8.0f, 0.25f, 0.25f, 1.00f, 1.00f, 1.00f, 1.00f, 32.0f, 4, true, {0,0,0,0}},
}};

// ════════════════════════════════════════════════════════════════════════════
// Layout Engine (the actual window manager)
// ════════════════════════════════════════════════════════════════════════════

class WorkspaceLayoutEngine {
public:
    explicit WorkspaceLayoutEngine(int window_w, int window_h)
        : window_w_(window_w), window_h_(window_h), current_vm_idx_(0) {
        vm_ = DEFAULT_VIEWMODELS[0];
        recompute_workspace();
    }
    
    // Core API
    void set_window_size(int w, int h) {
        window_w_ = w;
        window_h_ = h;
        recompute_workspace();
    }
    
    void set_viewmodel(int idx, int tune_iterations = 0) {
        if (idx < 0 || idx >= 8) return;
        current_vm_idx_ = idx;
        vm_ = DEFAULT_VIEWMODELS[idx];
        vm_.apply_deltas(tune_iterations);
        recompute_workspace();
    }
    
    Rect workspace_rect() const { return workspace_rect_; }
    Rect instrument_rect() const { return instrument_rect_; }
    
    // Window management
    uint32_t add_window(WindowMode mode = WindowMode::Free, Corner corner = Corner::None);
    void remove_window(uint32_t id);
    void toggle_fullscreen(uint32_t id);
    void snap_to_corner(uint32_t id, Corner corner);
    void start_drag(uint32_t id);
    void drag_to(uint32_t id, float x, float y);
    void resize_to(uint32_t id, float w, float h);
    
    // Query
    std::optional<uint32_t> window_at(float x, float y) const;
    WindowState* get_window(uint32_t id);
    const std::vector<WindowState>& windows() const { return windows_; }
    
    // Snap detection
    std::optional<Corner> detect_snap_corner(float x, float y) const;
    Rect corner_rect(Corner c) const;
    
private:
    void recompute_workspace();
    Rect compute_snapped_rect(Corner c) const;
    Rect constrain_rect(Rect r) const;
    
    int window_w_, window_h_;
    Rect workspace_rect_;
    Rect instrument_rect_;
    
    ViewModel vm_;
    int current_vm_idx_;
    std::vector<WindowState> windows_;
    uint32_t next_id_ = 1;
};

// ════════════════════════════════════════════════════════════════════════════
// Implementation (inline for header-only)
// ════════════════════════════════════════════════════════════════════════════

inline void WorkspaceLayoutEngine::recompute_workspace() {
    float ws_w = window_w_ * vm_.workspace_ratio;
    workspace_rect_ = {0, 0, ws_w, (float)window_h_};
    instrument_rect_ = {ws_w, 0, window_w_ - ws_w, (float)window_h_};
}

inline Rect WorkspaceLayoutEngine::corner_rect(Corner c) const {
    float pad = vm_.snap_padding_px;
    float w2 = (workspace_rect_.w - 3*pad) / 2;
    float h2 = (workspace_rect_.h - 3*pad) / 2;
    
    switch (c) {
    case Corner::TopLeft:     return {pad, pad, w2, h2};
    case Corner::TopRight:    return {pad + w2 + pad, pad, w2, h2};
    case Corner::BottomLeft:  return {pad, pad + h2 + pad, w2, h2};
    case Corner::BottomRight: return {pad + w2 + pad, pad + h2 + pad, w2, h2};
    default: return {0,0,0,0};
    }
}

inline std::optional<Corner> WorkspaceLayoutEngine::detect_snap_corner(float x, float y) const {
    float thresh = vm_.corner_snap_threshold_px;
    
    if (x < thresh && y < thresh) return Corner::TopLeft;
    if (x > workspace_rect_.w - thresh && y < thresh) return Corner::TopRight;
    if (x < thresh && y > workspace_rect_.h - thresh) return Corner::BottomLeft;
    if (x > workspace_rect_.w - thresh && y > workspace_rect_.h - thresh) return Corner::BottomRight;
    
    return std::nullopt;
}

inline Rect WorkspaceLayoutEngine::constrain_rect(Rect r) const {
    float min_w = workspace_rect_.w * vm_.min_frac_w;
    float min_h = workspace_rect_.h * vm_.min_frac_h;
    float max_w = workspace_rect_.w * vm_.max_frac_w;
    float max_h = workspace_rect_.h * vm_.max_frac_h;
    
    r.w = std::clamp(r.w, min_w, max_w);
    r.h = std::clamp(r.h, min_h, max_h);
    
    r.x = std::clamp(r.x, 0.0f, workspace_rect_.w - r.w);
    r.y = std::clamp(r.y, 0.0f, workspace_rect_.h - r.h);
    
    return r;
}

inline uint32_t WorkspaceLayoutEngine::add_window(WindowMode mode, Corner corner) {
    WindowState ws;
    ws.id = next_id_++;
    ws.mode = mode;
    ws.corner = corner;
    ws.visible = true;
    ws.focused = true;
    ws.z_order = (int)windows_.size();
    
    if (mode == WindowMode::Snapped && corner != Corner::None) {
        ws.rect = corner_rect(corner);
    } else if (mode == WindowMode::Fullscreen) {
        ws.rect = workspace_rect_;
    } else {
        // Default free window: centered, 50% size
        ws.rect = {
            workspace_rect_.w * 0.25f,
            workspace_rect_.h * 0.25f,
            workspace_rect_.w * 0.50f,
            workspace_rect_.h * 0.50f
        };
    }
    
    ws.prev_mode = mode;
    ws.prev_rect = ws.rect;
    ws.prev_corner = corner;
    
    windows_.push_back(ws);
    return ws.id;
}

inline void WorkspaceLayoutEngine::toggle_fullscreen(uint32_t id) {
    auto* w = get_window(id);
    if (!w) return;
    
    if (w->mode == WindowMode::Fullscreen) {
        // Restore
        w->mode = w->prev_mode;
        w->rect = w->prev_rect;
        w->corner = w->prev_corner;
    } else {
        // Save and fullscreen
        w->prev_mode = w->mode;
        w->prev_rect = w->rect;
        w->prev_corner = w->corner;
        
        w->mode = WindowMode::Fullscreen;
        w->rect = workspace_rect_;
        w->corner = Corner::None;
    }
}

inline void WorkspaceLayoutEngine::snap_to_corner(uint32_t id, Corner corner) {
    auto* w = get_window(id);
    if (!w || corner == Corner::None) return;
    
    w->mode = WindowMode::Snapped;
    w->corner = corner;
    w->rect = corner_rect(corner);
}

inline void WorkspaceLayoutEngine::drag_to(uint32_t id, float x, float y) {
    auto* w = get_window(id);
    if (!w) return;
    
    w->rect.x = x;
    w->rect.y = y;
    w->rect = constrain_rect(w->rect);
}

inline void WorkspaceLayoutEngine::resize_to(uint32_t id, float w, float h) {
    auto* ws = get_window(id);
    if (!ws) return;
    
    ws->rect.w = w;
    ws->rect.h = h;
    ws->rect = constrain_rect(ws->rect);
}

inline WindowState* WorkspaceLayoutEngine::get_window(uint32_t id) {
    for (auto& w : windows_) {
        if (w.id == id) return &w;
    }
    return nullptr;
}

inline std::optional<uint32_t> WorkspaceLayoutEngine::window_at(float x, float y) const {
    // Reverse order (top z-order first)
    for (auto it = windows_.rbegin(); it != windows_.rend(); ++it) {
        if (it->visible && it->rect.contains(x, y)) {
            return it->id;
        }
    }
    return std::nullopt;
}

} // namespace vsepr::ui
