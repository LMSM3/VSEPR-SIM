#include "renderer3d/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace vsepr {
namespace renderer3d {

namespace {

struct Vec3Key {
	std::int32_t x, y, z;
	bool operator==(const Vec3Key& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
};

struct Vec3Hash {
	std::size_t operator()(const Vec3Key& k) const noexcept {
		std::size_t h = 0x9e3779b9;
		h = h * 31 + static_cast<std::uint32_t>(k.x);
		h = h * 31 + static_cast<std::uint32_t>(k.y);
		h = h * 31 + static_cast<std::uint32_t>(k.z);
		return h;
	}
};

// Quantize a unit normal/int position for welding.
Vec3Key quantize(const Vec3f& v, float scale) {
	return {
		static_cast<std::int32_t>(std::lroundf(v.x * scale)),
		static_cast<std::int32_t>(std::lroundf(v.y * scale)),
		static_cast<std::int32_t>(std::lroundf(v.z * scale))};
}

// Add a vertex, welding with an existing one if within tolerance, returning index.
std::uint32_t add_vertex(Mesh& mesh, const Vec3f& pos, const Vec3f& normal, const Color& color, float weld_scale,
	std::unordered_map<Vec3Key, std::uint32_t, Vec3Hash>& weld_map) {
	const auto key = quantize(pos, weld_scale);
	const auto it = weld_map.find(key);
	if (it != weld_map.end()) {
		auto& v = mesh.vertices[it->second];
		v.normal = (v.normal + normal).normalized();
		return it->second;
	}
	const std::uint32_t idx = static_cast<std::uint32_t>(mesh.vertices.size());
	mesh.vertices.push_back(Vertex3D{pos, normal.normalized(), color});
	weld_map.emplace(key, idx);
	return idx;
}

} // namespace

Mesh Mesh::from_canonical_mesh(const dfae::reconstruction::CanonicalMesh& canonical) {
	Mesh mesh;
	if (canonical.empty()) return mesh;

	mesh.vertices.reserve(canonical.faces.size() * 3);
	mesh.indices.reserve(canonical.faces.size() * 3);

	std::uint32_t index = 0;
	for (const auto& face : canonical.faces) {
		const auto n = Vec3f(face.normal).normalized();
		for (int i = 0; i < 3; ++i) {
			const auto v = Vec3f(canonical.vertices[face.triangle.index[i]]);
			mesh.vertices.push_back(Vertex3D{v, n, Color::from_float(0.8f, 0.8f, 0.8f)});
			mesh.indices.push_back(index++);
		}
	}
	return mesh;
}

void Mesh::set_color(const Color& color) {
	for (auto& vertex : vertices) vertex.color = color;
}

Mesh PrimitiveFactory::icosphere(float radius, int subdivisions) {
	Mesh mesh;
	if (subdivisions < 0) subdivisions = 0;
	if (subdivisions > 5) subdivisions = 5;

	// Initial icosahedron vertices.
	constexpr float t = 1.61803398874989484820f; // golden ratio
	std::vector<Vec3f> positions = {
		{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
		{0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
		{t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};

	std::vector<std::uint32_t> indices = {
		0, 11, 5,   0, 5, 1,    0, 1, 7,    0, 7, 10,   0, 10, 11,
		1, 5, 9,    5, 11, 4,   11, 10, 2,  10, 7, 6,   7, 1, 8,
		3, 9, 4,    3, 4, 2,    3, 2, 6,    3, 6, 8,    3, 8, 9,
		4, 9, 5,    2, 4, 11,   6, 2, 10,   8, 6, 7,    9, 8, 1};

	std::map<std::uint64_t, std::uint32_t> middle_cache;
	auto middle = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
		std::uint64_t key = (static_cast<std::uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
		auto it = middle_cache.find(key);
		if (it != middle_cache.end()) return it->second;
		const Vec3f p = (positions[a] + positions[b]) * 0.5f;
		positions.push_back(p);
		std::uint32_t idx = static_cast<std::uint32_t>(positions.size() - 1);
		middle_cache[key] = idx;
		return idx;
	};

	for (int s = 0; s < subdivisions; ++s) {
		std::vector<std::uint32_t> next;
		next.reserve(indices.size() * 4);
		for (std::size_t i = 0; i < indices.size(); i += 3) {
			std::uint32_t a = indices[i];
			std::uint32_t b = indices[i + 1];
			std::uint32_t c = indices[i + 2];
			std::uint32_t ab = middle(a, b);
			std::uint32_t bc = middle(b, c);
			std::uint32_t ca = middle(c, a);
			next.insert(next.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
		}
		indices = std::move(next);
		middle_cache.clear();
	}

	// Normalize to sphere and assign smooth normals (same as position direction).
	mesh.vertices.reserve(positions.size());
	for (const auto& p : positions) {
		Vec3f n = p.normalized();
		mesh.vertices.push_back(Vertex3D{n * radius, n, Color::from_float(0.8f, 0.8f, 0.8f)});
	}
	mesh.indices = std::move(indices);
	return mesh;
}

Mesh PrimitiveFactory::cylinder(float radius, float height, int sectors) {
	Mesh mesh;
	if (sectors < 3) sectors = 3;
	if (sectors > 256) sectors = 256;

	const float half_h = height * 0.5f;
	const Color color = Color::from_float(0.8f, 0.8f, 0.8f);

	// We weld side vertices so each rim point shares a single normal computed
	// from the adjacent faces, but keep cap centers separate so cap normals are
	// flat (top/bottom). Use a quantized hash for welding.
	std::unordered_map<Vec3Key, std::uint32_t, Vec3Hash> weld_map;
	constexpr float weld_scale = 1.0e4f;

	auto wedge = [&](const Vec3f& pos, const Vec3f& normal) -> std::uint32_t {
		return add_vertex(mesh, pos, normal, color, weld_scale, weld_map);
	};

	// Side strip and caps.
	std::vector<std::uint32_t> top_ring;
	std::vector<std::uint32_t> bottom_ring;
	top_ring.reserve(sectors);
	bottom_ring.reserve(sectors);

	for (int i = 0; i < sectors; ++i) {
		const float theta = static_cast<float>(i) / sectors * 2.0f * 3.14159265358979323846f;
		const float c = std::cos(theta);
		const float s = std::sin(theta);
		const Vec3f top_pos{c * radius, s * radius, half_h};
		const Vec3f bot_pos{c * radius, s * radius, -half_h};
		const Vec3f side_n{c, s, 0.0f};

		top_ring.push_back(wedge(top_pos, side_n));
		bottom_ring.push_back(wedge(bot_pos, side_n));
	}

	const Vec3f top_center{0.0f, 0.0f, half_h};
	const Vec3f bot_center{0.0f, 0.0f, -half_h};
	const std::uint32_t top_ci = wedge(top_center, {0, 0, 1});
	const std::uint32_t bot_ci = wedge(bot_center, {0, 0, -1});

	for (int i = 0; i < sectors; ++i) {
		int j = (i + 1) % sectors;
		// Side quad as two triangles.
		mesh.indices.insert(mesh.indices.end(), {top_ring[i], bottom_ring[i], top_ring[j]});
		mesh.indices.insert(mesh.indices.end(), {top_ring[j], bottom_ring[i], bottom_ring[j]});
		// Top cap.
		mesh.indices.insert(mesh.indices.end(), {top_ci, top_ring[j], top_ring[i]});
		// Bottom cap (flip normal via winding).
		mesh.indices.insert(mesh.indices.end(), {bot_ci, bottom_ring[i], bottom_ring[j]});
	}

	return mesh;
}

} // namespace renderer3d
} // namespace vsepr
