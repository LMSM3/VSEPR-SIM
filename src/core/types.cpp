#include "core/types.hpp"
#include <cmath>

namespace vsepr {

double Vec3::length() const {
    return std::sqrt(x * x + y * y + z * z);
}

Vec3 Vec3::normalized() const {
    double len = length();
    if (len > 0.0) {
        return Vec3(x / len, y / len, z / len);
    }
    return Vec3(0.0, 0.0, 0.0);
}

} // namespace vsepr
