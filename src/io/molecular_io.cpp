/**
 * Unified Molecular File System — Implementation
 *
 * Single-point implementation for all molecular structure I/O.
 * Wraps XYZReader / XYZWriter / XYZAReader / XYZAWriter /
 * XYZTrajectory and exposes a clean, validated, format-agnostic API.
 *
 * All I/O goes through this module in the unified layer.
 */

#include "io/molecular_io.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace vsepr {
namespace io {

// ============================================================================
// Internal helpers
// ============================================================================

static std::string to_lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

static std::string extension_of(const std::string& filename) {
    const size_t dot = filename.rfind('.');
    if (dot == std::string::npos) return "";
    return to_lower_str(filename.substr(dot));
}

// ============================================================================
// Validation
// ============================================================================

std::string ValidationReport::summary() const {
    if (severity == ValidationSeverity::OK) return "OK";
    std::string s;
    for (const auto& w : warnings) s += "WARNING: " + w + "\n";
    for (const auto& e : errors)   s += "ERROR: "   + e + "\n";
    return s;
}

ValidationReport validate_structure(const XYZMolecule& mol) {
    ValidationReport report;

    if (mol.atoms.empty()) {
        report.severity = ValidationSeverity::FATAL;
        report.errors.push_back("Molecule has no atoms");
        return report;
    }

    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        const auto& atom = mol.atoms[i];

        if (atom.element.empty()) {
            report.errors.push_back(
                "Atom " + std::to_string(i) + " has empty element symbol");
            if (report.severity < ValidationSeverity::ERROR)
                report.severity = ValidationSeverity::ERROR;
        }

        const auto& p = atom.position;
        if (std::isnan(p[0]) || std::isnan(p[1]) || std::isnan(p[2]) ||
            std::isinf(p[0]) || std::isinf(p[1]) || std::isinf(p[2])) {
            report.warnings.push_back(
                "Atom " + std::to_string(i) + " (" + atom.element +
                ") has invalid coordinates (NaN or Inf)");
            if (report.severity < ValidationSeverity::WARNING)
                report.severity = ValidationSeverity::WARNING;
        }
    }

    // Check atoms not unreasonably close (skip NaN atoms already warned above)
    constexpr double MIN_DIST = 0.5;
    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        const auto& pi = mol.atoms[i].position;
        if (std::isnan(pi[0])) continue;
        for (size_t j = i + 1; j < mol.atoms.size(); ++j) {
            const auto& pj = mol.atoms[j].position;
            if (std::isnan(pj[0])) continue;
            const double dx = pi[0] - pj[0];
            const double dy = pi[1] - pj[1];
            const double dz = pi[2] - pj[2];
            const double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist < MIN_DIST) {
                report.errors.push_back(
                    "Atoms " + std::to_string(i) + " and " + std::to_string(j) +
                    " are too close (" + std::to_string(dist) + " \xC3\x85)");
                if (report.severity < ValidationSeverity::ERROR)
                    report.severity = ValidationSeverity::ERROR;
            }
        }
    }

    return report;
}

// ============================================================================
// Format Detection
// ============================================================================

MolecularFormat detect_format(const std::string& filename) {
    const std::string ext = extension_of(filename);
    if (ext == ".xyz")  return MolecularFormat::XYZ;
    if (ext == ".xyza") return MolecularFormat::XYZA;
    if (ext == ".xyzc") return MolecularFormat::XYZC;

    // Probe file header for .xyza content signature
    std::ifstream probe(filename);
    if (probe.is_open()) {
        std::string line1, line2;
        std::getline(probe, line1); // atom count
        std::getline(probe, line2); // comment
        if (line2.find("properties=") != std::string::npos)
            return MolecularFormat::XYZA;
        if (line2.find("CHECKPOINT") != std::string::npos)
            return MolecularFormat::XYZC;
    }

    return MolecularFormat::UNKNOWN;
}

FormatInfo get_format_info(MolecularFormat format) {
    switch (format) {
        case MolecularFormat::XYZ:
            return {"Standard XYZ", ".xyz", "Static molecular geometry",
                    false, false, false};
        case MolecularFormat::XYZA:
            return {"Extended XYZ", ".xyza",
                    "Molecular geometry with properties (charges, velocities, forces)",
                    true, true, false};
        case MolecularFormat::XYZC:
            return {"Checkpointed XYZ", ".xyzc",
                    "Binary MD checkpoint with full phase-space state",
                    true, true, true};
        default:
            return {"Unknown", "", "", false, false, false};
    }
}

// ============================================================================
// Error helpers
// ============================================================================

