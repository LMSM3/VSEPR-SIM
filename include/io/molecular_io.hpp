/**
 * Unified Molecular File System API
 *
 * Single entry point for all molecular structure I/O operations.
 * Replaces scattered XYZReader/XYZWriter usage with consistent,
 * validated, and future-proof interface.
 *
 * Design Principles:
 * - Result<T> for value-returning operations; Status for void operations
 * - Automatic format detection from file extension
 * - Centralized coordinate validation (NaN/Inf, unreasonable distances)
 * - Stream-based APIs for large trajectories (frame-by-frame, no full load)
 * - Metadata preservation for reproducibility
 *
 * Replaces ~435 lines of scattered I/O code with ~75 lines of unified API calls.
 */

#pragma once

#include "io/xyz_format.hpp"
#include "core/error.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace vsepr {
namespace io {

// ============================================================================
// Configuration & Options
// ============================================================================

/**
 * Options for file I/O operations
 */
struct IOOptions {
    // Validation
    bool validate_coords = false;    ///< Check for NaN, Inf, unreasonable distances (save: default on)
    bool skip_invalid_atoms = false; ///< Skip atoms with invalid coords instead of failing

    // Bond handling
    bool detect_bonds = true;        ///< Auto-detect bonds from covalent radii
    double bond_scale = 1.2;         ///< Scale factor for covalent radius sum

    // Format
    bool auto_detect_format = true;  ///< Detect format from extension
    int precision = 6;               ///< Decimal places for coordinates

    // Metadata
    std::optional<std::string> comment; ///< Override comment line
    bool include_metadata = true;       ///< Include generation metadata

    // Error handling
    bool verbose_errors = false;     ///< Include detailed error context
};

/**
 * File format types
 */
enum class MolecularFormat {
    XYZ,         ///< Standard XYZ (positions only)
    XYZA,        ///< Extended XYZ (positions + properties)
    XYZC,        ///< Binary checkpointed MD trajectories
    AUTO,        ///< Auto-detect from filename
    UNKNOWN
};

/**
 * Validation severity levels
 */
enum class ValidationSeverity {
    OK,          ///< No issues
    WARNING,     ///< Non-critical issues
    ERROR,       ///< Critical issues, file should not be written
    FATAL        ///< Catastrophic issues (NaN coords, zero atoms, etc.)
};

/**
 * Validation result for molecular structures
 */
struct ValidationReport {
    ValidationSeverity severity = ValidationSeverity::OK;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    bool passed() const { 
        return severity == ValidationSeverity::OK || 
               severity == ValidationSeverity::WARNING; 
    }

    std::string summary() const;
};

// ============================================================================
// Single-Structure I/O (Most Common Case)
// ============================================================================

/**
 * Load molecular structure from file
 * 
 * Automatically detects format from extension unless opts.auto_detect_format = false.
 * Supports: .xyz, .xyza, .xyzc (first frame)
 * 
 * @param filename Path to file
 * @param opts I/O options (validation, bonds, etc.)
 * @return Result with loaded molecule or error
 * 
 * Example:
 *   auto result = load_structure("water.xyz");
 *   if (result) {
 *       XYZMolecule mol = result.value();
 *       std::cout << "Loaded " << mol.num_atoms() << " atoms\n";
 *   } else {
 *       std::cerr << "Error: " << result.error().message() << "\n";
 *   }
 */
Result<XYZMolecule> load_structure(
    const std::string& filename,
    const IOOptions& opts = {}
);

/**
 * Save molecular structure to file
 * 
 * Automatically selects format from extension unless format explicitly specified.
 * Performs validation before writing unless opts.validate_coords = false.
 * 
 * @param filename Output path
 * @param mol Molecule to save
 * @param opts I/O options (precision, metadata, etc.)
 * @return Result indicating success or error
 * 
 * Example:
 *   IOOptions opts;
 *   opts.precision = 8;
 *   opts.comment = "Optimized structure";
 *   auto result = save_structure("output.xyz", mol, opts);
 */
Status save_structure(
    const std::string& filename,
    const XYZMolecule& mol,
    const IOOptions& opts = {}
);

/**
 * Validate molecular structure before I/O
 * 
 * Checks for:
 * - Invalid coordinates (NaN, Inf)
 * - Unreasonable bond lengths (<0.5 Å, >4.0 Å)
 * - Missing element symbols
 * - Duplicate atoms at same position
 * 
 * @param mol Molecule to validate
 * @return Validation report with warnings/errors
 */
ValidationReport validate_structure(const XYZMolecule& mol);

// ============================================================================
// Multi-Frame Trajectory I/O
// ============================================================================

/**
 * Load complete trajectory from multi-frame XYZ file
 * 
 * Loads all frames into memory. For large trajectories (>10k frames),
 * consider using TrajectoryReader for streaming access.
 * 
 * @param filename Path to multi-frame XYZ or XYZA file
 * @param opts I/O options
 * @return Result with trajectory or error
 */
Result<XYZTrajectory> load_trajectory(
    const std::string& filename,
    const IOOptions& opts = {}
);

/**
 * Save complete trajectory to multi-frame XYZ file
 * 
 * @param filename Output path
 * @param traj Trajectory to save
 * @param opts I/O options (applied to all frames)
 * @return Result indicating success or error
 */
Status save_trajectory(
    const std::string& filename,
    const XYZTrajectory& traj,
    const IOOptions& opts = {}
);

// ============================================================================
// Streaming I/O (For Large Trajectories)
// ============================================================================

/**
 * Progressive trajectory writer
 * 
 * For writing trajectories frame-by-frame without loading entire sequence
 * into memory. Ideal for MD simulations with 10k+ frames.
 * 
 * Example:
 *   TrajectoryWriter writer("trajectory.xyza");
 *   for (int step = 0; step < num_steps; ++step) {
 *       XYZMolecule frame = run_md_step();
 *       writer.append_frame(frame, step * dt);
 *   }
 *   writer.finalize(); // Writes footer, closes file
 */
class TrajectoryWriter {
public:
    /**
     * Open trajectory file for writing
     * 
     * @param filename Output path
     * @param format Format (AUTO = detect from extension)
     * @param opts I/O options
     */
    explicit TrajectoryWriter(
        const std::string& filename,
        MolecularFormat format = MolecularFormat::AUTO,
        const IOOptions& opts = {}
    );

