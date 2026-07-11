/**
 * xyzl_format.hpp
 * ----------------
 * XYZL Format: Line-Segment / Bead Dynamics with Sensor Integration.
 *
 * The .xyzL format extends the XYZ family to represent:
 *   - Coarse-grained beads with line-segment extents
 *   - Embedded sensor objects (wind, material, energy)
 *   - Friction-coupled bead dynamics
 *   - Multi-frame bead trajectories
 *
 * Format specification:
 *   Line 1: <N_entries>
 *   Line 2: <comment> [meta key=value pairs]
 *   Lines 3+: <type> <fields...>
 *
 * Entry types:
 *   B  — Bead:    B <label> <x0> <y0> <z0> <x1> <y1> <z1> <mass> <charge> <vx> <vy> <vz> <mu>
 *   S  — Sensor:  S <name> <type> <x0> <y0> <z0> <x1> <y1> <z1> <radius> <mu>
 *   L  — Link:    L <bead_i> <bead_j> <order>
 *
 * Comment line metadata:
 *   frame=<N> time=<fs> dt=<fs> energy=<eV> temperature=<K>
 *   box=<Lx,Ly,Lz> pbc=<1|0>
 *
 * Unit system (consistent with XYZ family):
 *   Coordinates: Å          Velocity: Å/fs
 *   Mass: amu               Charge: e
 *   Energy: eV              Time: fs
 *   Friction: dimensionless (dynamic friction coefficient)
 *
 * This format is designed for:
 *   - Coarse-grained bead dynamics (BeadSystem export/import)
 *   - Sensor-instrumented fluid simulations
 *   - Restartable bead-level checkpoints
 *   - Multi-frame bead trajectories
 *
 * Anti-black-box: plain-text, line-oriented, every field labeled by column.
 */

#pragma once

#include "sensor/sensor.hpp"
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <map>

namespace vsepr {
namespace io {

// ============================================================================
// XYZL data structures
// ============================================================================

/**
 * XYZLBead — one coarse-grained bead with line-segment extent.
 *
 * A bead occupies a directed segment [p0, p1] in space, not just a point.
 * This captures bead anisotropy (elongated polymers, rod-like molecules).
 * Spherical beads have p0 ≈ p1 (degenerate segment).
 */
struct XYZLBead {
    std::string label;           // Bead type label ("BB", "SC1", "W", "ION", ...)
    std::array<double, 3> p0;   // Segment start (Å)
    std::array<double, 3> p1;   // Segment end (Å)
    double mass   = 1.0;        // amu
    double charge = 0.0;        // e
    std::array<double, 3> velocity = {0.0, 0.0, 0.0};  // Å/fs
    double mu     = sensor::MU_DYNAMIC_SENSOR;  // Local friction coefficient

