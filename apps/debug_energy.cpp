/**
 * Minimal diagnostic: does LJCoulomb::eval produce nonzero energy for H2O?
 */
#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include <iostream>
#include <iomanip>
#include <memory>

int main() {
    using namespace atomistic;

    // Create H2O state manually
    State s;
    s.N = 3;
    s.X = {
        {0.0, 0.0, 0.0},   // O
        {0.96, 0.0, 0.0},  // H
        {-0.24, 0.93, 0.0} // H
    };
    s.type = {8, 1, 1};  // O, H, H
    s.V.resize(3, {0,0,0});
    s.M.resize(3, 1.0);
    s.F.resize(3, {0,0,0});
    s.Q = {0.0, 0.0, 0.0};

    std::cout << "State: N=" << s.N
              << " X.size=" << s.X.size()
              << " type.size=" << s.type.size()
              << " F.size=" << s.F.size()
              << " Q.size=" << s.Q.size()
              << " sane=" << sane(s) << "\n";

    // Create model
    auto model = create_lj_coulomb_model();

    // Set up params
    ModelParams p;
    p.rc = 10.0;
    p.k_coul = 138.935;

    std::cout << "Params: rc=" << p.rc << " k_coul=" << p.k_coul << "\n";

    // Evaluate
    model->eval(s, p);

    // Print results
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Energy terms:\n";
    std::cout << "  UvdW  = " << s.E.UvdW << "\n";
    std::cout << "  UCoul = " << s.E.UCoul << "\n";
    std::cout << "  Ubond = " << s.E.Ubond << "\n";
    std::cout << "  total = " << s.E.total() << "\n";

    std::cout << "Forces:\n";
    for (uint32_t i = 0; i < s.N; i++) {
        std::cout << "  F[" << i << "] = ("
                  << s.F[i].x << ", "
                  << s.F[i].y << ", "
                  << s.F[i].z << ")\n";
    }

    // Also check distances
    for (uint32_t i = 0; i < s.N; i++) {
        for (uint32_t j = i+1; j < s.N; j++) {
            Vec3 dr = s.X[i] - s.X[j];
            double r = std::sqrt(dot(dr, dr));
            std::cout << "  r(" << i << "," << j << ") = " << r << " A\n";
        }
    }

    return 0;
}
