#pragma once
#include "model.hpp"
#include <memory>
#include <vector>

namespace atomistic {

// Evaluates multiple IModel instances and sums forces + energy.
// Bonded terms write Ubond/Uangle/Utors; nonbonded writes UvdW/UCoul.
class CompositeModel : public IModel {
public:
    void add(std::unique_ptr<IModel> m) { models_.push_back(std::move(m)); }

    void eval(State& s, const ModelParams& p) const override
    {
        std::fill(s.F.begin(), s.F.end(), Vec3{0, 0, 0});
        s.E = {};

        for (const auto& m : models_) {
            // Each sub-model writes into a scratch state,
            // then forces and energy are accumulated.
            State tmp = s;           // copy positions, types, bonds, box
            tmp.F.assign(s.N, {0,0,0});
            tmp.E = {};

            m->eval(tmp, p);

            for (uint32_t i = 0; i < s.N; ++i)
                s.F[i] = s.F[i] + tmp.F[i];

            s.E.Ubond  += tmp.E.Ubond;
            s.E.Uangle += tmp.E.Uangle;
            s.E.Utors  += tmp.E.Utors;
            s.E.UvdW   += tmp.E.UvdW;
            s.E.UCoul  += tmp.E.UCoul;
            s.E.Uext   += tmp.E.Uext;
            s.E.Upol   += tmp.E.Upol;
        }
    }

private:
    std::vector<std::unique_ptr<IModel>> models_;
};

// Factory: bonded (from State::B) + LJ nonbonded (with 1-2 exclusions)
std::unique_ptr<IModel> create_composite_model(const State& s);

} // namespace atomistic
