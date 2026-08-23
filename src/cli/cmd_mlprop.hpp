#pragma once
// WO-72W — mlprop CLI command
#include <vector>
#include <string>

namespace vsepr::cli {

// vsepr mlprop <sub> [args...]
//   recommend <property> <value>   Rank candidates by target property/value
//   trends    <property>           Print feature-property trend analysis
//   list                           List all seed candidates
int cmd_mlprop(const std::vector<std::string>& args);

} // namespace vsepr::cli
