/**
 * vis_sw_viewer.cpp
 * -----------------
 * Homegrown software-rasterizer 3D viewer for the DFAE live data pipeline.
 *
 * Uses the Win32 API for windowing and a VSEPR-native software rasterizer.
 * No GLFW, GLEW, GLM, Qt, or VTK.
 *
 * Commands:
 *   create cube 2
 *   load molecule.xyz
 *   translate 0 0 -5
 *   rotate y 45
 *   scale 2 1 1
 *   wireframe
 *   solid
 *   color R G B
 *   bg R G B
 *   orbit-speed DEG
 *   fit
 *   screenshot PATH.png
 *   grid
 *   axes
 *   quit
 *
 * Window shortcuts (focus the viewer window):
 *   SPACE — pause/resume orbit
 *   G — toggle ground grid
 *   A — toggle axes
 *   F — fit camera
 *   F5 — screenshot
 *   F1 — show this help
 */

#include "dfae/io/render_snapshot.hpp"
#include "dfae/live/live_command_reactor.hpp"
#include "renderer3d/math.hpp"
#include "renderer3d/mesh.hpp"
#include "renderer3d/rasterizer.hpp"

#ifdef _WIN32
#include "renderer3d/win32_window.hpp"
#include <windows.h>
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

struct ViewerState {
	std::mutex mutex;
	vsepr::renderer3d::Mesh mesh;
	vsepr::renderer3d::Color base_color = vsepr::renderer3d::Color::from_float(0.6f, 0.75f, 0.9f);
	vsepr::renderer3d::Color bg_color = vsepr::renderer3d::Color::from_float(0.05f, 0.07f, 0.09f);
	bool wireframe = false;
	bool dirty = true;

	// Cached mesh bounds; updated only when mesh changes. Positions are in
	// renderer-local space because Mesh::from_canonical_mesh bakes the frame.
	vsepr::renderer3d::Vec3f mesh_min{0.0f, 0.0f, 0.0f};
	vsepr::renderer3d::Vec3f mesh_max{0.0f, 0.0f, 0.0f};
	bool mesh_bounds_valid = false;

	// Camera state shared between command thread and render loop.
	float orbit_speed = 0.5f;   // degrees per second
	float camera_radius = 6.0f; // distance from target
	vsepr::renderer3d::Vec3f camera_target{0.0f, 0.0f, 0.0f};
	float camera_height = 1.5f; // Y offset above target
	bool refit_camera = true;   // true when mesh changes until fit is applied

	// Screenshot path requested by the command thread; picked up by render loop.
	std::string pending_screenshot;
	std::string overlay_text;
	float overlay_decay = 0.0f;

	// Rendering overlays and controls.
	bool show_grid = true;
	bool show_axes = true;
	bool orbit_paused = false;
	bool show_fps = true;

	// Title update state.
	int frame_count = 0;
	float fps_elapsed = 0.0f;
	float last_fps = 0.0f;
	std::string current_title;
};

void recompute_mesh_bounds(ViewerState& state) {
	if (state.mesh.vertices.empty()) {
		state.mesh_bounds_valid = false;
		return;
	}
	state.mesh_min = state.mesh_max = state.mesh.vertices.front().position;
	for (const auto& v : state.mesh.vertices) {
		state.mesh_min.x = std::min(state.mesh_min.x, v.position.x);
		state.mesh_min.y = std::min(state.mesh_min.y, v.position.y);
		state.mesh_min.z = std::min(state.mesh_min.z, v.position.z);
		state.mesh_max.x = std::max(state.mesh_max.x, v.position.x);
		state.mesh_max.y = std::max(state.mesh_max.y, v.position.y);
		state.mesh_max.z = std::max(state.mesh_max.z, v.position.z);
	}
	state.mesh_bounds_valid = true;
}