    // Derived
    double length() const {
        double dx = p1[0]-p0[0], dy = p1[1]-p0[1], dz = p1[2]-p0[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    std::array<double, 3> midpoint() const {
        return {(p0[0]+p1[0])*0.5, (p0[1]+p1[1])*0.5, (p0[2]+p1[2])*0.5};
    }
    std::array<double, 3> axis() const {
        double L = length();
        if (L < 1e-30) return {0,0,0};
        return {(p1[0]-p0[0])/L, (p1[1]-p0[1])/L, (p1[2]-p0[2])/L};
    }
    double speed() const {
        return std::sqrt(velocity[0]*velocity[0] +
                         velocity[1]*velocity[1] +
                         velocity[2]*velocity[2]);
    }
    double kinetic_energy() const { return 0.5 * mass * speed() * speed(); }
};

/**
 * XYZLSensor — embedded sensor in the bead dynamics frame.
 * Wraps sensor::Sensor for file serialisation.
 */
struct XYZLSensor {
    std::string name;
    sensor::SensorType type = sensor::SensorType::WIND;
    std::array<double, 3> p0 = {0,0,0};
    std::array<double, 3> p1 = {0,0,0};
    double radius = 2.0;
    double mu     = sensor::MU_DYNAMIC_SENSOR;

    // Convert to runtime Sensor
    sensor::Sensor to_sensor(uint32_t id) const {
        sensor::Sensor s;
        s.id             = id;
        s.name           = name;
        s.type           = type;
        s.p0             = {p0[0], p0[1], p0[2]};
        s.p1             = {p1[0], p1[1], p1[2]};
        s.capture_radius = radius;
        s.mu_dynamic     = mu;
        s.update_geometry();
        return s;
    }

    // Build from runtime Sensor
    static XYZLSensor from_sensor(const sensor::Sensor& s) {
        XYZLSensor ls;
        ls.name   = s.name;
        ls.type   = s.type;
        ls.p0     = {s.p0.x, s.p0.y, s.p0.z};
        ls.p1     = {s.p1.x, s.p1.y, s.p1.z};
        ls.radius = s.capture_radius;
        ls.mu     = s.mu_dynamic;
        return ls;
    }
};

/**
 * XYZLLink — bead-bead connectivity.
 */
struct XYZLLink {
    int bead_i  = 0;
    int bead_j  = 0;
    double order = 1.0;  // Bond order / coupling strength
};

/**
 * XYZLFrame — one frame of bead dynamics + sensors.
 */
struct XYZLFrame {
    // Beads
    std::vector<XYZLBead>   beads;
    // Sensors
    std::vector<XYZLSensor> sensors;
    // Links
    std::vector<XYZLLink>   links;

    // Metadata
    std::string comment;
    uint64_t frame_number = 0;
    double time_fs        = 0.0;
    double dt_fs          = 1.0;
    double total_energy   = 0.0;  // eV
    double temperature    = 0.0;  // K

    // Bounding box (optional)
    std::array<double, 3> box = {0,0,0};
    bool has_pbc = false;

    // Entry count for serialisation
    size_t entry_count() const {
        return beads.size() + sensors.size() + links.size();
    }

    // Compute total kinetic energy
    double total_kinetic_energy() const {
        double ke = 0.0;
        for (const auto& b : beads) ke += b.kinetic_energy();
        return ke;
    }

    // Compute kinetic temperature from bead KE
    // T = 2 * KE_total / (3 * N * kB)
    double kinetic_temperature() const {
        if (beads.empty()) return 0.0;
        constexpr double kB_eV = 8.617333e-5; // eV/K
        // Convert KE from amu·Å²/fs² to eV: 1 amu·Å²/fs² = 103.6427 eV
        constexpr double amu_A2_fs2_to_eV = 103.6427;
        double ke_eV = total_kinetic_energy() * amu_A2_fs2_to_eV;
        return (2.0 / 3.0) * ke_eV / (beads.size() * kB_eV);
    }

    // Extract a ParticleSnapshot for sensor measurements
    sensor::ParticleSnapshot to_particle_snapshot() const {
        sensor::ParticleSnapshot snap;
        for (const auto& b : beads) {
            auto mid = b.midpoint();
            snap.positions.push_back({mid[0], mid[1], mid[2]});
            snap.velocities.push_back({b.velocity[0], b.velocity[1], b.velocity[2]});
            snap.elements.push_back(b.label);
            snap.masses.push_back(b.mass);
            snap.charges.push_back(b.charge);
            snap.pe_per_atom.push_back(0.0); // PE not stored per-bead in .xyzL
        }
        return snap;
    }

    // Build runtime SensorArray from embedded sensors
    sensor::SensorArray to_sensor_array() const {
        sensor::SensorArray arr;
        uint32_t id = 0;
        for (const auto& ls : sensors)
            arr.add(ls.to_sensor(id++));
        return arr;
    }
};

/**
 * XYZLTrajectory — multi-frame bead dynamics trajectory.
 */
struct XYZLTrajectory {
    std::vector<XYZLFrame> frames;

    size_t num_frames() const { return frames.size(); }
    void add_frame(const XYZLFrame& f) { frames.push_back(f); }
};

// ============================================================================
// XYZL Writer
// ============================================================================

class XYZLWriter {
public:
    XYZLWriter() = default;

    void set_precision(int digits) { precision_ = digits; }

    bool write(const std::string& filename, const XYZLFrame& frame) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            error_ = "Cannot open file: " + filename;
            return false;
        }
        return write_stream(file, frame);
    }

