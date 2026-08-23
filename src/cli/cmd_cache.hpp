#pragma once
// WO-72V — cache CLI command
#include <vector>
#include <string>

namespace vsepr::cli {

// vsepr cache <sub>
//   list-presets [<formula>]   — show material preset(s)
//   list-routes  <formula>     — show formation routes for a product
//   list-props   [<formula>]   — show property table entry/entries
//   index-status               — print trajectory index size
int cmd_cache(const std::vector<std::string>& args);

} // namespace vsepr::cli
