#pragma once

/**
 * renderer3d/rasterizer.hpp
 * -------------------------
 * Minimal deterministic software rasterizer.
 *
 * Owns a color buffer and a depth buffer.  Renders triangle lists with a
 * simple lambert lighting model.  No dependencies.
 */

#include "renderer3d/math.hpp"
#include "renderer3d/mesh.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace vsepr {
namespace renderer3d {

struct Viewport {
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	[[nodiscard]] bool valid() const noexcept { return width > 0 && height > 0; }
};

struct Framebuffer {
	int width = 0;
	int height = 0;

	// Packed 0xAARRGGBB pixels in row-major order.
	std::vector<std::uint32_t> color;
	std::vector<float> depth;

	[[nodiscard]] bool valid() const noexcept { return width > 0 && height > 0; }

	void resize(int w, int h);
	void clear(const Color& color, float depth = 1.0f);
	void set_pixel(int x, int y, const Color& c, float z);

	[[nodiscard]] const std::uint32_t* pixels() const { return color.data(); }
};

struct RenderParams {
	Mat4f world;
	Mat4f view;
	Mat4f projection;
	Viewport viewport;
	Vec3f light_direction = Vec3f{0.0f, 0.0f, 1.0f}.normalized();
	Color base_color = Color::from_float(0.7f, 0.7f, 0.7f);
	bool wireframe = false;
};

class SoftwareRasterizer {
public:
	/**
	 * Resize the framebuffer.
	 */
	void resize(int width, int height);

	/**
	 * Clear buffers.
	 */
	void clear(const Color& color = Color::from_float(0.1f, 0.1f, 0.1f));

	/**
	 * Render a mesh into the framebuffer.
	 */
	void render_mesh(const Mesh& mesh, const RenderParams& params);

	/**
	 * Draw a monospace-ish 5x7 character at (x,y) using packed 0xAARRGGBB color.
	 * Coordinates are top-left. Clipped to the framebuffer.
	 */
	void draw_char(int x, int y, char ch, const Color& color);

	/**
	 * Draw a short text label at (x,y) using draw_char.
	 */
	void draw_text(int x, int y, std::string_view text, const Color& color);

	/**
	 * Draw a ground grid in the XZ plane at y=0 with the given world-space params.
	 * Extent is half-width on each axis.
	 */
	void draw_grid(const RenderParams& params, float extent = 10.0f, float step = 1.0f, const Color& color = Color{64, 64, 64});

	/**
	 * Draw world-space axes: X=red, Y=green, Z=blue.
	 */
	void draw_axes(const RenderParams& params, float length = 2.0f);

	[[nodiscard]] const Framebuffer& framebuffer() const noexcept { return framebuffer_; }

	/**
	 * Write the current framebuffer as a PPM image.
	 */
	[[nodiscard]] std::vector<std::uint8_t> to_ppm() const;

	/**
	 * Write the current framebuffer as a PNG image (no external deps).
	 * Returns empty vector on failure.
	 */
	[[nodiscard]] std::vector<std::uint8_t> to_png() const;

private:
	Framebuffer framebuffer_;

	void draw_world_line(const Vec3f& a, const Vec3f& b, const RenderParams& params, const Color& color);

	struct ProcessedVertex {
		Vec4f clip;
		Vec3f normal;
		Color color;
		Vec3f world_pos;
	};

	[[nodiscard]] std::vector<ProcessedVertex> process_vertices(
		const Mesh& mesh, const RenderParams& params) const;

	void draw_triangle(
		const ProcessedVertex& a,
		const ProcessedVertex& b,
		const ProcessedVertex& c,
		const RenderParams& params);

	void draw_line(int x0, int y0, int x1, int y1, const Color& color);
};

} // namespace renderer3d
} // namespace vsepr