void apply_keyboard_shortcut(ViewerState& state, int key, bool pressed) {
	if (!pressed) return;
	std::lock_guard<std::mutex> lock(state.mutex);
	switch (key) {
	case ' ':
		state.orbit_paused = !state.orbit_paused;
		state.overlay_text = state.orbit_paused ? "orbit paused" : "orbit resumed";
		state.overlay_decay = 1.5f;
		break;
	case 'G':
	case 'g':
		state.show_grid = !state.show_grid;
		state.overlay_text = state.show_grid ? "grid on" : "grid off";
		state.overlay_decay = 1.5f;
		break;
	case 'A':
	case 'a':
		state.show_axes = !state.show_axes;
		state.overlay_text = state.show_axes ? "axes on" : "axes off";
		state.overlay_decay = 1.5f;
		break;
	case 'F':
	case 'f':
		state.refit_camera = true;
		state.overlay_text = "camera fit";
		state.overlay_decay = 2.0f;
		break;
	case VK_F5:
		state.pending_screenshot = "screenshot_" + std::to_string(state.frame_count) + ".png";
		state.overlay_text = "queued " + state.pending_screenshot;
		state.overlay_decay = 2.0f;
		break;
	case VK_F1:
		state.overlay_text = "SPACE pause | G grid | A axes | F fit | F5 screenshot";
		state.overlay_decay = 5.0f;
		break;
	}
}

void update_title(vsepr::renderer3d::Win32Window& window, ViewerState& state) {
	const int fps = static_cast<int>(state.last_fps + 0.5f);
	std::ostringstream oss;
	oss << "VSEPR 3D Viewer — " << (state.wireframe ? "wireframe" : "solid");
	if (state.show_fps) oss << " | " << fps << " FPS";
	if (state.orbit_paused) oss << " | PAUSED";
	const auto title = oss.str();
	if (title != state.current_title) {
		state.current_title = title;
		window.set_title(title);
	}
}

