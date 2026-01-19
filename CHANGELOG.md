# VSEPR-Sim Version History

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-01-17

### Added - Windows Distribution
- Professional Windows launcher script (`vsepr.bat`) with error handling
- Windows build script (`build_windows.bat`) with prerequisite checking
- Application icon support (.ico with multiple resolutions)
- Windows resource files (.rc) for executable metadata
- Icon generation script using Python/Pillow
- Comprehensive Windows distribution documentation
- CMake integration for Windows resources

### Added - Build System
- Automated dependency checking
- MinGW and MSVC support
- Parallel compilation support
- Clean build options
- Visualization build flags
- Test suite integration

### Added - Documentation
- WINDOWS_DISTRIBUTION.md - Distribution guidelines
- QUICKSTART.md - Getting started guide
- CLI_REFERENCE.md - Command reference
- BUILD_INSTRUCTIONS.md - Compilation guide

### Changed
- Improved error messages with actionable solutions
- Enhanced CMakeLists.txt for Windows compatibility
- Updated directory structure for distribution

### Fixed
- Path handling with spaces
- Missing data file detection
- Cross-platform compatibility issues

## [1.0.0] - Previous Release

### Added
- Core VSEPR geometry engine
- Molecular dynamics simulation
- Formula parser and builder
- XYZ file import/export
- Command-line interface
- Batch processing
- Periodic boundary conditions
- JSON chemistry database
- Multiple geometry optimizers

### Features
- Build molecules from chemical formulas
- Optimize geometry using VSEPR theory
- Molecular dynamics simulation
- Conformer search
- Isomer enumeration
- Visualization support (OpenGL)

---

## Version Numbering

- **Major**: Breaking API changes, major new features
- **Minor**: New features, backward compatible
- **Patch**: Bug fixes, minor improvements

## Release Types

- **Stable**: Production-ready releases
- **Beta**: Feature-complete, testing phase
- **Alpha**: Early preview, experimental features

[2.0.0]: https://github.com/yourusername/vsepr-sim/compare/v1.0.0...v2.0.0
[1.0.0]: https://github.com/yourusername/vsepr-sim/releases/tag/v1.0.0
