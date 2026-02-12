/**
 * test_pbc_integration.cpp
 * 
 * Quick smoke test to verify PBC integration works
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

int main() {
    std::cout << "=== PBC Integration Test ===" << std::endl;
    
    // Test 1: BoxPBC math
    {
        std::cout << "\n[Test 1] BoxPBC MIC calculation" << std::endl;
        
        vsepr::BoxPBC box(10, 10, 10);
        vsepr::Vec3 ri = {0, 0, 0};
        vsepr::Vec3 rj = {9, 0, 0};  // Near edge
        
        vsepr::Vec3 dr = box.delta(ri, rj);
        
        std::cout << "  Box: 10 x 10 x 10" << std::endl;
        std::cout << "  ri = (0, 0, 0)" << std::endl;
        std::cout << "  rj = (9, 0, 0)" << std::endl;
        std::cout << "  Δr = (" << dr.x << ", " << dr.y << ", " << dr.z << ")" << std::endl;
        
        // Expected: dr = {-1, 0, 0} (wrapped to nearest image)
        double error = std::abs(dr.x - (-1.0));
        std::cout << "  Expected: (-1, 0, 0)" << std::endl;
        std::cout << "  Error: " << error << std::endl;
        
        if (error < 1e-10) {
            std::cout << "  ✅ PASS" << std::endl;
        } else {
            std::cout << "  ❌ FAIL" << std::endl;
            return 1;
        }
    }
    
    // Test 2: Force field without PBC (molecule)
    {
        std::cout << "\n[Test 2] Force field without PBC" << std::endl;
        
        vsepr::State state;
        state.N = 2;
        state.X = {{0, 0, 0}, {3, 0, 0}};  // Two atoms 3 Å apart
        state.V.resize(2, {0, 0, 0});
        state.Q.resize(2, 0.0);
        state.M.resize(2, 1.0);
        state.type.resize(2, 1);  // Carbon-like
        state.F.resize(2, {0, 0, 0});
        
        // box is disabled by default
        std::cout << "  PBC enabled: " << (state.box.enabled ? "yes" : "no") << std::endl;
        std::cout << "  Distance: 3 Å" << std::endl;
        
        // Create model and evaluate
        auto model = vsepr::create_lj_coulomb_model();
        vsepr::ModelParams params;
        params.rc = 10.0;
        
        model->eval(state, params);
        
        std::cout << "  Energy: " << state.E.total() << std::endl;
        std::cout << "  Force on atom 0: (" << state.F[0].x << ", " << state.F[0].y << ", " << state.F[0].z << ")" << std::endl;
        
        // Should have attractive force (positive x direction on atom 0)
        if (state.F[0].x > 0) {
            std::cout << "  ✅ PASS (attractive force)" << std::endl;
        } else {
            std::cout << "  ❌ FAIL (wrong force direction)" << std::endl;
            return 1;
        }
    }
    
    // Test 3: Force field with PBC (crystal)
    {
        std::cout << "\n[Test 3] Force field with PBC" << std::endl;
        
        vsepr::State state;
        state.N = 2;
        state.X = {{0, 0, 0}, {9, 0, 0}};  // Two atoms 9 Å apart (raw)
        state.V.resize(2, {0, 0, 0});
        state.Q.resize(2, 0.0);
        state.M.resize(2, 1.0);
        state.type.resize(2, 1);  // Carbon-like
        state.F.resize(2, {0, 0, 0});
        
        // Enable PBC with 10 Å box
        state.box = vsepr::BoxPBC(10, 10, 10);
        
        std::cout << "  PBC enabled: " << (state.box.enabled ? "yes" : "no") << std::endl;
        std::cout << "  Box: 10 x 10 x 10" << std::endl;
        std::cout << "  Raw distance: 9 Å" << std::endl;
        std::cout << "  MIC distance: 1 Å (nearest image)" << std::endl;
        
        // Create model and evaluate
        auto model = vsepr::create_lj_coulomb_model();
        vsepr::ModelParams params;
        params.rc = 10.0;
        
        model->eval(state, params);
        
        std::cout << "  Energy: " << state.E.total() << std::endl;
        std::cout << "  Force on atom 0: (" << state.F[0].x << ", " << state.F[0].y << ", " << state.F[0].z << ")" << std::endl;
        
        // With MIC, atoms are actually 1 Å apart → strong repulsion
        // Force should be negative (push atom 0 in -x direction)
        if (state.F[0].x < 0) {
            std::cout << "  ✅ PASS (repulsive force via MIC)" << std::endl;
        } else {
            std::cout << "  ❌ FAIL (MIC not working)" << std::endl;
            return 1;
        }
    }
    
    // Test 4: PBC vs non-PBC comparison
    {
        std::cout << "\n[Test 4] PBC vs non-PBC force comparison" << std::endl;
        
        // Setup two identical states
        vsepr::State state_no_pbc, state_with_pbc;
        
        for (auto* s : {&state_no_pbc, &state_with_pbc}) {
            s->N = 2;
            s->X = {{0, 0, 0}, {9, 0, 0}};
            s->V.resize(2, {0, 0, 0});
            s->Q.resize(2, 0.0);
            s->M.resize(2, 1.0);
            s->type.resize(2, 1);
            s->F.resize(2, {0, 0, 0});
        }
        
        // Enable PBC only on second state
        state_with_pbc.box = vsepr::BoxPBC(10, 10, 10);
        
        // Evaluate both
        auto model = vsepr::create_lj_coulomb_model();
        vsepr::ModelParams params;
        params.rc = 10.0;
        
        model->eval(state_no_pbc, params);
        model->eval(state_with_pbc, params);
        
        std::cout << "  Without PBC:" << std::endl;
        std::cout << "    Distance: 9 Å" << std::endl;
        std::cout << "    Force: " << state_no_pbc.F[0].x << std::endl;
        
        std::cout << "  With PBC:" << std::endl;
        std::cout << "    MIC distance: 1 Å" << std::endl;
        std::cout << "    Force: " << state_with_pbc.F[0].x << std::endl;
        
        // Forces should be opposite signs (attractive vs repulsive)
        if (state_no_pbc.F[0].x > 0 && state_with_pbc.F[0].x < 0) {
            std::cout << "  ✅ PASS (PBC changes force correctly)" << std::endl;
        } else {
            std::cout << "  ❌ FAIL (PBC not affecting force)" << std::endl;
            return 1;
        }
    }
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    std::cout << "PBC integration verified!" << std::endl;
    
    return 0;
}