void console_command_loop(dfae::live::LiveCommandReactor& reactor, ViewerState& state, std::atomic<bool>& running) {
	std::cout << "VSEPR homegrown 3D viewer command shell\n";
	std::cout << reactor.help_text() << "\n";
	std::cout << "extra: wireframe | solid | color R G B | bg R G B | orbit-speed DEG | fit | screenshot PATH.png | grid | axes\n";

	std::string line;
	while (running.load() && std::getline(std::cin, line)) {
		auto first = line.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) continue;
		auto last = line.find_last_not_of(" \t\r\n");
		const auto trimmed = line.substr(first, last - first + 1);
		if (trimmed.empty()) continue;
		if (trimmed == "quit" || trimmed == "exit" || trimmed == "q") {
			running.store(false);
			break;
		}

		if (trimmed == "wireframe") {
			std::lock_guard<std::mutex> lock(state.mutex);
			state.wireframe = true;
			state.dirty = true;
			state.overlay_text = "mode: wireframe";
			state.overlay_decay = 1.5f;
			std::cout << "mode: wireframe\n";
			continue;
		}
		if (trimmed == "solid") {
			std::lock_guard<std::mutex> lock(state.mutex);
			state.wireframe = false;
			state.dirty = true;
			state.overlay_text = "mode: solid";
			state.overlay_decay = 1.5f;
			std::cout << "mode: solid\n";
			continue;
		}
		if (trimmed.rfind("color ", 0) == 0) {
			std::istringstream iss(trimmed.substr(6));
			float r, g, b;
			if (iss >> r >> g >> b) {
				std::lock_guard<std::mutex> lock(state.mutex);
				state.base_color = vsepr::renderer3d::Color::from_float(r, g, b);
				state.dirty = true;
				state.overlay_text = "color updated";
				state.overlay_decay = 1.5f;
			}
			continue;
		}
		if (trimmed.rfind("orbit-speed ", 0) == 0) {
			std::istringstream iss(trimmed.substr(12));
			float deg_per_sec = 0.5f;
			if (iss >> deg_per_sec) {
				std::lock_guard<std::mutex> lock(state.mutex);
				state.orbit_speed = deg_per_sec;
				std::cout << "orbit-speed set to " << deg_per_sec << " deg/s\n";
			}
			continue;
		}
		if (trimmed == "fit") {
			std::lock_guard<std::mutex> lock(state.mutex);
			state.refit_camera = true;
			state.overlay_text = "camera fit";
			state.overlay_decay = 2.0f;
			continue;
		}
		if (trimmed.rfind("bg ", 0) == 0) {
			std::istringstream iss(trimmed.substr(3));
			float r, g, b;
			if (iss >> r >> g >> b) {
				std::lock_guard<std::mutex> lock(state.mutex);
				state.bg_color = vsepr::renderer3d::Color::from_float(r, g, b);
				state.dirty = true;
				state.overlay_text = "bg updated";
				state.overlay_decay = 1.5f;
			}
			continue;
		}
		if (trimmed.rfind("screenshot ", 0) == 0) {
			std::lock_guard<std::mutex> lock(state.mutex);
			state.pending_screenshot = trimmed.substr(11);
			state.overlay_text = "screenshot queued";
			state.overlay_decay = 1.5f;
			continue;
		}
		if (trimmed == "grid") {
			std::lock_guard<std::mutex> lock(state.mutex);
			state.show_grid = !state.show_grid;
			state.overlay_text = state.show_grid ? "grid on" : "grid off";
			state.overlay_decay = 1.5f;
			continue;
		}
		if (trimmed == "axes") {
			std::lock_guard<std::mutex> lock(state.mutex);
			state.show_axes = !state.show_axes;
			state.overlay_text = state.show_axes ? "axes on" : "axes off";
			state.overlay_decay = 1.5f;
			continue;
		}

		auto result = reactor.execute(trimmed);
		if (std::holds_alternative<dfae::live::CommandOk>(result)) {
			std::lock_guard<std::mutex> lock(state.mutex);
			const bool is_create = (trimmed.rfind("create ", 0) == 0);
			state.mesh = vsepr::renderer3d::Mesh::from_canonical_mesh(reactor.state().published_mesh);
			state.mesh.set_color(state.base_color);
			recompute_mesh_bounds(state);
			state.dirty = true;
			if (is_create) {
				state.refit_camera = true;
				state.overlay_text = "created mesh";
				state.overlay_decay = 1.5f;
			}
			std::cout << std::get<dfae::live::CommandOk>(result).echo << "\n";
		} else {
			const auto& err = std::get<dfae::live::CommandError>(result);
			std::cerr << "error: " << err.message << "\n";
		}
	}
}

} // namespace