    ~TrajectoryWriter();

    // Non-copyable
    TrajectoryWriter(const TrajectoryWriter&) = delete;
    TrajectoryWriter& operator=(const TrajectoryWriter&) = delete;

    // Movable
    TrajectoryWriter(TrajectoryWriter&&) noexcept;
    TrajectoryWriter& operator=(TrajectoryWriter&&) noexcept;

    /**
     * Append single frame to trajectory
     *
     * @param mol Molecular structure for this frame
     * @param time Simulation time (optional, for MD trajectories)
     * @return Status indicating success or error
     */
    Status append_frame(const XYZMolecule& mol, double time = 0.0);

    /**
     * Finalize trajectory and close file
     *
     * Must be called explicitly or via destructor.
     * After finalize(), no more frames can be appended.
     *
     * @return Status indicating success or error
     */
    Status finalize();

    /**
     * Get number of frames written so far
     */
    size_t num_frames_written() const;

    /**
     * Check if writer is still open
     */
    bool is_open() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * Progressive trajectory reader
 * 
 * For reading trajectories frame-by-frame without loading entire sequence
 * into memory.
 * 
 * Example:
 *   TrajectoryReader reader("trajectory.xyza");
 *   while (reader.has_more_frames()) {
 *       auto result = reader.read_frame();
 *       if (result) {
 *           process_frame(result.value());
 *       }
 *   }
 */
class TrajectoryReader {
public:
    /**
     * Open trajectory file for reading
     * 
     * @param filename Input path
     * @param opts I/O options
     */
    explicit TrajectoryReader(
        const std::string& filename,
        const IOOptions& opts = {}
    );

    ~TrajectoryReader();

    // Non-copyable, movable
    TrajectoryReader(const TrajectoryReader&) = delete;
    TrajectoryReader& operator=(const TrajectoryReader&) = delete;
    TrajectoryReader(TrajectoryReader&&) noexcept;
    TrajectoryReader& operator=(TrajectoryReader&&) noexcept;

    /**
     * Read next frame from trajectory
     * 
     * @return Result with frame or error if no more frames
     */
    Result<XYZMolecule> read_frame();

    /**
     * Check if more frames available
     */
    bool has_more_frames() const;

    /**
     * Get total number of frames (if known)
     * 
     * Returns 0 if frame count unknown (e.g., streaming from pipe).
     */
    size_t num_frames() const;

    /**
     * Get current frame index
     */
    size_t current_frame() const;

    /**
     * Reset to beginning of trajectory
     */
    Status reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Format Utilities
// ============================================================================

/**
 * Detect molecular file format from filename or content
 * 
 * First checks extension, then file header if needed.
 */
MolecularFormat detect_format(const std::string& filename);

/**
 * Convert between file formats
 *
 * Example: convert_file("input.xyz", "output.xyza", opts);
 */
Status convert_file(
    const std::string& input_path,
    const std::string& output_path,
    const IOOptions& opts = {}
);

/**
 * Get format-specific information
 */
struct FormatInfo {
    std::string name;
    std::string extension;
    std::string description;
    bool supports_properties;    ///< Velocities, charges, etc.
    bool supports_trajectories;  ///< Multi-frame sequences
    bool is_binary;
};

FormatInfo get_format_info(MolecularFormat format);

// ============================================================================
// Batch Operations
// ============================================================================

/**
 * Load all structures from directory matching pattern
 * 
 * Example: load_directory("structures/", "*.xyz")
 */
Result<std::vector<XYZMolecule>> load_directory(
    const std::string& dir_path,
    const std::string& pattern = "*.xyz",
    const IOOptions& opts = {}
);

/**
 * Save multiple structures to separate files
 *
 * Uses {basename}_{index}.{ext} naming scheme.
 *
 * Example: save_batch("output/water", molecules)
 *          → output/water_0.xyz, output/water_1.xyz, ...
 */
Status save_batch(
    const std::string& basename,
    const std::vector<XYZMolecule>& molecules,
    const IOOptions& opts = {}
);

// ============================================================================
// Error Helpers
// ============================================================================

/**
 * Build a Status from an I/O error code, message, and optional filename
 */
Status make_io_error(
    ErrorCode code,
    const std::string& message,
    const std::string& filename = ""
);

}} // namespace vsepr::io
