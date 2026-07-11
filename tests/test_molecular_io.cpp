/**
 * test_molecular_io.cpp
 * ---------------------
 * Comprehensive test suite for the unified molecular I/O API.
 * Covers single-structure load/save, trajectory streaming,
 * format detection, validation, conversion, and batch operations.
 */

#include "io/molecular_io.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

// ============================================================================
// Minimal test harness
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    static void name(); \
    struct _Reg_##name { _Reg_##name() { \
        try { \
            name(); \
            std::cout << "  PASS  " #name "\n"; \
            ++g_passed; \
        } catch (const std::exception& ex) { \
            std::cout << "  FAIL  " #name " — " << ex.what() << "\n"; \
            ++g_failed; \
        } \
    }} _reg_##name; \
    static void name()

#define ASSERT(cond) \
    do { if (!(cond)) throw std::runtime_error("assertion failed: " #cond); } while (0)

#define ASSERT_EQ(a, b) \
    do { if (!((a) == (b))) throw std::runtime_error( \
        std::string("expected equal: " #a " == " #b)); } while (0)

// ============================================================================
// Temporary-file RAII helper
// ============================================================================

struct TmpFile {
    std::string path;
    explicit TmpFile(const std::string& suffix) {
        path = std::filesystem::temp_directory_path().string() +
               "/vsepr_io_test_" + std::to_string(std::rand()) + suffix;
    }
    ~TmpFile() { std::remove(path.c_str()); }
};

// ============================================================================
// Reference molecules
// ============================================================================

static vsepr::io::XYZMolecule make_water() {
    vsepr::io::XYZMolecule m;
    m.comment = "Water H2O";
    m.atoms.emplace_back("O",  0.000000,  0.000000,  0.119262);
    m.atoms.emplace_back("H",  0.000000,  0.763239, -0.477049);
    m.atoms.emplace_back("H",  0.000000, -0.763239, -0.477049);
    return m;
}

static vsepr::io::XYZMolecule make_methane() {
    vsepr::io::XYZMolecule m;
    m.comment = "Methane CH4";
    m.atoms.emplace_back("C",  0.000000,  0.000000,  0.000000);
    m.atoms.emplace_back("H",  0.629118,  0.629118,  0.629118);
    m.atoms.emplace_back("H", -0.629118, -0.629118,  0.629118);
    m.atoms.emplace_back("H", -0.629118,  0.629118, -0.629118);
    m.atoms.emplace_back("H",  0.629118, -0.629118, -0.629118);
    return m;
}

// ============================================================================
// Week 1 — Core API
// ============================================================================

TEST(detect_format_xyz) {
    ASSERT(vsepr::io::detect_format("water.xyz")  == vsepr::io::MolecularFormat::XYZ);
    ASSERT(vsepr::io::detect_format("traj.xyza")  == vsepr::io::MolecularFormat::XYZA);
    ASSERT(vsepr::io::detect_format("md.xyzc")    == vsepr::io::MolecularFormat::XYZC);
    ASSERT(vsepr::io::detect_format("unknown.dat") == vsepr::io::MolecularFormat::UNKNOWN);
}

TEST(detect_format_case_insensitive) {
    ASSERT(vsepr::io::detect_format("Water.XYZ")  == vsepr::io::MolecularFormat::XYZ);
    ASSERT(vsepr::io::detect_format("Traj.XYZA")  == vsepr::io::MolecularFormat::XYZA);
}

TEST(save_and_load_xyz) {
    TmpFile f(".xyz");
    auto mol = make_water();

    auto save_status = vsepr::io::save_structure(f.path, mol);
    ASSERT(save_status.is_ok());

    auto load_result = vsepr::io::load_structure(f.path);
    ASSERT(load_result.is_ok());

    const auto& loaded = load_result.value();
    ASSERT_EQ(loaded.atoms.size(), mol.atoms.size());
    ASSERT_EQ(loaded.atoms[0].element, "O");
    ASSERT_EQ(loaded.atoms[1].element, "H");
    ASSERT(std::fabs(loaded.atoms[0].position[2] - 0.119262) < 1e-5);
}

TEST(save_preserves_comment) {
    TmpFile f(".xyz");
    auto mol = make_water();
    mol.comment = "Test comment line";

    vsepr::io::save_structure(f.path, mol);
    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().comment, "Test comment line");
}

TEST(comment_override_via_options) {
    TmpFile f(".xyz");
    auto mol = make_water();
    mol.comment = "Original";

    vsepr::io::IOOptions opts;
    opts.comment = "Overridden";
    vsepr::io::save_structure(f.path, mol, opts);

    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().comment, "Overridden");
}

TEST(load_nonexistent_file_returns_error) {
    auto result = vsepr::io::load_structure("/tmp/vsepr_io_does_not_exist_xyz_xyz.xyz");
    ASSERT(result.is_error());
    ASSERT(result.error().code == vsepr::ErrorCode::FILE_NOT_FOUND);
}

TEST(save_methane_roundtrip) {
    TmpFile f(".xyz");
    auto mol = make_methane();

    ASSERT(vsepr::io::save_structure(f.path, mol).is_ok());

    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().atoms.size(), 5u);
    ASSERT_EQ(result.value().atoms[0].element, "C");
}

TEST(precision_option_respected) {
    TmpFile f(".xyz");
    auto mol = make_water();

    vsepr::io::IOOptions opts;
    opts.precision = 3;
    vsepr::io::save_structure(f.path, mol, opts);

    // Re-load and check round-trip within precision
    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT(std::fabs(result.value().atoms[0].position[2] - 0.119) < 1e-2);
}

// ============================================================================
// Validation
// ============================================================================

TEST(validate_clean_molecule_is_ok) {
    const auto report = vsepr::io::validate_structure(make_water());
    ASSERT(report.passed());
    ASSERT(report.severity == vsepr::io::ValidationSeverity::OK);
}

TEST(validate_nan_coord_is_warning) {
    auto mol = make_water();
    mol.atoms[1].position[0] = std::numeric_limits<double>::quiet_NaN();
    const auto report = vsepr::io::validate_structure(mol);
    ASSERT(!report.errors.empty() || !report.warnings.empty());
}

TEST(validate_empty_molecule_is_fatal) {
    vsepr::io::XYZMolecule empty;
    const auto report = vsepr::io::validate_structure(empty);
    ASSERT(!report.passed());
    ASSERT(report.severity == vsepr::io::ValidationSeverity::FATAL);
}

TEST(skip_invalid_atoms_filters_nan) {
    TmpFile f(".xyz");
    auto mol = make_water();
    mol.atoms[2].position[0] = std::numeric_limits<double>::quiet_NaN();

    vsepr::io::IOOptions opts;
    opts.skip_invalid_atoms = true;
    ASSERT(vsepr::io::save_structure(f.path, mol, opts).is_ok());

    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().atoms.size(), 2u); // NaN atom dropped
}

TEST(save_all_nan_returns_error) {
    TmpFile f(".xyz");
    vsepr::io::XYZMolecule mol;
    mol.comment = "All NaN";
    const double nan = std::numeric_limits<double>::quiet_NaN();
    mol.atoms.emplace_back("O", nan, nan, nan);

    vsepr::io::IOOptions opts;
    opts.skip_invalid_atoms = true;
    const auto status = vsepr::io::save_structure(f.path, mol, opts);
    ASSERT(status.is_error()); // Nothing left to write
}

// ============================================================================
// XYZA format
// ============================================================================

TEST(save_and_load_xyza) {
    TmpFile f(".xyza");
    auto mol = make_water();

    ASSERT(vsepr::io::save_structure(f.path, mol).is_ok());

    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().atoms.size(), 3u);
}