int main() {
#ifdef _WIN32
	dfae::live::LiveCommandReactor reactor;
	ViewerState state;
	std::atomic<bool> running{true};

	// Start with a default cube so the window is not empty.
	reactor.execute("create cube 2");
	{
		std::lock_guard<std::mutex> lock(state.mutex);
		state.mesh = vsepr::renderer3d::Mesh::from_canonical_mesh(reactor.state().published_mesh);
		state.mesh.set_color(state.base_color);
		recompute_mesh_bounds(state);
		state.dirty = true;
		state.refit_camera = true;
	}

	vsepr::renderer3d::Win32Window window(800, 600, "VSEPR Homegrown 3D Viewer - vis-sw-viewer");
	if (!window.initialize()) {
		std::cerr << "Failed to create window\n";
		return 1;
	}
	window.show();

	std::thread command_thread(console_command_loop, std::ref(reactor), std::ref(state), std::ref(running));

	window.set_key_callback([&state](int key, bool pressed) { apply_keyboard_shortcut(state, key, pressed); });

	vsepr::renderer3d::SoftwareRasterizer rasterizer;
	rasterizer.resize(window.client_width(), window.client_height());

	float orbit_angle = 0.0f;

	auto last_time = std::chrono::steady_clock::now();

	while (window.pump_one_frame()) {
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - last_time).count();
		last_time = now;

		int width = window.client_width();
		int height = window.client_height();
		rasterizer.resize(width, height);

		vsepr::renderer3d::RenderParams params;
		params.viewport = {0, 0, width, height};
		params.light_direction = vsepr::renderer3d::Vec3f{0.0f, 0.0f, 1.0f}.normalized();

		{
			std::lock_guard<std::mutex> lock(state.mutex);
			if (!state.orbit_paused) {
				orbit_angle += state.orbit_speed * dt * 3.14159265f / 180.0f;
			}
			++state.frame_count;
			state.fps_elapsed += dt;
			if (state.fps_elapsed >= 0.5f) {
				state.last_fps = state.frame_count / state.fps_elapsed;
				state.frame_count = 0;
				state.fps_elapsed = 0.0f;
				update_title(window, state);
			}

			// Auto-fit camera when geometry changes or user requests `fit`.
				if (state.refit_camera && state.mesh_bounds_valid) {
					state.camera_target = (state.mesh_min + state.mesh_max) * 0.5f;
					const auto extent = state.mesh_max - state.mesh_min;
					const auto max_extent = std::max({extent.x, extent.y, extent.z});
					state.camera_radius = std::max(max_extent * 1.8f, 2.0f);
					state.camera_height = state.camera_target.y + max_extent * 0.3f;
					state.refit_camera = false;
				}

			const auto eye = vsepr::renderer3d::Vec3f{
				state.camera_target.x + state.camera_radius * std::sin(orbit_angle),
				state.camera_height,
				state.camera_target.z + state.camera_radius * std::cos(orbit_angle)};
			params.view = vsepr::renderer3d::Mat4f::look_at(
				eye, state.camera_target, vsepr::renderer3d::Vec3f{0.0f, 1.0f, 0.0f});
			params.projection = vsepr::renderer3d::Mat4f::perspective(
				60.0f * 3.14159265f / 180.0f,
				width > 0 && height > 0 ? static_cast<float>(width) / height : 1.0f,
				0.1f,
				1000.0f);
			params.base_color = state.base_color;
			params.wireframe = state.wireframe;
			rasterizer.clear(state.bg_color);

			if (state.show_grid) {
				rasterizer.draw_grid(params, 10.0f, 1.0f, vsepr::renderer3d::Color{64, 64, 64});
			}
			if (state.show_axes) {
				rasterizer.draw_axes(params, 2.0f);
			}

			if (!state.mesh.empty()) {
				rasterizer.render_mesh(state.mesh, params);
			}

			// Fade out on-screen feedback.
			state.overlay_decay -= dt;
			if (state.overlay_decay > 0.0f && !state.overlay_text.empty()) {
				rasterizer.draw_text(8, 8, state.overlay_text, vsepr::renderer3d::Color::from_float(1.0f, 1.0f, 0.8f));
			}

			// Execute any queued screenshot right after presenting the scene.
			if (!state.pending_screenshot.empty()) {
				const auto png = rasterizer.to_png();
				if (!png.empty()) {
					std::ofstream file(state.pending_screenshot, std::ios::binary);
					if (file) {
						file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
						state.overlay_text = "saved " + state.pending_screenshot;
					} else {
						state.overlay_text = "failed writing " + state.pending_screenshot;
					}
				} else {
					state.overlay_text = "failed encoding screenshot";
				}
				state.overlay_decay = 2.0f;
				state.pending_screenshot.clear();
			}
		}

		window.present(rasterizer.framebuffer().pixels(), width, height);

		// Throttle to ~60 FPS.
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	running.store(false);
	command_thread.join();
	return 0;
#else
	std::cerr << "vis_sw_viewer is Windows-only in this milestone.\n";
	return 1;
#endif
}