Status make_io_error(ErrorCode code, const std::string& message,
                     const std::string& filename) {
    return Status::error(code, message, filename);
}

// ============================================================================
// Single-structure load
// ============================================================================

Result<XYZMolecule> load_structure(const std::string& filename,
                                    const IOOptions& opts) {
    // Existence check
    std::ifstream test(filename);
    if (!test.is_open()) {
        return Result<XYZMolecule>::error(
            ErrorCode::FILE_NOT_FOUND, "Cannot open file", filename);
    }
    test.close();

    const MolecularFormat fmt =
        opts.auto_detect_format ? detect_format(filename) : MolecularFormat::XYZ;

    XYZMolecule mol;

    if (fmt == MolecularFormat::XYZA) {
        XYZAReader reader;
        if (!reader.read(filename, mol)) {
            return Result<XYZMolecule>::error(
                ErrorCode::FILE_CORRUPTED, "Parse error: " + reader.get_error(),
                filename);
        }
    } else if (fmt == MolecularFormat::XYZ || fmt == MolecularFormat::UNKNOWN) {
        XYZReader reader;
        if (!reader.read(filename, mol)) {
            return Result<XYZMolecule>::error(
                ErrorCode::FILE_CORRUPTED, "Parse error: " + reader.get_error(),
                filename);
        }
        if (opts.detect_bonds && mol.bonds.empty()) {
            reader.detect_bonds(mol, opts.bond_scale);
        }
    } else {
        return Result<XYZMolecule>::error(
            ErrorCode::FILE_INVALID_FORMAT,
            "Use load_trajectory() for .xyzc files", filename);
    }

    if (opts.comment.has_value()) {
        mol.comment = opts.comment.value();
    }

    if (opts.validate_coords) {
        const auto report = validate_structure(mol);
        if (!report.passed()) {
            return Result<XYZMolecule>::error(
                ErrorCode::CHEMISTRY_UNREASONABLE_GEOMETRY,
                report.summary(), filename);
        }
    }

    return Result<XYZMolecule>::ok(std::move(mol));
}

// ============================================================================
// Single-structure save
// ============================================================================

Status save_structure(const std::string& filename, const XYZMolecule& mol,
                      const IOOptions& opts) {
    // Determine what to write — optionally filter invalid atoms
    XYZMolecule to_write;
    to_write.comment  = opts.comment.value_or(mol.comment);
    to_write.bonds    = mol.bonds;
    to_write.total_energy = mol.total_energy;
    to_write.total_charge = mol.total_charge;
    to_write.formula       = mol.formula;

    if (opts.skip_invalid_atoms) {
        to_write.atoms.reserve(mol.atoms.size());
        for (const auto& atom : mol.atoms) {
            const auto& p = atom.position;
            if (!std::isnan(p[0]) && !std::isnan(p[1]) && !std::isnan(p[2]) &&
                !std::isinf(p[0]) && !std::isinf(p[1]) && !std::isinf(p[2])) {
                to_write.atoms.push_back(atom);
            }
        }
    } else {
        to_write.atoms = mol.atoms;
    }

    // Optional coordinate validation (strict — fails on bad coords)
    if (opts.validate_coords) {
        const auto report = validate_structure(to_write);
        if (!report.passed()) {
            return Status::error(
                ErrorCode::CHEMISTRY_UNREASONABLE_GEOMETRY,
                report.summary(), filename);
        }
    }

    if (to_write.atoms.empty()) {
        return Status::error(ErrorCode::INVALID_ARGUMENT,
                             "No valid atoms to write", filename);
    }

    const MolecularFormat fmt =
        opts.auto_detect_format ? detect_format(filename) : MolecularFormat::XYZ;

    if (fmt == MolecularFormat::XYZA) {
        XYZAWriter writer;
        writer.set_precision(opts.precision);
        if (!writer.write(filename, to_write)) {
            return Status::error(ErrorCode::FILE_WRITE_FAILED,
                                 "Write failed: " + writer.get_error(), filename);
        }
    } else {
        // Default to standard XYZ for .xyz and UNKNOWN extensions
        XYZWriter writer;
        writer.set_precision(opts.precision);
        if (!writer.write(filename, to_write)) {
            return Status::error(ErrorCode::FILE_WRITE_FAILED,
                                 "Write failed: " + writer.get_error(), filename);
        }
    }

    return Status::ok();
}

// ============================================================================
// Trajectory load (in-memory)
// ============================================================================

