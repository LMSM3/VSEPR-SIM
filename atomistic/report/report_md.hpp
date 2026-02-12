#pragma once
#include "../core/state.hpp"
#include "../integrators/fire.hpp"
#include <string>

namespace atomistic {

std::string fire_report_md(const State& s, const FIREStats& st);

} // namespace atomistic
