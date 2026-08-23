#pragma once
// WO-67O FEA Bridge validate + export API  (Phase 1)
#include "vsim/objects/bridge_objects.hpp"
#include <string>
#include <vector>
namespace vsim {
struct VsimDocument;
struct FEABridgeDiagnostic { int code; std::string level; std::string message; int source_line=0; };
std::vector<FEABridgeDiagnostic> validate_fea_bridge(const FEABridgeObject& fea, const VsimDocument& doc);
void export_fea_bridge(const FEABridgeObject& fea, const VsimDocument& doc, const std::string& output_dir);
void execute_fea_bridges(const VsimDocument& doc, const std::string& output_dir);
} // namespace vsim