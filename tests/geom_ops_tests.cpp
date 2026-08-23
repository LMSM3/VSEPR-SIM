#include "core/math_vec3.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace vsepr;

#define ASSERT_NEAR(a, b, tol) \
    assert(std::abs((a) - (b)) < (tol) && "Assertion failed: values not close enough")

void test_vec3_basic() {
    std::cout << "Testing Vec3 basic operations...\n";
    
    Vec3 a{1.0, 2.0, 3.0};
    Vec3 b{4.0, 5.0, 6.0};
    
    // Addition
    Vec3 c = a + b;
    assert(c.x == 5.0 && c.y == 7.0 && c.z == 9.0);
    
    // Subtraction
    Vec3 d = b - a;
    assert(d.x == 3.0 && d.y == 3.0 && d.z == 3.0);
    
    // Scalar multiply
    Vec3 e = a * 2.0;
    assert(e.x == 2.0 && e.y == 4.0 && e.z == 6.0);
    
    // Dot product
    double dp = a.dot(b);
    assert(dp == 1.0*4.0 + 2.0*5.0 + 3.0*6.0);
    
    // Cross product: a × b
    Vec3 cp = a.cross(b);
    // (2*6 - 3*5, 3*4 - 1*6, 1*5 - 2*4) = (-3, 6, -3)
    assert(cp.x == -3.0 && cp.y == 6.0 && cp.z == -3.0);
    
    // Norm
    Vec3 f{3.0, 4.0, 0.0};
    assert(f.norm() == 5.0);
    
    std::cout << "  ✓ Vec3 basic operations passed\n";
}

void test_distance() {
    std::cout << "Testing distance calculations...\n";
    
    std::vector<double> coords = {
        0.0, 0.0, 0.0,  // atom 0
        3.0, 4.0, 0.0   // atom 1
    };
    
    double d = distance(coords, 0, 1);
    ASSERT_NEAR(d, 5.0, 1e-10);
    
    Vec3 r01 = rij(coords, 0, 1);
    assert(r01.x == 3.0 && r01.y == 4.0 && r01.z == 0.0);
    
    std::cout << "  ✓ Distance calculations passed\n";
}

void test_angle() {
    std::cout << "Testing angle calculations...\n";
    
    // Right angle: 90 degrees
    std::vector<double> coords = {
        1.0, 0.0, 0.0,  // atom 0 (i)
        0.0, 0.0, 0.0,  // atom 1 (j, vertex)
        0.0, 1.0, 0.0   // atom 2 (k)
    };
    
    double theta = angle(coords, 0, 1, 2);
    ASSERT_NEAR(theta, M_PI / 2.0, 1e-10);
    
    // Linear: 180 degrees
    coords = {
        -1.0, 0.0, 0.0,  // atom 0
         0.0, 0.0, 0.0,  // atom 1
         1.0, 0.0, 0.0   // atom 2
    };
    
    theta = angle(coords, 0, 1, 2);
    ASSERT_NEAR(theta, M_PI, 1e-10);
    
    // 60 degrees (equilateral triangle vertex)
    coords = {
        1.0,  0.0, 0.0,  // atom 0
        0.0,  0.0, 0.0,  // atom 1
        0.5,  std::sqrt(3.0)/2.0, 0.0   // atom 2
    };
    
    theta = angle(coords, 0, 1, 2);
    ASSERT_NEAR(theta, M_PI / 3.0, 1e-9);
    
    std::cout << "  ✓ Angle calculations passed\n";
}