Result<XYZTrajectory> load_trajectory(const std::string& filename,
                                       const IOOptions& /*opts*/) {
    std::ifstream test(filename);
    if (!test.is_open()) {
        return Result<XYZTrajectory>::error(
            ErrorCode::FILE_NOT_FOUND, "Cannot open file", filename);
    }
    test.close();

    XYZTrajectory traj;
    if (!traj.read(filename)) {
        return Result<XYZTrajectory>::error(
            ErrorCode::FILE_CORRUPTED, "Failed to read trajectory", filename);
    }

    return Result<XYZTrajectory>::ok(std::move(traj));
}

// ============================================================================
// Trajectory save (in-memory)
// ============================================================================

Status save_trajectory(const std::string& filename, const XYZTrajectory& traj,
                        const IOOptions& opts) {
    if (traj.num_frames() == 0) {
        return Status::error(ErrorCode::INVALID_ARGUMENT,
                             "Cannot save empty trajectory");
    }

    // Write frame-by-frame to the file
    std::ofstream file(filename);
    if (!file.is_open()) {
        return Status::error(ErrorCode::FILE_WRITE_FAILED,
                             "Cannot open file for writing", filename);
    }

    XYZWriter writer;
    writer.set_precision(opts.precision);

    for (size_t i = 0; i < traj.num_frames(); ++i) {
        if (!writer.write_stream(file, traj.get_frame(i))) {
            return Status::error(ErrorCode::FILE_WRITE_FAILED,
                                 "Failed writing frame " + std::to_string(i),
                                 filename);
        }
    }

    return Status::ok();
}

// ============================================================================
// TrajectoryWriter — streaming, frame-by-frame
// ============================================================================

struct TrajectoryWriter::Impl {
    std::ofstream file;
    XYZWriter     writer;
    IOOptions     opts;
    size_t        frames_written = 0;
    bool          finalized = false;

    explicit Impl(const std::string& filename, const IOOptions& o)
        : opts(o)
    {
        file.open(filename);
        writer.set_precision(o.precision);
    }

    bool is_open() const { return file.is_open() && !finalized; }
};

TrajectoryWriter::TrajectoryWriter(const std::string& filename,
                                   MolecularFormat /*format*/,
                                   const IOOptions& opts)
    : impl_(std::make_unique<Impl>(filename, opts))
{}

TrajectoryWriter::~TrajectoryWriter() {
    if (impl_ && impl_->is_open()) {
        impl_->finalized = true;
    }
}

TrajectoryWriter::TrajectoryWriter(TrajectoryWriter&&) noexcept = default;
TrajectoryWriter& TrajectoryWriter::operator=(TrajectoryWriter&&) noexcept = default;

Status TrajectoryWriter::append_frame(const XYZMolecule& mol, double /*time*/) {
    if (!impl_ || !impl_->is_open()) {
        return Status::error(ErrorCode::FILE_WRITE_FAILED,
                             "Trajectory writer is not open or already finalized");
    }

    // Optional NaN filtering per frame
    if (impl_->opts.skip_invalid_atoms) {
        XYZMolecule filtered;
        filtered.comment = mol.comment;
        for (const auto& atom : mol.atoms) {
            const auto& p = atom.position;
            if (!std::isnan(p[0]) && !std::isnan(p[1]) && !std::isnan(p[2]) &&
                !std::isinf(p[0]) && !std::isinf(p[1]) && !std::isinf(p[2])) {
                filtered.atoms.push_back(atom);
            }
        }
        if (!impl_->writer.write_stream(impl_->file, filtered)) {
            return Status::error(ErrorCode::FILE_WRITE_FAILED,
                                 "Failed writing trajectory frame");
        }
    } else {
        if (!impl_->writer.write_stream(impl_->file, mol)) {
            return Status::error(ErrorCode::FILE_WRITE_FAILED,
                                 "Failed writing trajectory frame");
        }
    }

    ++impl_->frames_written;
    return Status::ok();
}

Status TrajectoryWriter::finalize() {
    if (!impl_) {
        return Status::error(ErrorCode::INVALID_ARGUMENT, "Writer not initialized");
    }
    impl_->finalized = true;
    if (impl_->file.is_open()) impl_->file.close();
    return Status::ok();
}

size_t TrajectoryWriter::num_frames_written() const {
    return impl_ ? impl_->frames_written : 0;
}

bool TrajectoryWriter::is_open() const {
    return impl_ && impl_->is_open();
}

// ============================================================================
// TrajectoryReader — streaming, frame-by-frame
// ============================================================================

struct TrajectoryReader::Impl {
    std::ifstream file;
    XYZReader     reader;
    IOOptions     opts;
    size_t        current_frame = 0;
    bool          at_eof = false;