// ============================================================================
// TrajectoryWriter / TrajectoryReader
// ============================================================================

TEST(trajectory_writer_creates_multiframe_file) {
    TmpFile f(".xyza");
    {
        vsepr::io::TrajectoryWriter writer(f.path);
        for (int i = 0; i < 5; ++i) {
            auto frame = make_water();
            frame.comment = "Frame " + std::to_string(i);
            ASSERT(writer.append_frame(frame, i * 0.5).is_ok());
        }
        ASSERT(writer.finalize().is_ok());
        ASSERT_EQ(writer.num_frames_written(), 5u);
    }

    vsepr::io::TrajectoryReader reader(f.path);
    int count = 0;
    while (reader.has_more_frames()) {
        auto result = reader.read_frame();
        ASSERT(result.is_ok());
        ASSERT_EQ(result.value().atoms.size(), 3u);
        ++count;
    }
    ASSERT_EQ(count, 5);
}

TEST(trajectory_reader_reset) {
    TmpFile f(".xyza");
    {
        vsepr::io::TrajectoryWriter writer(f.path);
        for (int i = 0; i < 3; ++i) {
            auto frame = make_methane();
            ASSERT(writer.append_frame(frame).is_ok());
        }
        writer.finalize();
    }

    vsepr::io::TrajectoryReader reader(f.path);
    int count1 = 0;
    while (reader.has_more_frames()) { reader.read_frame(); ++count1; }
    ASSERT_EQ(count1, 3);

    reader.reset();
    int count2 = 0;
    while (reader.has_more_frames()) { reader.read_frame(); ++count2; }
    ASSERT_EQ(count2, 3);
}

TEST(trajectory_writer_is_open) {
    TmpFile f(".xyz");
    vsepr::io::TrajectoryWriter writer(f.path);
    ASSERT(writer.is_open());
    writer.finalize();
    ASSERT(!writer.is_open());
}

