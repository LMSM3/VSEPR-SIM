#include "composite.hpp"
#include "bonded.hpp"

namespace atomistic {

std::unique_ptr<IModel> create_composite_model(const State& s)
{
    auto comp = std::make_unique<CompositeModel>();

    // Layer 1: bonded terms (only if bonds are present)
    if (!s.B.empty())
        comp->add(create_generic_bonded_model(s));

    // Layer 2: LJ nonbonded (with 1-2 exclusions from State::B)
    comp->add(create_lj_coulomb_model());

    return comp;
}

} // namespace atomistic
