#pragma once
// =============================================================================
// src/batch/manifest_loader.hpp  -  WO-B9-001  Batch Manifest Loader
// =============================================================================
//
// JSON loader and validator for batch_manifest.json.
// Depends on BatchManifestSection / BatchManifestSweepAxis declared in
// include/vsim/vsim_document.hpp.
//
// WO-B9-001  |  VSEPR-SIM beta-9
// =============================================================================

#include "include/vsim/vsim_document.hpp"
#include <string>
#include <vector>

namespace vsim {
namespace batch {

// ---------------------------------------------------------------------------
// ManifestValidation  -  result of validate_manifest()
// ---------------------------------------------------------------------------

struct ManifestValidation {
    bool                     ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ---------------------------------------------------------------------------
// load_manifest   -  populate `out` from JSON file at `path`
//                    returns false and sets `error_msg` on parse failure
// ---------------------------------------------------------------------------

bool load_manifest(const std::string& path,
                   BatchManifestSection& out,
                   std::string& error_msg);

// ---------------------------------------------------------------------------
// validate_manifest  -  check required fields and value constraints
// ---------------------------------------------------------------------------

ManifestValidation validate_manifest(const BatchManifestSection& m);

} // namespace batch
} // namespace vsim
