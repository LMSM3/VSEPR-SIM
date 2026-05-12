#include "vis/auto_pilot.hpp"
#include "vis/renderer.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

// stb_image_write implementation (once per translation unit)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb_image_write.h"

// GL header (same as renderer)
#ifdef _WIN32
#  include <windows.h>
#endif
#include <GL/gl.h>

namespace vsepr {

AutoPilot::AutoPilot() = default;

void AutoPilot::tick(Camera& camera, double dt, int fb_width, int fb_height) {
    // --- Auto-spin ---
    if (spin_enabled_) {
        double dx = static_cast<double>(spin_speed_) * dt;
        double dy = 0.0;

        // Optional vertical wobble (sinusoidal)
        if (wobble_amp_ > 0.0f) {
            spin_phase_ += static_cast<float>(dt) * 1.5f;  // ~1.5 rad/s wobble
            dy = static_cast<double>(wobble_amp_) * std::sin(spin_phase_) * dt;
        }

        camera.orbit(dx, dy);
    }

    // --- Auto-capture ---
    if (capture_enabled_ && fb_width > 0 && fb_height > 0) {
        capture_timer_ += static_cast<float>(dt);
        if (capture_timer_ >= capture_interval_) {
            capture_timer_ -= capture_interval_;
            capture_now(fb_width, fb_height);
        }
    }
}

void AutoPilot::capture_now(int fb_width, int fb_height) {
    namespace fs = std::filesystem;

    // Ensure output directory exists
    fs::create_directories(capture_dir_);

    // Build filename: captures/frame_0001.png
    ++capture_count_;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "frame_%04d.png", capture_count_);
    std::string path = (fs::path(capture_dir_) / buf).string();

    save_framebuffer_png(fb_width, fb_height, path);
}

void AutoPilot::save_framebuffer_png(int width, int height, const std::string& path) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL gives bottom-up; flip vertically for PNG (top-down)
    int stride = width * 3;
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < height; ++y) {
        std::memcpy(flipped.data() + y * stride,
                     pixels.data() + (height - 1 - y) * stride,
                     stride);
    }

    if (stbi_write_png(path.c_str(), width, height, 3, flipped.data(), stride)) {
        std::cout << "[AutoPilot] Captured " << path << "\n";
    } else {
        std::cerr << "[AutoPilot] Failed to write " << path << "\n";
    }
}

} // namespace vsepr

