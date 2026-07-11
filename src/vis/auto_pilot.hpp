#pragma once
/**
 * auto_pilot.hpp
 * --------------
 * Auto-spin and auto-capture features for the 3D visualization module.
 * Auto-spin continuously orbits the camera around the molecule.
 * Auto-capture periodically saves framebuffer snapshots to PNG.
 */

#include <string>

namespace vsepr {

class Camera;  // Forward: renderer.hpp Camera

/**
 * AutoPilot drives hands-free camera rotation and periodic screenshot capture.
 * Tick once per frame from the Window run loop.
 */
class AutoPilot {
public:
    AutoPilot();

    // ====== Auto-spin ======

    void set_spin_enabled(bool on)        { spin_enabled_ = on; }
    bool spin_enabled() const             { return spin_enabled_; }
    void toggle_spin()                    { spin_enabled_ = !spin_enabled_; }

    void  set_spin_speed(float speed)     { spin_speed_ = speed; }
    float spin_speed() const              { return spin_speed_; }

    void  set_wobble(float amp)           { wobble_amp_ = amp; }
    float wobble() const                  { return wobble_amp_; }

    // ====== Auto-capture ======

    void set_capture_enabled(bool on)     { capture_enabled_ = on; }
    bool capture_enabled() const          { return capture_enabled_; }
    void toggle_capture()                 { capture_enabled_ = !capture_enabled_; }

    void  set_capture_interval(float sec) { capture_interval_ = sec; }
    float capture_interval() const        { return capture_interval_; }

    void set_capture_dir(const std::string& dir) { capture_dir_ = dir; }
    const std::string& capture_dir() const       { return capture_dir_; }

    void capture_now(int fb_width, int fb_height);
    int capture_count() const { return capture_count_; }

    // ====== Per-frame tick ======

    void tick(Camera& camera, double dt, int fb_width, int fb_height);

private:
    bool  spin_enabled_  = false;
    float spin_speed_    = 120.0f;
    float wobble_amp_    = 0.0f;
    float spin_phase_    = 0.0f;

    bool        capture_enabled_  = false;
    float       capture_interval_ = 2.0f;
    float       capture_timer_    = 0.0f;
    int         capture_count_    = 0;
    std::string capture_dir_      = "captures";

    void save_framebuffer_png(int width, int height, const std::string& path);
};

} // namespace vsepr
