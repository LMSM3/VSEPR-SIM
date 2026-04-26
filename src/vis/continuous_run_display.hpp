#pragma once
// continuous_run_display.hpp -- Passive batch display (console + optional GL)
// VSEPR-SIM | vis layer
//
// Console path: fully inline, zero GL deps. Works in any standalone build.
// GL path:      enabled by -DVSEPR_HAS_GL; Window constructed in .cpp.
//
// Usage (identical in both paths):
//   ContinuousRunDisplay display;
//   display.configure("NiTi batch", 5.0f);
//   display.open(960, 720);   // returns false -> console fallback, silent
//   std::thread work([&]{ display.bridge().start(); ...; display.bridge().finish(); });
//   display.run_event_loop(); // blocks until batch done or close() called
//   work.join();

#include "vis/batch_window_bridge.hpp"
#include "vis/viz_config.hpp"
#include <memory>
#include <atomic>
#include <string>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

namespace vsepr {
class Window;
namespace vis {

// Custom deleter -- body in continuous_run_display.cpp (GL builds).
// For non-GL standalone builds the window_ is always null; the deleter
// is never invoked, but must still be defined for the type to be complete.
struct WindowDeleter {
#ifdef VSEPR_HAS_GL
    void operator()(Window* p) const noexcept;  // defined in .cpp
#else
    void operator()(Window*) const noexcept {}  // never called
#endif
};

struct DisplayConfig {
    std::string run_label          = "Batch Run";
    float       display_fps        = 5.0f;
    int         win_width          = 960;
    int         win_height         = 720;
    bool        auto_close_on_done = true;
    bool        console_fallback   = true;
    VizConfig   viz;
};

// ---------------------------------------------------------------------------
// ConsoleProgressBar -- fully inline, no GL dependency
// ---------------------------------------------------------------------------
class ConsoleProgressBar {
public:
    explicit ConsoleProgressBar(int w = 40) : bar_width_(w) {}

    void print(const BatchStatus& s) const {
        float pct = s.total_frames > 0
            ? static_cast<float>(s.frame_index) / s.total_frames : 0.0f;
        int filled = static_cast<int>(pct * bar_width_);
        std::cout << "\r[";
        for (int i = 0; i < bar_width_; ++i)
            std::cout << (i < filled ? '#' : '-');
        std::cout << "] ";
        if (s.total_frames > 0)
            std::cout << std::setw(3) << static_cast<int>(pct * 100) << "% ";
        std::cout << s.run_label;
        if (!s.current_op.empty()) std::cout << " | " << s.current_op;
        std::cout << "  "
                  << static_cast<int>(s.elapsed_s) / 60 << "m"
                  << static_cast<int>(s.elapsed_s) % 60 << "s   ";
        std::cout.flush();
    }

    void done(const BatchStatus& s) const {
        print(s);
        std::cout << "\n[BATCH DONE] " << s.run_label
                  << "  frames="    << s.frame_index
                  << "  artifacts=" << s.artifacts_done
                  << "  elapsed="   << static_cast<int>(s.elapsed_s) << "s\n";
    }
private:
    int bar_width_;
};

// ---------------------------------------------------------------------------
// ContinuousRunDisplay
// ---------------------------------------------------------------------------
class ContinuousRunDisplay {
public:
    ContinuousRunDisplay()  = default;
    ~ContinuousRunDisplay() = default;  // WindowDeleter body in .cpp
    ContinuousRunDisplay(const ContinuousRunDisplay&)            = delete;
    ContinuousRunDisplay& operator=(const ContinuousRunDisplay&) = delete;

    void configure(const DisplayConfig& cfg) { cfg_ = cfg; }
    void configure(const std::string& label, float display_fps = 5.0f) {
        cfg_.run_label   = label;
        cfg_.display_fps = display_fps;
    }

    // Open GL window. Returns true on success.
    // When VSEPR_HAS_GL is not defined the inline stub returns false,
    // making run_event_loop() use the console path automatically.
#ifdef VSEPR_HAS_GL
    bool open(int width = 960, int height = 720);
#else
    bool open(int /*width*/ = 960, int /*height*/ = 720) { return false; }
#endif

    void close() { close_requested_.store(true); }

    BatchWindowBridge& bridge() { return bridge_; }

    // Blocks until batch done or close() called.
    void run_event_loop() {
        if (window_) { run_gl_loop(); return; }
        run_console_loop();
    }

private:
#ifdef VSEPR_HAS_GL
    void run_gl_loop();  // defined in continuous_run_display.cpp
#else
    void run_gl_loop() {}  // never reached; window_ is always null
#endif

    // Fully inline console fallback -- no GL, no window, no extra deps.
    void run_console_loop() {
        using clock = std::chrono::steady_clock;
        const double tick_s = 1.0 / std::max(cfg_.display_fps, 0.5f);
        while (!close_requested_.load()) {
            auto t0 = clock::now();
            console_bar_.print(bridge_.status_snapshot());
            if (bridge_.is_done()) {
                console_bar_.done(bridge_.status_snapshot());
                break;
            }
            double cost  = std::chrono::duration<double>(clock::now() - t0).count();
            double sleep = tick_s - cost;
            if (sleep > 0.0)
                std::this_thread::sleep_for(std::chrono::duration<double>(sleep));
        }
    }

    DisplayConfig                          cfg_;
    BatchWindowBridge                      bridge_;
    ConsoleProgressBar                     console_bar_;
    std::unique_ptr<Window, WindowDeleter> window_;
    std::atomic<bool>                      close_requested_{false};
};

} // namespace vis
} // namespace vsepr
