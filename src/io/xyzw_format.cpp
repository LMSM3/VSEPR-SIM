/**
 * XYZW Format Implementation
 *
 * Wind particle field I/O:
 *   element x y z  wx wy wz  omega
 *
 * Header line format:
 *   WIND | Source=<str> | Direction=(<dx>,<dy>,<dz>) | Magnitude=<f> |
 *         Wavelength=<f> | Decay=<f> | Step=<n> | Time=<f>fs | Seed=<n>
 *
 * Reference: docs/FILE_FORMATS.md § XYZW Format: Wind Particle Field
 */

#include "io/xyz_format.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace vsepr {
namespace io {

// ── XYZWField helpers ──────────────────────────────────────────────────────

void XYZWField::clamp_omega() {
    for (auto& a : atoms)
        a.omega = std::max(0.0, std::min(1.0, a.omega));
}

void XYZWField::scale_wind(double factor) {
    for (auto& a : atoms) {
        a.wind[0] *= factor;
        a.wind[1] *= factor;
        a.wind[2] *= factor;
    }
}

std::vector<std::array<double, 3>> XYZWField::to_velocity_offsets() const {
    std::vector<std::array<double, 3>> offsets;
    offsets.reserve(atoms.size());
    for (const auto& a : atoms) {
        offsets.push_back({a.wind[0] * a.omega,
                           a.wind[1] * a.omega,
                           a.wind[2] * a.omega});
    }
    return offsets;
}

// ── XYZWReader ─────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

void XYZWReader::parse_header_meta(const std::string& comment, XYZWField& field) {
    // Tokenise on '|'
    std::istringstream ss(comment);
    std::string token;
    while (std::getline(ss, token, '|')) {
        token = trim(token);
        if (token.empty() || token == "WIND") continue;

        auto eq = token.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(token.substr(0, eq));
        std::string val = trim(token.substr(eq + 1));

        if (key == "Source") {
            field.source = val;
        } else if (key == "Magnitude") {
            try { field.magnitude = std::stod(val); } catch (...) {}
        } else if (key == "Wavelength") {
            try { field.wavelength = std::stod(val); } catch (...) {}
        } else if (key == "Decay") {
            try { field.decay = std::stod(val); } catch (...) {}
        } else if (key == "Step") {
            try { field.step = static_cast<uint64_t>(std::stoll(val)); } catch (...) {}
        } else if (key == "Seed") {
            try { field.seed = static_cast<uint64_t>(std::stoll(val)); } catch (...) {}
        } else if (key == "Time") {
            // Accept "10.5fs" or "10.5"
            try {
                size_t idx = 0;
                field.time = std::stod(val, &idx);
            } catch (...) {}
        } else if (key == "Direction") {
            // Expect (dx,dy,dz) — strip parens
            std::string v = val;
            v.erase(std::remove_if(v.begin(), v.end(),
                    [](char c){ return c == '(' || c == ')'; }), v.end());
            std::istringstream dss(v);
            std::string part;
            int idx = 0;
            while (std::getline(dss, part, ',') && idx < 3) {
                try { field.direction[idx++] = std::stod(trim(part)); } catch (...) {}
            }
        }
    }
}

bool XYZWReader::read(const std::string& filename, XYZWField& field) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        error_message_ = "Cannot open file: " + filename;
        return false;
    }
    return read_stream(f, field);
}

bool XYZWReader::read_stream(std::istream& input, XYZWField& field) {
    field = XYZWField{};

    std::string line;

    // Line 1: atom count
    if (!std::getline(input, line)) {
        error_message_ = "Empty XYZW stream";
        return false;
    }
    int n_atoms = 0;
    try { n_atoms = std::stoi(trim(line)); } catch (...) {
        error_message_ = "Invalid atom count: " + line;
        return false;
    }
    if (n_atoms <= 0) {
        error_message_ = "Atom count must be positive";
        return false;
    }

    // Line 2: header / comment
    if (!std::getline(input, line)) {
        error_message_ = "Missing XYZW header line";
        return false;
    }
    if (line.find("WIND") == std::string::npos) {
        error_message_ = "Header line missing WIND keyword";
        return false;
    }
    parse_header_meta(line, field);

    // Atom lines
    field.atoms.reserve(n_atoms);
    int warnings = 0;
    for (int i = 0; i < n_atoms; ++i) {
        if (!std::getline(input, line)) {
            error_message_ = "Unexpected end of file at atom " + std::to_string(i);
            return false;
        }
        std::istringstream ss(line);
        XYZWAtom atom;
        if (!(ss >> atom.element
                 >> atom.position[0] >> atom.position[1] >> atom.position[2]
                 >> atom.wind[0]     >> atom.wind[1]     >> atom.wind[2]
                 >> atom.omega)) {
            error_message_ = "Parse error at atom line " + std::to_string(i + 3);
            return false;
        }
        if (atom.omega < 0.0 || atom.omega > 1.0) {
            ++warnings;
            atom.omega = std::max(0.0, std::min(1.0, atom.omega));
        }
        field.atoms.push_back(atom);
    }

    if (static_cast<int>(field.atoms.size()) != n_atoms) {
        error_message_ = "Atom count mismatch";
        return false;
    }
    return true;
}

// ── XYZWWriter ─────────────────────────────────────────────────────────────

std::string XYZWWriter::build_header_comment(const XYZWField& field) const {
    std::ostringstream ss;
    ss << "WIND";
    if (!field.source.empty())
        ss << " | Source=" << field.source;
    ss << " | Direction=("
       << field.direction[0] << ","
       << field.direction[1] << ","
       << field.direction[2] << ")";
    ss << " | Magnitude=" << field.magnitude;
    if (field.wavelength > 0.0)
        ss << " | Wavelength=" << field.wavelength;
    if (field.decay > 0.0)
        ss << " | Decay=" << field.decay;
    ss << " | Step=" << field.step;
    ss << " | Time=" << field.time << "fs";
    if (field.seed > 0)
        ss << " | Seed=" << field.seed;
    return ss.str();
}

bool XYZWWriter::write(const std::string& filename, const XYZWField& field) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        error_message_ = "Cannot open file for writing: " + filename;
        return false;
    }
    return write_stream(f, field);
}

bool XYZWWriter::write_stream(std::ostream& output, const XYZWField& field) {
    output << field.atoms.size() << "\n";
    output << build_header_comment(field) << "\n";

    output << std::fixed << std::setprecision(precision_);
    for (const auto& a : field.atoms) {
        output << std::left  << std::setw(3)  << a.element
               << std::right
               << " " << std::setw(12) << a.position[0]
               << " " << std::setw(12) << a.position[1]
               << " " << std::setw(12) << a.position[2]
               << "   "
               << std::setw(10) << a.wind[0]
               << " " << std::setw(10) << a.wind[1]
               << " " << std::setw(10) << a.wind[2]
               << "   "
               << std::setw(8)  << a.omega
               << "\n";
    }
    return output.good();
}

std::string XYZWWriter::to_string(const XYZWField& field) {
    std::ostringstream ss;
    write_stream(ss, field);
    return ss.str();
}

}} // namespace vsepr::io
