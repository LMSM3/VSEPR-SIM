#include "renderer3d/win32_window.hpp"

#ifdef _WIN32

#include <cstring>
#include <stdexcept>

namespace vsepr {
namespace renderer3d {

namespace {

constexpr const wchar_t* kWindowClassName = L"VseprRenderer3DWindow";

std::wstring to_wstring(const std::string& s) {
	if (s.empty()) return {};
	const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size);
	return result;
}

} // namespace

Win32Window::Win32Window(int width, int height, const std::string& title)
	: client_width_(width), client_height_(height), title_(title) {}

Win32Window::~Win32Window() {
	cleanup();
}

bool Win32Window::initialize() {
	HINSTANCE instance = GetModuleHandle(nullptr);

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = window_proc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOWFRAME);
	wc.lpszClassName = kWindowClassName;
	if (!RegisterClassExW(&wc)) {
		if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
	}

	RECT rect{0, 0, client_width_, client_height_};
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

	hwnd_ = CreateWindowExW(
		0,
		kWindowClassName,
		to_wstring(title_).c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		instance,
		this);

	if (!hwnd_) return false;

	SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	hdc_ = GetDC(hwnd_);
	resize_backbuffer(client_width_, client_height_);
	return true;
}

void Win32Window::show() {
	if (hwnd_) ShowWindow(hwnd_, SW_SHOW);
}

void Win32Window::set_title(const std::string& title) {
	title_ = title;
	if (hwnd_) SetWindowTextW(hwnd_, to_wstring(title_).c_str());
}

bool Win32Window::pump_one_frame() {
	MSG msg{};
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			should_close_ = true;
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	if (should_close_) return false;
	if (frame_callback_) frame_callback_();
	return true;
}

void Win32Window::present(const std::uint32_t* pixels, int width, int height) {
	if (!mem_dc_ || !dib_pixels_ || width != client_width_ || height != client_height_) return;
	const std::size_t count = static_cast<std::size_t>(width) * height;
	std::memcpy(dib_pixels_, pixels, count * sizeof(std::uint32_t));

	BitBlt(hdc_, 0, 0, width, height, mem_dc_, 0, 0, SRCCOPY);
}

void Win32Window::notify_resize(int width, int height) {
	client_width_ = width;
	client_height_ = height;
	resize_backbuffer(width, height);
}

void Win32Window::resize_backbuffer(int width, int height) {
	if (mem_dc_) {
		SelectObject(mem_dc_, old_bitmap_);
		DeleteObject(dib_bitmap_);
		DeleteDC(mem_dc_);
	}

	hdc_ = GetDC(hwnd_);
	mem_dc_ = CreateCompatibleDC(hdc_);

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	dib_bitmap_ = CreateDIBSection(mem_dc_, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&dib_pixels_), nullptr, 0);
	old_bitmap_ = static_cast<HBITMAP>(SelectObject(mem_dc_, dib_bitmap_));
}

void Win32Window::cleanup() {
	if (mem_dc_) {
		SelectObject(mem_dc_, old_bitmap_);
		DeleteObject(dib_bitmap_);
		DeleteDC(mem_dc_);
		mem_dc_ = nullptr;
	}
	if (hwnd_ && hdc_) {
		ReleaseDC(hwnd_, hdc_);
		hdc_ = nullptr;
	}
	if (hwnd_) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
}

LRESULT CALLBACK Win32Window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (msg) {
	case WM_SIZE: {
		if (self) {
			const int width = LOWORD(lparam);
			const int height = HIWORD(lparam);
			self->notify_resize(width, height);
		}
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		if (self && self->key_callback_) self->key_callback_(static_cast<int>(wparam), true);
		if (wparam == VK_ESCAPE) PostQuitMessage(0);
		return 0;
	case WM_KEYUP:
		if (self && self->key_callback_) self->key_callback_(static_cast<int>(wparam), false);
		return 0;
	default:
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}
}

} // namespace renderer3d
} // namespace vsepr

#endif // _WIN32
