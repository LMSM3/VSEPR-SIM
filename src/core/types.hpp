#pragma once

#include <cstddef>
#include <vector>
#include <string>

namespace vsepr {

// Basic 3D vector structure
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    
    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(double scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    
    double dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    
    double length() const;
    Vec3 normalized() const;
};

// Atom structure
struct Atom {
    std::string element;
    Vec3 position;
    Vec3 velocity;
    Vec3 force;
    double mass = 1.0;
    double charge = 0.0;
    int id = 0;
    
    Atom() = default;
    Atom(const std::string& elem, const Vec3& pos) 
        : element(elem), position(pos) {}
};

// Molecule structure
struct Molecule {
    std::vector<Atom> atoms;
    double energy = 0.0;
    
    void addAtom(const Atom& atom) {
        atoms.push_back(atom);
    }
    
    size_t size() const {
        return atoms.size();
    }
};

} // namespace vsepr