    bool write_trajectory(const std::string& filename, const XYZLTrajectory& traj) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            error_ = "Cannot open file: " + filename;
            return false;
        }
        for (const auto& f : traj.frames) {
            if (!write_stream(file, f)) return false;
        }
        return true;
    }

    bool write_stream(std::ostream& out, const XYZLFrame& frame) {
        out << frame.entry_count() << "\n";
        out << build_comment(frame) << "\n";

        out << std::fixed << std::setprecision(precision_);

        // Beads
        for (const auto& b : frame.beads) {
            out << "B " << b.label
                << " " << b.p0[0] << " " << b.p0[1] << " " << b.p0[2]
                << " " << b.p1[0] << " " << b.p1[1] << " " << b.p1[2]
                << " " << b.mass << " " << b.charge
                << " " << b.velocity[0] << " " << b.velocity[1] << " " << b.velocity[2]
                << " " << b.mu << "\n";
        }

        // Sensors
        for (const auto& s : frame.sensors) {
            out << "S " << s.name
                << " " << sensor::sensor_type_name(s.type)
                << " " << s.p0[0] << " " << s.p0[1] << " " << s.p0[2]
                << " " << s.p1[0] << " " << s.p1[1] << " " << s.p1[2]
                << " " << s.radius
                << " " << s.mu << "\n";
        }

        // Links
        for (const auto& l : frame.links) {
            out << "L " << l.bead_i << " " << l.bead_j
                << " " << l.order << "\n";
        }

        return true;
    }

    std::string to_string(const XYZLFrame& frame) {
        std::ostringstream ss;
        write_stream(ss, frame);
        return ss.str();
    }

    const std::string& get_error() const { return error_; }

private:
    int precision_ = 6;
    std::string error_;

    std::string build_comment(const XYZLFrame& frame) const {
        std::ostringstream c;
        c << std::fixed << std::setprecision(precision_);
        if (!frame.comment.empty()) c << frame.comment << " | ";
        c << "frame=" << frame.frame_number
          << " time=" << frame.time_fs
          << " dt=" << frame.dt_fs
          << " energy=" << frame.total_energy
          << " temperature=" << frame.temperature;
        if (frame.has_pbc) {
            c << " box=" << frame.box[0] << "," << frame.box[1] << "," << frame.box[2]
              << " pbc=1";
        }
        c << " beads=" << frame.beads.size()
          << " sensors=" << frame.sensors.size()
          << " links=" << frame.links.size();
        return c.str();
    }
};

// ============================================================================
// XYZL Reader
// ============================================================================

class XYZLReader {
public:
    XYZLReader() = default;

