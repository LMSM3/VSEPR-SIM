#include "dfae/live/live_command_reactor.hpp"
#include "renderer3d/math.hpp"
#include "renderer3d/mesh.hpp"
#include "renderer3d/rasterizer.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] int check(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << "\n";
		return 1;
	}
	return 0;
}

} // namespace

int main() {
	using namespace vsepr::renderer3d;
	int failures = 0;

	// 1. Math sanity checks.
	{
		const auto tr = Mat4f::translate({1.0f, 2.0f, 3.0f}) * Vec3f{0.0f, 0.0f, 0.0f};
		failures += check(std::abs(tr.x - 1.0f) < 1e-4f, "translation moves point");
		failures += check(std::abs(tr.y - 2.0f) < 1e-4f, "translation moves point y");
		failures += check(std::abs(tr.z - 3.0f) < 1e-4f, "translation moves point z");
	}
	{
		const auto rot = Mat4f::rotate_axis({0.0f, 0.0f, 1.0f}, 3.14159265f / 2.0f) * Vec3f{1.0f, 0.0f, 0.0f};
		failures += check(std::abs(rot.x) < 1e-3f, "rotation maps x to y");
		failures += check(std::abs(rot.y - 1.0f) < 1e-3f, "rotation maps x to y value");
	}
	{
		const auto inv = Mat4f::translate({1.0f, 2.0f, 3.0f}).inverted();
		const auto back = inv * Vec3f{1.0f, 2.0f, 3.0f};
		failures += check(std::abs(back.x) < 1e-3f, "matrix inverse undoes translation");
		failures += check(std::abs(back.y) < 1e-3f, "matrix inverse undoes translation y");
		failures += check(std::abs(back.z) < 1e-3f, "matrix inverse undoes translation z");
	}

	// 2. Mesh adapter produces triangles from a canonical cube.
	dfae::live::LiveCommandReactor reactor;
	auto ok = reactor.execute("create cube 2");
	if (std::holds_alternative<dfae::live::CommandError>(ok)) {
		std::cerr << "reactor create failed\n";
		return 1;
	}
	const auto mesh = Mesh::from_canonical_mesh(reactor.state().published_mesh);
	failures += check(!mesh.empty(), "adapted mesh is not empty");
	failures += check(mesh.triangle_count() > 0, "adapted mesh has triangles");

	// 3. Software rasterizer renders a frame and writes a PPM.
	SoftwareRasterizer rasterizer;
	rasterizer.resize(128, 128);

	RenderParams params;
	params.world = Mat4f::rotate_axis({0.0f, 1.0f, 0.0f}, 0.3f) * Mat4f::translate({0.0f, 0.0f, -4.0f});
	params.view = Mat4f::look_at({0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	params.projection = Mat4f::perspective(60.0f * 3.14159265f / 180.0f, 1.0f, 0.1f, 100.0f);
	params.viewport = {0, 0, 128, 128};
	params.base_color = Color::from_float(0.6f, 0.75f, 0.9f);
	params.light_direction = {0.0f, 0.0f, 1.0f};

	rasterizer.clear(Color::from_float(0.05f, 0.07f, 0.09f));
	rasterizer.render_mesh(mesh, params);

	const auto ppm = rasterizer.to_ppm();
	failures += check(!ppm.empty(), "PPM output is not empty");
	failures += check(ppm.size() > 16, "PPM has header and pixels");

	const std::string path = "renderer3d_test_output.ppm";
	{
		std::ofstream ofs(path, std::ios::binary);
		failures += check(ofs.is_open(), "can open PPM output file");
		ofs.write(reinterpret_cast<const char*>(ppm.data()), static_cast<std::ptrdiff_t>(ppm.size()));
	}

	// 4. Wireframe mode also produces output.
	rasterizer.clear(Color::from_float(0.05f, 0.07f, 0.09f));
	params.wireframe = true;
	rasterizer.render_mesh(mesh, params);
	const auto wire_ppm = rasterizer.to_ppm();
	failures += check(!wire_ppm.empty(), "wireframe PPM is not empty");

	std::remove(path.c_str());

	// 5. PNG render verification.
	{
		rasterizer.clear(Color::from_float(0.05f, 0.07f, 0.09f));
		params.wireframe = false;
		rasterizer.render_mesh(mesh, params);
		const auto png = rasterizer.to_png();
		failures += check(!png.empty(), "PNG output is not empty");
		failures += check(png.size() > 33, "PNG has signature and chunks");
		// PNG signature bytes.
		const std::uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
		failures += check(std::memcmp(png.data(), sig, 8) == 0, "PNG has correct signature");

		const std::string png_path = "renderer3d_test_output.png";
		{
			std::ofstream ofs(png_path, std::ios::binary);
			failures += check(ofs.is_open(), "can open PNG output file");
			ofs.write(reinterpret_cast<const char*>(png.data()), static_cast<std::ptrdiff_t>(png.size()));
		}
		std::remove(png_path.c_str());
	}

	// 6. Procedural primitives: icosphere and cylinder are non-empty and have
	// vertex normals consistent with the surface (normal not zero).
	{
		const auto sphere = PrimitiveFactory::icosphere(1.0f, 2);
		failures += check(sphere.triangle_count() == 80, "icosphere subdiv 2 has 80 triangles");
		failures += check(!sphere.vertices.empty(), "icosphere has vertices");
		const float nlen = sphere.vertices.front().normal.norm();
		failures += check(std::abs(nlen - 1.0f) < 1e-3f, "icosphere normal is unit length");
	}
	{
		const auto cyl = PrimitiveFactory::cylinder(1.0f, 2.0f, 16);
		failures += check(cyl.triangle_count() > 0, "cylinder has triangles");
		failures += check(!cyl.vertices.empty(), "cylinder has vertices");
		const float nlen = cyl.vertices.front().normal.norm();
		failures += check(std::abs(nlen - 1.0f) < 1e-3f, "cylinder normal is unit length");

		// Render the cylinder to make sure the rasterizer accepts the welded mesh.
		rasterizer.clear(Color::from_float(0.05f, 0.07f, 0.09f));
		rasterizer.render_mesh(cyl, params);
		failures += check(!rasterizer.to_ppm().empty(), "cylinder renders to PPM");
	}

	// 7. Empty render is safe.
	Mesh empty_mesh;
	rasterizer.clear();
	rasterizer.render_mesh(empty_mesh, params);
	failures += check(rasterizer.framebuffer().valid(), "empty render leaves framebuffer valid");

	// 8. The display adapter accepts the same canonical mesh published from XYZ.
	{
		const std::string xyz_path = "renderer3d_unified_test.xyz";
		{
			std::ofstream xyz(xyz_path);
			xyz << "1\ncarbon\nC 0 0 0\n";
		}
		auto loaded = reactor.execute("load " + xyz_path);
		failures += check(std::holds_alternative<dfae::live::CommandOk>(loaded), "renderer reactor loads XYZ");
		const auto xyz_mesh = Mesh::from_canonical_mesh(reactor.state().published_mesh);
		failures += check(xyz_mesh.triangle_count() == reactor.state().published_mesh.triangle_count(),
			"renderer adapter preserves published canonical triangle count");
		std::remove(xyz_path.c_str());
	}

	if (failures != 0) {
		std::cerr << failures << " check(s) failed.\n";
		return 1;
	}

	std::cout << "PASS: homegrown 3D renderer end-to-end\n";
	return 0;
}