// ============================================================================
// In-memory trajectory
// ============================================================================

TEST(save_and_load_trajectory_in_memory) {
    TmpFile f(".xyza");

    vsepr::io::XYZTrajectory traj;
    traj.add_frame(make_water(), 0.0);
    traj.add_frame(make_water(), 1.0);
    traj.add_frame(make_water(), 2.0);

    ASSERT(vsepr::io::save_trajectory(f.path, traj).is_ok());

    auto result = vsepr::io::load_trajectory(f.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().num_frames(), 3u);
}

TEST(save_empty_trajectory_returns_error) {
    TmpFile f(".xyza");
    vsepr::io::XYZTrajectory empty;
    ASSERT(vsepr::io::save_trajectory(f.path, empty).is_error());
}

// ============================================================================
// Format conversion
// ============================================================================

TEST(convert_xyz_to_xyza) {
    TmpFile src(".xyz"), dst(".xyza");
    vsepr::io::save_structure(src.path, make_water());

    ASSERT(vsepr::io::convert_file(src.path, dst.path).is_ok());

    auto result = vsepr::io::load_structure(dst.path);
    ASSERT(result.is_ok());
    ASSERT_EQ(result.value().atoms.size(), 3u);
}

TEST(convert_missing_input_returns_error) {
    TmpFile dst(".xyz");
    auto status = vsepr::io::convert_file("/no/such/file.xyz", dst.path);
    ASSERT(status.is_error());
}

// ============================================================================
// Batch operations
// ============================================================================

TEST(save_batch_produces_indexed_files) {
    const std::string base =
        std::filesystem::temp_directory_path().string() +
        "/vsepr_batch_test_" + std::to_string(std::rand());

    std::vector<vsepr::io::XYZMolecule> mols = {make_water(), make_methane()};
    ASSERT(vsepr::io::save_batch(base, mols).is_ok());

    ASSERT(std::filesystem::exists(base + "_0.xyz"));
    ASSERT(std::filesystem::exists(base + "_1.xyz"));

    std::remove((base + "_0.xyz").c_str());
    std::remove((base + "_1.xyz").c_str());
}

TEST(save_batch_empty_is_ok) {
    const std::string base =
        std::filesystem::temp_directory_path().string() +
        "/vsepr_batch_empty_" + std::to_string(std::rand());
    std::vector<vsepr::io::XYZMolecule> empty;
    ASSERT(vsepr::io::save_batch(base, empty).is_ok());
}

// ============================================================================
// get_format_info
// ============================================================================

TEST(format_info_xyz) {
    auto info = vsepr::io::get_format_info(vsepr::io::MolecularFormat::XYZ);
    ASSERT(info.name == "Standard XYZ");
    ASSERT(!info.is_binary);
    ASSERT(!info.supports_trajectories);
}

TEST(format_info_xyza) {
    auto info = vsepr::io::get_format_info(vsepr::io::MolecularFormat::XYZA);
    ASSERT(info.supports_trajectories);
    ASSERT(info.supports_properties);
}

TEST(format_info_xyzc) {
    auto info = vsepr::io::get_format_info(vsepr::io::MolecularFormat::XYZC);
    ASSERT(info.is_binary);
}

// ============================================================================
// make_io_error
// ============================================================================

TEST(make_io_error_builds_status) {
    auto s = vsepr::io::make_io_error(
        vsepr::ErrorCode::FILE_NOT_FOUND, "test message", "file.xyz");
    ASSERT(s.is_error());
    ASSERT(s.error().code == vsepr::ErrorCode::FILE_NOT_FOUND);
}

// ============================================================================
// Coordinate precision round-trip
// ============================================================================

TEST(high_precision_roundtrip) {
    TmpFile f(".xyz");
    vsepr::io::XYZMolecule mol;
    mol.comment = "Precision test";
    mol.atoms.emplace_back("C", 1.234567, -2.345678, 3.456789);

    vsepr::io::IOOptions opts;
    opts.precision = 6;
    vsepr::io::save_structure(f.path, mol, opts);

    auto result = vsepr::io::load_structure(f.path);
    ASSERT(result.is_ok());
    ASSERT(std::fabs(result.value().atoms[0].position[0] - 1.234567) < 1e-5);
    ASSERT(std::fabs(result.value().atoms[0].position[1] - (-2.345678)) < 1e-5);
    ASSERT(std::fabs(result.value().atoms[0].position[2] - 3.456789) < 1e-5);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== Molecular I/O Unit Tests ===\n\n";
    // All TEST() objects self-register and run at static-init time.
    std::cout << "\nResults: " << g_passed << " passed, "
              << g_failed << " failed\n";
    return g_failed > 0 ? 1 : 0;
}