    explicit Impl(const std::string& filename, const IOOptions& o)
        : opts(o)
    {
        file.open(filename);
    }

    bool is_open() const { return file.is_open() && !at_eof; }
};

TrajectoryReader::TrajectoryReader(const std::string& filename,
                                   const IOOptions& opts)
    : impl_(std::make_unique<Impl>(filename, opts))
{}

TrajectoryReader::~TrajectoryReader() = default;
TrajectoryReader::TrajectoryReader(TrajectoryReader&&) noexcept = default;
TrajectoryReader& TrajectoryReader::operator=(TrajectoryReader&&) noexcept = default;

Result<XYZMolecule> TrajectoryReader::read_frame() {
    if (!impl_ || !impl_->file.is_open()) {
        return Result<XYZMolecule>::error(ErrorCode::FILE_NOT_FOUND,
                                          "Trajectory file not open");
    }
    if (impl_->at_eof) {
        return Result<XYZMolecule>::error(ErrorCode::PARSE_UNEXPECTED_EOF,
                                          "No more frames");
    }

    XYZMolecule mol;
    if (!impl_->reader.read_stream(impl_->file, mol)) {
        impl_->at_eof = true;
        return Result<XYZMolecule>::error(ErrorCode::PARSE_UNEXPECTED_EOF,
                                          "No more frames");
    }

    if (impl_->opts.detect_bonds && mol.bonds.empty()) {
        impl_->reader.detect_bonds(mol, impl_->opts.bond_scale);
    }

    ++impl_->current_frame;
    return Result<XYZMolecule>::ok(std::move(mol));
}

bool TrajectoryReader::has_more_frames() const {
    return impl_ && impl_->file.is_open() && !impl_->at_eof &&
           impl_->file.peek() != std::ifstream::traits_type::eof();
}

size_t TrajectoryReader::num_frames() const {
    return 0; // Unknown until fully read
}

size_t TrajectoryReader::current_frame() const {
    return impl_ ? impl_->current_frame : 0;
}

Status TrajectoryReader::reset() {
    if (!impl_ || !impl_->file.is_open()) {
        return Status::error(ErrorCode::FILE_NOT_FOUND,
                             "Trajectory file not open");
    }
    impl_->file.seekg(0, std::ios::beg);
    impl_->current_frame = 0;
    impl_->at_eof = false;
    return Status::ok();
}

// ============================================================================
// Format conversion
// ============================================================================

Status convert_file(const std::string& input_path,
                    const std::string& output_path,
                    const IOOptions& opts) {
    auto load_result = load_structure(input_path, opts);
    if (!load_result.is_ok()) {
        return Status::error(load_result.error());
    }
    return save_structure(output_path, load_result.value(), opts);
}

// ============================================================================
// Batch operations
// ============================================================================

Result<std::vector<XYZMolecule>> load_directory(const std::string& dir_path,
                                                  const std::string& pattern,
                                                  const IOOptions& opts) {
    std::vector<XYZMolecule> results;

    // Determine extension from pattern (e.g. "*.xyz" → ".xyz")
    std::string ext_filter;
    const size_t star = pattern.rfind('*');
    if (star != std::string::npos) {
        ext_filter = to_lower_str(pattern.substr(star + 1));
    }

    std::error_code ec;
    for (const auto& entry :
         std::filesystem::directory_iterator(dir_path, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string fname = entry.path().string();
        if (!ext_filter.empty() && extension_of(fname) != ext_filter) continue;

        auto result = load_structure(fname, opts);
        if (result.is_ok()) {
            results.push_back(std::move(result.value()));
        }
    }

    if (ec) {
        return Result<std::vector<XYZMolecule>>::error(
            ErrorCode::FILE_NOT_FOUND, "Cannot iterate directory", dir_path);
    }

    return Result<std::vector<XYZMolecule>>::ok(std::move(results));
}

Status save_batch(const std::string& basename,
                  const std::vector<XYZMolecule>& molecules,
                  const IOOptions& opts) {
    const MolecularFormat fmt =
        opts.auto_detect_format ? detect_format(basename + ".xyz")
                                : MolecularFormat::XYZ;
    const std::string ext =
        (fmt == MolecularFormat::XYZA) ? ".xyza" : ".xyz";

    for (size_t i = 0; i < molecules.size(); ++i) {
        const std::string path =
            basename + "_" + std::to_string(i) + ext;
        const auto s = save_structure(path, molecules[i], opts);
        if (s.is_error()) return s;
    }

    return Status::ok();
}

}} // namespace vsepr::io
