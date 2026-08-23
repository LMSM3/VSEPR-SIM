#pragma once
// WO-67N DEM Bridge validate + export API  (Phase 1)
#include "vsim/objects/bridge_objects.hpp"
#include <string>
#include <vector>
namespace vsim {
struct VsimDocument;
struct DEMBridgeDiagnostic { int code; std::string level; std::string message; int source_line=0; };
std::vector<DEMBridgeDiagnostic> validate_dem_bridge(const DEMBridgeObject& dem, const VsimDocument& doc);
void export_dem_bridge(const DEMBridgeObject& dem, const VsimDocument& doc, const std::string& output_dir);
void execute_dem_bridges(const VsimDocument& doc, const std::string& output_dir);
} // namespace vsim