#include "renderer3d/rasterizer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

namespace vsepr {
namespace renderer3d {

namespace {

constexpr float kDepthClear = 1.0f;

inline int orient2d(int ax, int ay, int bx, int by, int cx, int cy) {
	return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

inline float edge_function(float ax, float ay, float bx, float by, float cx, float cy) {
	return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

// ============================================================================
// Minimal embedded PNG writer.
// ============================================================================

void png_write_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
	out.push_back(static_cast<std::uint8_t>(value >> 24));
	out.push_back(static_cast<std::uint8_t>(value >> 16));
	out.push_back(static_cast<std::uint8_t>(value >> 8));
	out.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t crc32_table[256];
bool crc32_table_ready = false;

void init_crc32() {
	if (crc32_table_ready) return;
	for (int i = 0; i < 256; ++i) {
		std::uint32_t c = static_cast<std::uint32_t>(i);
		for (int k = 0; k < 8; ++k) {
			c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
		}
		crc32_table[i] = c;
	}
	crc32_table_ready = true;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
	init_crc32();
	std::uint32_t c = 0xFFFFFFFFu;
	for (std::size_t i = 0; i < size; ++i) {
		c = crc32_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
	}
	return c ^ 0xFFFFFFFFu;
}

void adler32_update(std::uint32_t& a, std::uint32_t& b, const std::uint8_t* data, std::size_t size) {
	for (std::size_t i = 0; i < size; ++i) {
		a = (a + data[i]) % 65521;
		b = (b + a) % 65521;
	}
}
std::uint32_t adler32_finalize(std::uint32_t a, std::uint32_t b) { return (b << 16) | a; }

void write_png_chunk(std::vector<std::uint8_t>& out, const std::string& type, const std::uint8_t* data, std::size_t size) {
	png_write_u32(out, static_cast<std::uint32_t>(size));
	const std::size_t start = out.size();
	out.insert(out.end(), type.begin(), type.end());
	if (size) out.insert(out.end(), data, data + size);
	const std::uint32_t crc = crc32(out.data() + start, type.size() + size);
	png_write_u32(out, crc);
}

std::vector<std::uint8_t> zlib_compress(const std::vector<std::uint8_t>& src) {
	// Minimal compressor for PNG: store blocks with fixed 32K max size.
	std::vector<std::uint8_t> out;
	out.reserve(src.size() + 64);
	out.push_back(0x78); // CMF
	out.push_back(0x9C); // FLG

	std::size_t offset = 0;
	while (offset < src.size()) {
		const std::size_t chunk = std::min<std::size_t>(src.size() - offset, 65535);
		const bool last = (offset + chunk) >= src.size();
		std::uint8_t bfinal = last ? 0x01 : 0x00;
		std::uint8_t btype = 0x00; // stored block
		out.push_back(static_cast<std::uint8_t>(bfinal | (btype << 1)));
		out.push_back(static_cast<std::uint8_t>(chunk & 0xFF));
		out.push_back(static_cast<std::uint8_t>(chunk >> 8));
		out.push_back(static_cast<std::uint8_t>(~(chunk & 0xFF)));
		out.push_back(static_cast<std::uint8_t>((~(chunk >> 8)) & 0xFF));
		out.insert(out.end(), src.begin() + static_cast<std::ptrdiff_t>(offset), src.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
		offset += chunk;
	}

	std::uint32_t a = 1, b = 0;
	adler32_update(a, b, src.data(), src.size());
	const std::uint32_t adler = adler32_finalize(a, b);
	png_write_u32(out, adler);
	return out;
}

} // namespace

void Framebuffer::resize(int w, int h) {
	width = w;
	height = h;
	color.resize(static_cast<std::size_t>(w) * h, 0);
	depth.resize(static_cast<std::size_t>(w) * h, kDepthClear);
}

void Framebuffer::clear(const Color& c, float d) {
	const uint32_t packed = c.rgba();
	std::fill(color.begin(), color.end(), packed);
	std::fill(depth.begin(), depth.end(), d);
}

void Framebuffer::set_pixel(int x, int y, const Color& c, float z) {
	if (x < 0 || x >= width || y < 0 || y >= height) return;
	const auto index = static_cast<std::size_t>(y) * width + x;
	if (z < depth[index]) {
		depth[index] = z;
		color[index] = c.rgba();
	}
}

void SoftwareRasterizer::resize(int width, int height) {
	framebuffer_.resize(width, height);
}

void SoftwareRasterizer::clear(const Color& color) {
	framebuffer_.clear(color, kDepthClear);
}

static Color shade_vertex(const Vec3f& world_pos, const Vec3f& world_normal, const Color& base_color, const RenderParams& params) {
	const Material mat;
	const auto L = -params.light_direction.normalized();
	const auto V = (Vec3f{0.0f, 0.0f, 0.0f} - world_pos).normalized();
	const auto H = (L + V).normalized();

	const float lambert = std::max(0.0f, dot(world_normal, L));
	const float spec = std::pow(std::max(0.0f, dot(world_normal, H)), mat.shininess);

	auto comp = [](uint8_t base, uint8_t diff, uint8_t amb, uint8_t spec_c, float lambert, float spec) -> uint8_t {
		const float lit = (base / 255.0f) * ((amb / 255.0f) + (diff / 255.0f) * lambert + (spec_c / 255.0f) * spec);
		return static_cast<uint8_t>(std::clamp(lit * 255.0f, 0.0f, 255.0f));
	};

	return {
		comp(base_color.r, mat.diffuse.r, mat.ambient.r, mat.specular.r, lambert, spec),
		comp(base_color.g, mat.diffuse.g, mat.ambient.g, mat.specular.g, lambert, spec),
		comp(base_color.b, mat.diffuse.b, mat.ambient.b, mat.specular.b, lambert, spec)};
}

std::vector<SoftwareRasterizer::ProcessedVertex> SoftwareRasterizer::process_vertices(
	const Mesh& mesh, const RenderParams& params) const {
	std::vector<ProcessedVertex> result;
	result.reserve(mesh.vertices.size());

	const auto mvp = params.projection * params.view * params.world;
	const auto world_inv_t = params.world.inverted().transposed();

	for (const auto& v : mesh.vertices) {
		const Vec4f world_pos4 = params.world * Vec4f(v.position);
		const Vec3f world_pos = world_pos4.xyz();
		const Vec4f clip = mvp * Vec4f(v.position);
		const Vec3f world_normal = (world_inv_t * v.normal).normalized();

		// Use vertex color if not default grey; otherwise fall back to params base color.
		const Color base = (v.color.r == 0xCC && v.color.g == 0xCC && v.color.b == 0xCC)
			? params.base_color
			: v.color;
		const Color lit = shade_vertex(world_pos, world_normal, base, params);
		result.push_back({clip, world_normal, lit, world_pos});
	}
	return result;
}

void SoftwareRasterizer::render_mesh(const Mesh& mesh, const RenderParams& params) {
	if (!params.viewport.valid() || mesh.empty()) return;

	auto processed = process_vertices(mesh, params);

	for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
		const auto& a = processed[mesh.indices[i + 0]];
		const auto& b = processed[mesh.indices[i + 1]];
		const auto& c = processed[mesh.indices[i + 2]];
		draw_triangle(a, b, c, params);
	}
}

void SoftwareRasterizer::draw_triangle(
	const ProcessedVertex& a,
	const ProcessedVertex& b,
	const ProcessedVertex& c,
	const RenderParams& params) {
	// Viewport transform to screen space.
	auto to_screen = [&](const Vec4f& clip) -> Vec3f {
		if (std::abs(clip.w) < 1e-6f) return {0.0f, 0.0f, 0.0f};
		const float inv_w = 1.0f / clip.w;
		const float ndc_x = clip.x * inv_w;
		const float ndc_y = clip.y * inv_w;
		const float ndc_z = clip.z * inv_w;
		return {
			(ndc_x * 0.5f + 0.5f) * params.viewport.width + params.viewport.x,
			(-ndc_y * 0.5f + 0.5f) * params.viewport.height + params.viewport.y,
			ndc_z};
	};

	const auto sa = to_screen(a.clip);
	const auto sb = to_screen(b.clip);
	const auto sc = to_screen(c.clip);

	if (params.wireframe) {
		draw_line(static_cast<int>(sa.x), static_cast<int>(sa.y), static_cast<int>(sb.x), static_cast<int>(sb.y), a.color);
		draw_line(static_cast<int>(sb.x), static_cast<int>(sb.y), static_cast<int>(sc.x), static_cast<int>(sc.y), b.color);
		draw_line(static_cast<int>(sc.x), static_cast<int>(sc.y), static_cast<int>(sa.x), static_cast<int>(sa.y), c.color);
		return;
	}

	// Back-face culling using screen-space normal z.
	const auto face_normal = (sb - sa).cross(sc - sa).normalized();
	if (face_normal.z <= 0.0f) return;

	// Integer bounding box.
	int min_x = static_cast<int>(std::floor(std::min({sa.x, sb.x, sc.x})));
	int max_x = static_cast<int>(std::ceil(std::max({sa.x, sb.x, sc.x})));
	int min_y = static_cast<int>(std::floor(std::min({sa.y, sb.y, sc.y})));
	int max_y = static_cast<int>(std::ceil(std::max({sa.y, sb.y, sc.y})));

	min_x = std::max(min_x, params.viewport.x);
	max_x = std::min(max_x, params.viewport.x + params.viewport.width - 1);
	min_y = std::max(min_y, params.viewport.y);
	max_y = std::min(max_y, params.viewport.y + params.viewport.height - 1);

	// Sort vertices by y to walk scanlines with edge stepping.
	struct V2 {
		float x, y, z;
		Color color;
	};
	V2 p[3] = {{sa.x, sa.y, sa.z, a.color}, {sb.x, sb.y, sb.z, b.color}, {sc.x, sc.y, sc.z, c.color}};
	if (p[1].y < p[0].y) std::swap(p[0], p[1]);
	if (p[2].y < p[0].y) std::swap(p[0], p[2]);
	if (p[2].y < p[1].y) std::swap(p[1], p[2]);

	auto interpolate = [](const V2& a, const V2& b, float y) -> std::pair<float, Color> {
		const float dy = b.y - a.y;
		if (std::abs(dy) < 1e-6f) return {b.x, b.color};
		const float t = (y - a.y) / dy;
		const auto lerped = Color::lerp(a.color, b.color, t);
		return {a.x + t * (b.x - a.x), lerped};
	};

	auto z_interpolate = [](const V2& a, const V2& b, float y) -> float {
		const float dy = b.y - a.y;
		if (std::abs(dy) < 1e-6f) return b.z;
		const float t = (y - a.y) / dy;
		return a.z + t * (b.z - a.z);
	};

	auto fill_edges = [&](int top_y, int bottom_y, auto& left_x, auto& right_x, auto& left_color, auto& right_color, auto& left_z, auto& right_z) {
		for (int y = std::max(top_y, min_y); y <= std::min(bottom_y, max_y); ++y) {
			int x_start = static_cast<int>(std::ceil(std::max(left_x(y), static_cast<float>(min_x))));
			int x_end = static_cast<int>(std::floor(std::min(right_x(y), static_cast<float>(max_x))));
			if (x_start > x_end) continue;

			const auto [x0_color, x1_color] = std::pair<Color, Color>{left_color(y), right_color(y)};
			const float z_row0 = left_z(y);
			const float z_row1 = right_z(y);
			const float inv_w = static_cast<float>(x_end - x_start);
			const float z_step = (inv_w > 0.0f) ? (z_row1 - z_row0) / inv_w : 0.0f;
			float z = z_row0;

			std::size_t index = static_cast<std::size_t>(y) * framebuffer_.width + x_start;
			for (int x = x_start; x <= x_end; ++x) {
				if (z < framebuffer_.depth[index]) {
					framebuffer_.depth[index] = z;
					const float u = (inv_w > 0.0f) ? static_cast<float>(x - x_start) / inv_w : 0.0f;
					framebuffer_.color[index] = Color::lerp(x0_color, x1_color, u).rgba();
				}
				++index;
				z += z_step;
			}
		}
	};

	auto left_fn = [&](float y) -> float { return interpolate(p[0], p[2], y).first; };
	auto right_top_fn = [&](float y) -> float { return interpolate(p[0], p[1], y).first; };
	auto right_bot_fn = [&](float y) -> float { return interpolate(p[1], p[2], y).first; };
	auto left_color_fn = [&](float y) -> Color { return interpolate(p[0], p[2], y).second; };
	auto right_top_color_fn = [&](float y) -> Color { return interpolate(p[0], p[1], y).second; };
	auto right_bot_color_fn = [&](float y) -> Color { return interpolate(p[1], p[2], y).second; };
	auto left_z_fn = [&](float y) -> float { return z_interpolate(p[0], p[2], y); };
	auto right_top_z_fn = [&](float y) -> float { return z_interpolate(p[0], p[1], y); };
	auto right_bot_z_fn = [&](float y) -> float { return z_interpolate(p[1], p[2], y); };

	const int split_y = static_cast<int>(p[1].y);
	if (p[0].y < p[1].y) {
		fill_edges(static_cast<int>(p[0].y), split_y, left_fn, right_top_fn, left_color_fn, right_top_color_fn, left_z_fn, right_top_z_fn);
	}
	if (p[1].y < p[2].y) {
		fill_edges(split_y, static_cast<int>(p[2].y), left_fn, right_bot_fn, left_color_fn, right_bot_color_fn, left_z_fn, right_bot_z_fn);
	}
}

void SoftwareRasterizer::draw_line(int x0, int y0, int x1, int y1, const Color& color) {
	int dx = std::abs(x1 - x0);
	int dy = std::abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;

	while (true) {
		framebuffer_.set_pixel(x0, y0, color, 0.0f);
		if (x0 == x1 && y0 == y1) break;
		const int e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }
	}
}

void SoftwareRasterizer::draw_world_line(
	const Vec3f& a,
	const Vec3f& b,
	const RenderParams& params,
	const Color& color) {
	const auto mvp = params.projection * params.view * params.world;
	const auto project = [&](const Vec3f& p) -> Vec3f {
		Vec4f v = mvp * Vec4f(p);
		if (std::abs(v.w) < 1e-6f) return {-1000000.0f, -1000000.0f, 0.0f};
		v.x /= v.w;
		v.y /= v.w;
		v.z /= v.w;
		const float sx = (v.x * 0.5f + 0.5f) * params.viewport.width + params.viewport.x;
		const float sy = (-v.y * 0.5f + 0.5f) * params.viewport.height + params.viewport.y;
		return {sx, sy, v.z};
	};
	const auto pa = project(a);
	const auto pb = project(b);
	if (pa.x < params.viewport.x && pb.x < params.viewport.x) return;
	if (pa.x > params.viewport.x + params.viewport.width && pb.x > params.viewport.x + params.viewport.width) return;
	if (pa.y < params.viewport.y && pb.y < params.viewport.y) return;
	if (pa.y > params.viewport.y + params.viewport.height && pb.y > params.viewport.y + params.viewport.height) return;
	draw_line(static_cast<int>(pa.x), static_cast<int>(pa.y), static_cast<int>(pb.x), static_cast<int>(pb.y), color);
}

void SoftwareRasterizer::draw_grid(const RenderParams& params, float extent, float step, const Color& color) {
	const int n = static_cast<int>(extent / step);
	for (int i = -n; i <= n; ++i) {
		const float t = static_cast<float>(i) * step;
		draw_world_line(Vec3f{-extent, 0.0f, t}, Vec3f{extent, 0.0f, t}, params, color);
		draw_world_line(Vec3f{t, 0.0f, -extent}, Vec3f{t, 0.0f, extent}, params, color);
	}
}

void SoftwareRasterizer::draw_axes(const RenderParams& params, float length) {
	draw_world_line(Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{length, 0.0f, 0.0f}, params, Color{220, 48, 48});
	draw_world_line(Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{0.0f, length, 0.0f}, params, Color{48, 220, 48});
	draw_world_line(Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{0.0f, 0.0f, length}, params, Color{48, 96, 220});
}

// 5x7 monospace font: each glyph is 5 columns x 7 rows, 95 printable chars.
static constexpr std::size_t kFontGlyphs = 95;
static constexpr std::size_t kFontColumns = 5;
static constexpr std::array<std::uint8_t, kFontGlyphs * kFontColumns> kFont5x7 = {
	0x00, 0x00, 0x00, 0x00, 0x00, // space
	0x00, 0x00, 0x5F, 0x00, 0x00, // !
	0x00, 0x07, 0x00, 0x07, 0x00, // "
	0x14, 0x7F, 0x14, 0x7F, 0x14, // #
	0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
	0x23, 0x13, 0x08, 0x64, 0x62, // %
	0x36, 0x49, 0x55, 0x22, 0x50, // &
	0x00, 0x05, 0x03, 0x00, 0x00, // '
	0x00, 0x1C, 0x22, 0x41, 0x00, // (
	0x00, 0x41, 0x22, 0x1C, 0x00, // )
	0x08, 0x2A, 0x1C, 0x2A, 0x08, // *
	0x08, 0x08, 0x3E, 0x08, 0x08, // +
	0x00, 0x50, 0x30, 0x00, 0x00, // ,
	0x08, 0x08, 0x08, 0x08, 0x08, // -
	0x00, 0x60, 0x60, 0x00, 0x00, // .
	0x20, 0x10, 0x08, 0x04, 0x02, // /
	0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
	0x00, 0x42, 0x7F, 0x40, 0x00, // 1
	0x42, 0x61, 0x51, 0x49, 0x46, // 2
	0x21, 0x41, 0x45, 0x4B, 0x31, // 3
	0x18, 0x14, 0x12, 0x7F, 0x10, // 4
	0x27, 0x45, 0x45, 0x45, 0x39, // 5
	0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
	0x01, 0x71, 0x09, 0x05, 0x03, // 7
	0x36, 0x49, 0x49, 0x49, 0x36, // 8
	0x06, 0x49, 0x49, 0x29, 0x1E, // 9
	0x00, 0x36, 0x36, 0x00, 0x00, // :
	0x00, 0x56, 0x36, 0x00, 0x00, // ;
	0x00, 0x08, 0x14, 0x22, 0x41, // <
	0x14, 0x14, 0x14, 0x14, 0x14, // =
	0x41, 0x22, 0x14, 0x08, 0x00, // >
	0x02, 0x01, 0x51, 0x09, 0x06, // ?
	0x32, 0x49, 0x79, 0x41, 0x3E, // @
	0x7E, 0x11, 0x11, 0x11, 0x7E, // A
	0x7F, 0x49, 0x49, 0x49, 0x36, // B
	0x3E, 0x41, 0x41, 0x41, 0x22, // C
	0x7F, 0x41, 0x41, 0x22, 0x1C, // D
	0x7F, 0x49, 0x49, 0x49, 0x41, // E
	0x7F, 0x09, 0x09, 0x01, 0x01, // F
	0x3E, 0x41, 0x41, 0x51, 0x32, // G
	0x7F, 0x08, 0x08, 0x08, 0x7F, // H
	0x00, 0x41, 0x7F, 0x41, 0x00, // I
	0x20, 0x40, 0x41, 0x3F, 0x01, // J
	0x7F, 0x08, 0x14, 0x22, 0x41, // K
	0x7F, 0x40, 0x40, 0x40, 0x40, // L
	0x7F, 0x02, 0x04, 0x02, 0x7F, // M
	0x7F, 0x04, 0x08, 0x10, 0x7F, // N
	0x3E, 0x41, 0x41, 0x41, 0x3E, // O
	0x7F, 0x09, 0x09, 0x09, 0x06, // P
	0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
	0x7F, 0x09, 0x19, 0x29, 0x46, // R
	0x46, 0x49, 0x49, 0x49, 0x31, // S
	0x01, 0x01, 0x7F, 0x01, 0x01, // T
	0x3F, 0x40, 0x40, 0x40, 0x3F, // U
	0x1F, 0x20, 0x40, 0x20, 0x1F, // V
	0x7F, 0x20, 0x18, 0x20, 0x7F, // W
	0x63, 0x14, 0x08, 0x14, 0x63, // X
	0x03, 0x04, 0x78, 0x04, 0x03, // Y
	0x61, 0x51, 0x49, 0x45, 0x43, // Z
	0x00, 0x00, 0x7F, 0x41, 0x00, // [
	0x02, 0x04, 0x08, 0x10, 0x20, // backslash
	0x00, 0x41, 0x7F, 0x00, 0x00, // ]
	0x04, 0x02, 0x01, 0x02, 0x04, // ^
	0x40, 0x40, 0x40, 0x40, 0x40, // _
	0x00, 0x01, 0x02, 0x04, 0x00, // `
	0x20, 0x54, 0x54, 0x54, 0x78, // a
	0x7F, 0x48, 0x44, 0x44, 0x38, // b
	0x38, 0x44, 0x44, 0x44, 0x20, // c
	0x38, 0x44, 0x44, 0x48, 0x7F, // d
	0x38, 0x54, 0x54, 0x54, 0x18, // e
	0x08, 0x7E, 0x09, 0x01, 0x02, // f
	0x08, 0x14, 0x54, 0x54, 0x3C, // g
	0x7F, 0x08, 0x04, 0x04, 0x78, // h
	0x00, 0x44, 0x7D, 0x40, 0x00, // i
	0x20, 0x40, 0x44, 0x3D, 0x00, // j
	0x00, 0x7F, 0x10, 0x28, 0x44, // k
	0x00, 0x41, 0x7F, 0x40, 0x00, // l
	0x7C, 0x04, 0x18, 0x04, 0x78, // m
	0x7C, 0x08, 0x04, 0x04, 0x78, // n
	0x38, 0x44, 0x44, 0x44, 0x38, // o
	0x7C, 0x14, 0x14, 0x14, 0x08, // p
	0x08, 0x14, 0x14, 0x18, 0x7C, // q
	0x7C, 0x08, 0x04, 0x04, 0x08, // r
	0x48, 0x54, 0x54, 0x54, 0x20, // s
	0x04, 0x3F, 0x44, 0x40, 0x20, // t
	0x3C, 0x40, 0x40, 0x20, 0x7C, // u
	0x1C, 0x20, 0x40, 0x20, 0x1C, // v
	0x3C, 0x40, 0x30, 0x40, 0x3C, // w
	0x44, 0x28, 0x10, 0x28, 0x44, // x
	0x0C, 0x50, 0x50, 0x50, 0x3C, // y
	0x44, 0x64, 0x54, 0x4C, 0x44, // z
	0x00, 0x08, 0x36, 0x41, 0x00, // {
	0x00, 0x00, 0x7F, 0x00, 0x00, // |
	0x00, 0x41, 0x36, 0x08, 0x00, // }
	0x08, 0x08, 0x2A, 0x1C, 0x08, // ~
};

void SoftwareRasterizer::draw_char(int x, int y, char ch, const Color& color) {
	if (ch < 32 || ch > 126) ch = '?';
	const int index = (ch - 32) * static_cast<int>(kFontColumns);
	for (int row = 0; row < 7; ++row) {
		const std::uint8_t bits = kFont5x7[static_cast<std::size_t>(index + row)];
		for (int col = 0; col < 5; ++col) {
			if (bits & (1 << (4 - col))) {
				framebuffer_.set_pixel(x + col, y + row, color, 0.0f);
			}
		}
	}
}

void SoftwareRasterizer::draw_text(int x, int y, std::string_view text, const Color& color) {
	int cursor = x;
	for (char ch : text) {
		draw_char(cursor, y, ch, color);
		cursor += 6;
	}
}

std::vector<std::uint8_t> SoftwareRasterizer::to_ppm() const {
	std::ostringstream oss;
	oss << "P6\n" << framebuffer_.width << " " << framebuffer_.height << "\n255\n";
	const std::string header = oss.str();

	std::vector<std::uint8_t> result;
	result.reserve(header.size() + static_cast<std::size_t>(framebuffer_.width) * framebuffer_.height * 3);
	result.insert(result.end(), header.begin(), header.end());

	for (const auto pixel : framebuffer_.color) {
		result.push_back((pixel >> 0) & 0xFF);  // B
		result.push_back((pixel >> 8) & 0xFF);  // G
		result.push_back((pixel >> 16) & 0xFF); // R
	}
	return result;
}

std::vector<std::uint8_t> SoftwareRasterizer::to_png() const {
	std::vector<std::uint8_t> idat;
	for (int y = 0; y < framebuffer_.height; ++y) {
		idat.push_back(0); // filter byte: none
		for (int x = 0; x < framebuffer_.width; ++x) {
			const std::uint32_t pixel = framebuffer_.color[static_cast<std::size_t>(y) * framebuffer_.width + x];
			idat.push_back((pixel >> 16) & 0xFF); // R
			idat.push_back((pixel >> 8) & 0xFF);  // G
			idat.push_back((pixel >> 0) & 0xFF);  // B
		}
	}

	const auto compressed = zlib_compress(idat);

	std::vector<std::uint8_t> result;
	// PNG signature.
	const std::uint8_t signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	result.insert(result.end(), signature, signature + 8);

	std::vector<std::uint8_t> ihdr;
	png_write_u32(ihdr, static_cast<std::uint32_t>(framebuffer_.width));
	png_write_u32(ihdr, static_cast<std::uint32_t>(framebuffer_.height));
	ihdr.push_back(8);  // bit depth
	ihdr.push_back(2);  // color type RGB
	ihdr.push_back(0);  // compression
	ihdr.push_back(0);  // filter method
	ihdr.push_back(0);  // interlace
	write_png_chunk(result, "IHDR", ihdr.data(), ihdr.size());
	write_png_chunk(result, "IDAT", compressed.data(), compressed.size());
	write_png_chunk(result, "IEND", nullptr, 0);
	return result;
}

} // namespace renderer3d
} // namespace vsepr
