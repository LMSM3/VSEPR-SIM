#pragma once

/**
 * renderer3d/mesh.hpp
 * -------------------
 * Native renderer mesh representation and adapters from DFAE data.
 */

#include "renderer3d/math.hpp"
#include "dfae/reconstruction/canonical_mesh.hpp"

#include <cstdint>
#include <vector>

namespace vsepr {
namespace renderer3d {

struct Vertex3D {
	Vec3f position;
	Vec3f normal;
	Color color;
};

struct Material {
	Color diffuse = Color::from_float(0.7f, 0.7f, 0.7f);
	Color ambient = Color::from_float(0.1f, 0.1f, 0.1f);
	Color specular = Color::from_float(0.3f, 0.3f, 0.3f);
	float shininess = 16.0f;
};

struct Mesh {
	std::vector<Vertex3D> vertices;
	std::vector<std::uint32_t> indices;

	[[nodiscard]] bool empty() const noexcept { return indices.empty(); }
	[[nodiscard]] std::size_t triangle_count() const noexcept { return indices.size() / 3; }

	/**
	 * Build from a DFAE canonical mesh.  The canonical mesh is already a world
	 * triangle soup, so we expand each triangle into its own vertices and keep
	 * the per-triangle normal.  Returns an empty mesh if the input is empty.
	 */
	[[nodiscard]] static Mesh from_canonical_mesh(const dfae::reconstruction::CanonicalMesh& canonical);

	/**
	 * Apply a uniform material color to all vertices.
	 */
	void set_color(const Color& color);
};

/**
 * Procedural primitive factory. Generates smooth-normal meshes without
 * external geometry dependencies so the renderer can show high-quality
 * sphere/cylinder primitives directly.
 */
struct PrimitiveFactory {
	/**
	 * Generate an icosphere. Subdivisions 0..5 give 20..20480 triangles.
	 */
	[[nodiscard]] static Mesh icosphere(float radius, int subdivisions = 2);

	/**
	 * Generate a capped cylinder oriented along the Z axis.
	 * sectors controls the radial tessellation.
	 */
	[[nodiscard]] static Mesh cylinder(float radius, float height, int sectors = 24);
};

} // namespace renderer3d
} // namespace vsepr
