/**
 * pbc_test.cpp - Test periodic boundary conditions
 */

#include "box/pbc.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>

using namespace vsepr;

void test_wrap() {
    std::cout << "Testing wrap()...\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    // In bounds
    Vec3 r1 = box.wrap({5.0, 5.0, 5.0});
    assert(std::abs(r1.x - 5.0) < 1e-10);
    assert(std::abs(r1.y - 5.0) < 1e-10);
    
    // Out of bounds positive
    Vec3 r2 = box.wrap({15.0, 5.0, 5.0});
    assert(std::abs(r2.x - 5.0) < 1e-10);
    
    // Out of bounds negative
    Vec3 r3 = box.wrap({-0.5, 5.0, 5.0});
    assert(std::abs(r3.x - 9.5) < 1e-10);
    
    // Multiple wraps
    Vec3 r4 = box.wrap({25.0, -15.0, 5.0});
    assert(std::abs(r4.x - 5.0) < 1e-10);
    assert(std::abs(r4.y - 5.0) < 1e-10);
    
    std::cout << "  ✓ All wrap tests passed\n";
}

void test_minimum_image() {
    std::cout << "Testing minimum image convention...\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    // Same position
    Vec3 dr1 = box.delta({5, 5, 5}, {5, 5, 5});
    assert(dr1.norm() < 1e-10);
    
    // Near neighbors (no wrapping needed)
    Vec3 dr2 = box.delta({5, 5, 5}, {6, 5, 5});
    assert(std::abs(dr2.x - 1.0) < 1e-10);
    assert(std::abs(dr2.y - 0.0) < 1e-10);
    
    // Across boundary (should use closer image)
    // From x=1 to x=9: direct = +8, wrapped = -2 (shorter!)
    Vec3 dr3 = box.delta({1, 5, 5}, {9, 5, 5});
    assert(std::abs(dr3.x - (-2.0)) < 1e-10);
    
    // From x=9 to x=1: direct = -8, wrapped = +2
    Vec3 dr4 = box.delta({9, 5, 5}, {1, 5, 5});
    assert(std::abs(dr4.x - 2.0) < 1e-10);
    
    std::cout << "  ✓ All minimum image tests passed\n";
}

void test_distance() {
    std::cout << "Testing distance calculations...\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    // Regular distance
    double d1 = box.dist({0, 0, 0}, {3, 4, 0});
    assert(std::abs(d1 - 5.0) < 1e-10);
    
    // Distance across boundary
    double d2 = box.dist({1, 0, 0}, {9, 0, 0});
    assert(std::abs(d2 - 2.0) < 1e-10);  // Wraps to -2
    
    // Distance squared (avoid sqrt)
    double d2_sq = box.dist2({1, 0, 0}, {9, 0, 0});
    assert(std::abs(d2_sq - 4.0) < 1e-10);
    
    std::cout << "  ✓ All distance tests passed\n";
}

void test_disabled_box() {
    std::cout << "Testing disabled box (no PBC)...\n";
    
    BoxOrtho box;  // Default constructor, disabled
    assert(!box.enabled());
    
    // Should behave as if no PBC
    Vec3 r1 = box.wrap({15.0, 5.0, 5.0});
    assert(std::abs(r1.x - 15.0) < 1e-10);  // No wrapping
    
    Vec3 dr = box.delta({1, 0, 0}, {9, 0, 0});
    assert(std::abs(dr.x - 8.0) < 1e-10);  // No MIC
    
    std::cout << "  ✓ Disabled box tests passed\n";
}

void test_coord_array() {
    std::cout << "Testing coordinate array wrapping...\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    std::vector<double> coords = {
        15.0, 5.0, 5.0,   // Atom 0: out of bounds
        -0.5, 5.0, 5.0,   // Atom 1: negative
        5.0, 5.0, 5.0     // Atom 2: in bounds
    };
    
    box.wrap_coords(coords);
    
    assert(std::abs(coords[0] - 5.0) < 1e-10);   // Wrapped
    assert(std::abs(coords[3] - 9.5) < 1e-10);   // Wrapped
    assert(std::abs(coords[6] - 5.0) < 1e-10);   // Unchanged
    
    std::cout << "  ✓ Coordinate array tests passed\n";
}

void test_set_dimensions() {
    std::cout << "Testing dynamic box resizing...\n";
    
    BoxOrtho box;
    assert(!box.enabled());
    
    box.set_dimensions(10.0, 10.0, 10.0);
    assert(box.enabled());
    assert(std::abs(box.volume() - 1000.0) < 1e-10);
    
    // Verify invL is updated
    Vec3 r = box.wrap({15.0, 5.0, 5.0});
    assert(std::abs(r.x - 5.0) < 1e-10);
    
    // Change size
    box.set_dimensions(Vec3{20.0, 20.0, 20.0});
    r = box.wrap({15.0, 5.0, 5.0});
    assert(std::abs(r.x - 15.0) < 1e-10);  // Now in bounds
    
    std::cout << "  ✓ Dynamic resizing tests passed\n";
}

int main() {
    std::cout << "\n";
    std::cout << "=================================\n";
    std::cout << "  PBC Implementation Test Suite\n";
    std::cout << "=================================\n\n";
    
    test_wrap();
    test_minimum_image();
    test_distance();
    test_disabled_box();
    test_coord_array();
    test_set_dimensions();
    
    std::cout << "\n";
    std::cout << "=================================\n";
    std::cout << "  ✓ All PBC tests passed!\n";
    std::cout << "=================================\n\n";
    
    return 0;
}