    bool read(const std::string& filename, XYZLFrame& frame) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            error_ = "Cannot open file: " + filename;
            return false;
        }
        return read_stream(file, frame);
    }

    bool read_trajectory(const std::string& filename, XYZLTrajectory& traj) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            error_ = "Cannot open file: " + filename;
            return false;
        }
        while (file.good() && file.peek() != EOF) {
            XYZLFrame frame;
            if (!read_stream(file, frame)) break;
            traj.frames.push_back(std::move(frame));
        }
        return !traj.frames.empty();
    }

    bool read_stream(std::istream& in, XYZLFrame& frame) {
        std::string line;

        // Line 1: entry count
        if (!std::getline(in, line)) return false;
        int n_entries = 0;
        try { n_entries = std::stoi(line); }
        catch (...) { error_ = "Invalid entry count: " + line; return false; }

        // Line 2: comment + metadata
        if (!std::getline(in, line)) return false;
        frame.comment = line;
        parse_metadata(line, frame);

        // Lines 3+: entries
        for (int i = 0; i < n_entries; ++i) {
            if (!std::getline(in, line)) {
                error_ = "Unexpected EOF at entry " + std::to_string(i);
                return false;
            }
            if (line.empty()) { --i; continue; }

            std::istringstream iss(line);
            char type;
            iss >> type;

            if (type == 'B') {
                XYZLBead b;
                iss >> b.label
                    >> b.p0[0] >> b.p0[1] >> b.p0[2]
                    >> b.p1[0] >> b.p1[1] >> b.p1[2]
                    >> b.mass >> b.charge
                    >> b.velocity[0] >> b.velocity[1] >> b.velocity[2]
                    >> b.mu;
                frame.beads.push_back(std::move(b));
            }
            else if (type == 'S') {
                XYZLSensor s;
                std::string type_str;
                iss >> s.name >> type_str
                    >> s.p0[0] >> s.p0[1] >> s.p0[2]
                    >> s.p1[0] >> s.p1[1] >> s.p1[2]
                    >> s.radius >> s.mu;
                if (type_str == "wind")     s.type = sensor::SensorType::WIND;
                else if (type_str == "material") s.type = sensor::SensorType::MATERIAL;
                else if (type_str == "energy")   s.type = sensor::SensorType::ENERGY;
                frame.sensors.push_back(std::move(s));
            }
            else if (type == 'L') {
                XYZLLink l;
                iss >> l.bead_i >> l.bead_j >> l.order;
                frame.links.push_back(l);
            }
        }

        return true;
    }

    const std::string& get_error() const { return error_; }

private:
    std::string error_;

    void parse_metadata(const std::string& comment, XYZLFrame& frame) {
        // Parse key=value pairs from comment line
        auto find_val = [&](const std::string& key) -> std::string {
            size_t pos = comment.find(key + "=");
            if (pos == std::string::npos) return "";
            pos += key.size() + 1;
            size_t end = comment.find_first_of(" \t|", pos);
            return comment.substr(pos, end - pos);
        };

        auto to_d = [](const std::string& s) -> double {
            try { return std::stod(s); } catch (...) { return 0.0; }
        };
        auto to_u = [](const std::string& s) -> uint64_t {
            try { return std::stoull(s); } catch (...) { return 0; }
        };

        std::string v;
        if (!(v = find_val("frame")).empty())       frame.frame_number = to_u(v);
        if (!(v = find_val("time")).empty())        frame.time_fs      = to_d(v);
        if (!(v = find_val("dt")).empty())          frame.dt_fs        = to_d(v);
        if (!(v = find_val("energy")).empty())      frame.total_energy = to_d(v);
        if (!(v = find_val("temperature")).empty()) frame.temperature  = to_d(v);

        if (!(v = find_val("box")).empty()) {
            // Parse box=Lx,Ly,Lz
            std::istringstream bss(v);
            char comma;
            bss >> frame.box[0] >> comma >> frame.box[1] >> comma >> frame.box[2];
        }
        if (!(v = find_val("pbc")).empty()) {
            frame.has_pbc = (v == "1" || v == "true");
        }
    }
};

// ============================================================================
// Format detection extension
// ============================================================================
//
// Add XYZL to the format family.
// Caller can check: if extension == ".xyzL" || ".xyzl" → use XYZLReader.
//

inline bool is_xyzl_file(const std::string& filename) {
    if (filename.size() < 5) return false;
    std::string ext = filename.substr(filename.size() - 5);
    // Case-insensitive check for ".xyzl" or ".xyzL"
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".xyzl";
}

} // namespace io
} // namespace vsepr