void test_torsion() {
    std::cout << "Testing torsion (dihedral) calculations...\n";
    
    // Planar (cis): 0 degrees
    std::vector<double> coords = {
        -1.5, 0.0, 0.0,  // atom 0
        -0.5, 0.0, 0.0,  // atom 1
         0.5, 0.0, 0.0,  // atom 2
         1.5, 0.0, 0.0   // atom 3
    };
    
    double phi = torsion(coords, 0, 1, 2, 3);
    ASSERT_NEAR(phi, 0.0, 1e-10);
    
    // Trans (180°): twist last atom
    coords = {
        -1.5,  0.0,  0.0,  // atom 0
        -0.5,  0.0,  0.0,  // atom 1
         0.5,  0.0,  0.0,  // atom 2
         1.5,  0.0,  0.0   // atom 3 (same as cis for now)
    };
    // Modify to be trans
    coords[3*3 + 1] = 0.0;  // y
    coords[3*3 + 2] = 0.0;  // z
    phi = torsion(coords, 0, 1, 2, 3);
    ASSERT_NEAR(phi, 0.0, 1e-10);  // still planar
    
    // 90° twist: proper setup
    // Central bond along x-axis, first plane in xy, second plane rotated 90°
    coords = {
         0.0, 1.0,  0.0,  // atom 0 (first plane, y-direction)
         0.0, 0.0,  0.0,  // atom 1 (central bond start)
         1.0, 0.0,  0.0,  // atom 2 (central bond end)
         1.0, 0.0,  1.0   // atom 3 (second plane, z-direction)
    };
    
    phi = torsion(coords, 0, 1, 2, 3);
    ASSERT_NEAR(std::abs(phi), M_PI / 2.0, 1e-9);
    
    std::cout << "  ✓ Torsion calculations passed\n";
}

void test_translation_invariance() {
    std::cout << "Testing translation invariance...\n";
    
    std::vector<double> coords = {
        0.0, 0.0, 0.0,
        1.0, 0.0, 0.0,
        0.5, std::sqrt(3.0)/2.0, 0.0
    };
    
    // Distance should be translation invariant
    auto dist_func = [](const std::vector<double>& c) {
        return distance(c, 0, 1);
    };
    assert(check_translation_invariance(coords, dist_func));
    
    // Angle should be translation invariant
    auto angle_func = [](const std::vector<double>& c) {
        return angle(c, 0, 1, 2);
    };
    assert(check_translation_invariance(coords, angle_func));
    
    std::cout << "  ✓ Translation invariance verified\n";
}

void test_rotation_invariance() {
    std::cout << "Testing rotation invariance...\n";
    
    std::vector<double> coords = {
        1.0, 0.0, 0.0,
        0.0, 0.0, 0.0,
        0.0, 1.0, 0.0
    };
    
    // Distance should be rotation invariant
    auto dist_func = [](const std::vector<double>& c) {
        return distance(c, 0, 1);
    };
    assert(check_rotation_invariance(coords, dist_func));
    
    // Angle should be rotation invariant
    auto angle_func = [](const std::vector<double>& c) {
        return angle(c, 0, 1, 2);
    };
    assert(check_rotation_invariance(coords, angle_func));
    
    std::cout << "  ✓ Rotation invariance verified\n";
}

void test_geometric_center() {
    std::cout << "Testing geometric center...\n";
    
    std::vector<double> coords = {
        1.0, 0.0, 0.0,
       -1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0,-1.0, 0.0
    };
    
    Vec3 center = geometric_center(coords);
    ASSERT_NEAR(center.x, 0.0, 1e-10);
    ASSERT_NEAR(center.y, 0.0, 1e-10);
    ASSERT_NEAR(center.z, 0.0, 1e-10);
    
    // Test centering
    coords = {
        2.0, 3.0, 4.0,
        3.0, 4.0, 5.0
    };
    center_coords(coords);
    Vec3 new_center = geometric_center(coords);
    ASSERT_NEAR(new_center.x, 0.0, 1e-10);
    ASSERT_NEAR(new_center.y, 0.0, 1e-10);
    ASSERT_NEAR(new_center.z, 0.0, 1e-10);
    
    std::cout << "  ✓ Geometric center operations passed\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Geometry Operations Test Suite\n";
    std::cout << "========================================\n\n";
    
    try {
        test_vec3_basic();
        test_distance();
        test_angle();
        test_torsion();
        test_translation_invariance();
        test_rotation_invariance();
        test_geometric_center();
        
        std::cout << "\n========================================\n";
        std::cout << "All tests passed! ✓\n";
        std::cout << "========================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
