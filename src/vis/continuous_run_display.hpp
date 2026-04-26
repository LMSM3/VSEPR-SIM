#pragma once
// continuous_run_display.hpp
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

struct DisplayConfig {
    std::string run_label = "Batch Run";
    float display_fps = 5.0f;
    int win_width = 960;
    int win_height = 720;
    bool auto_close_on_done = true;
    bool console_fallback = true;
    VizConfig viz;
};

class ConsoleProgressBar {
public:
    explicit ConsoleProgressBar(int w = 40) : bar_width_(w) {}
    void print(const BatchStatus& s) const;
    void done(const BatchStatus& s) const;
private:
    int bar_width_;
};

class ContinuousRunDisplay {
public:
    ContinuousRunDisplay()  = default;
    ~ContinuousRunDisplay() { close(); }
    ContinuousRunDisplay(const ContinuousRunDisplay&)            = delete;
    ContinuousRunDisplay& operator=(const ContinuousRunDisplay&) = delete;
    void configure(const DisplayConfig& cfg) { cfg_ = cfg; }
    void configure(const std::string& label, float display_fps = 5.0f) {
        cfg_.run_label = label; cfg_.display_fps = display_fps;
    }
    bool open(int width = 960, int height = 720);
    void close() { close_requested_.store(true); }
    BatchWindowBridge& bridge() { return bridge_; }
    void run_event_loop();
private:
    void run_gl_loop();
    void run_console_loop();
    DisplayConfig           cfg_;
    BatchWindowBridge       bridge_;
    ConsoleProgressBar      console_bar_;
    std::unique_ptr<Window> window_;
    std::atomic<bool>       close_requested_{false};
};

} // namespace vis
} // namespace vsepr
