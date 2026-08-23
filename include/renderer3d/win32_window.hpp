#pragma once

/**
 * renderer3d/win32_window.hpp
 * ---------------------------
 * Minimal Win32 window with a DIB-section backbuffer.  No GLFW.
 */

#ifdef _WIN32

#include "renderer3d/math.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <windows.h>

namespace vsepr {
namespace renderer3d {

using FrameCallback = std::function<void()>;
using KeyCallback = std::function<void(int key, bool pressed)>;

class Win32Window {
public:
	Win32Window(int width, int height, const std::string& title);
	~Win32Window();

	[[nodiscard]] bool initialize();
	void show();

	/**
	 * Pump messages for one frame. Returns false when the window is closed.
	 */
	[[nodiscard]] bool pump_one_frame();

	/**
	 * Blit a BGRA32 pixel buffer to the window client area.
	 */
	void present(const std::uint32_t* pixels, int width, int height);

	[[nodiscard]] bool should_close() const noexcept { return should_close_; }
	[[nodiscard]] int client_width() const noexcept { return client_width_; }
	[[nodiscard]] int client_height() const noexcept { return client_height_; }

	/**
	 * Update the window title text.
	 */
	void set_title(const std::string& title);

	void set_frame_callback(FrameCallback cb) { frame_callback_ = std::move(cb); }
	void set_key_callback(KeyCallback cb) { key_callback_ = std::move(cb); }

	// Static helper to call from window proc for resize.
	void notify_resize(int width, int height);

private:
	int client_width_;
	int client_height_;
	std::string title_;
	HWND hwnd_ = nullptr;
	HDC hdc_ = nullptr;
	HDC mem_dc_ = nullptr;
	HBITMAP dib_bitmap_ = nullptr;
	HBITMAP old_bitmap_ = nullptr;
	std::uint32_t* dib_pixels_ = nullptr;
	bool should_close_ = false;

	FrameCallback frame_callback_;
	KeyCallback key_callback_;

	static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void resize_backbuffer(int width, int height);
	void cleanup();
};

} // namespace renderer3d
} // namespace vsepr

#endif // _WIN32
