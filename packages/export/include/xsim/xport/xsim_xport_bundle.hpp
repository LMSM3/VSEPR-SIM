#pragma once
/**
 * xsim_xport_bundle.hpp
 * =====================
 * ExportBundle — assembled artifact of a completed export run.
 *
 * Carries the job that drove the export, the result it produced,
 * and the manifest that inventories the output artifacts.
 *
 * namespace xsim::xport
 */

#include "xsim_xport_job.hpp"
#include "xsim_xport_result.hpp"
#include "xsim_xport_manifest.hpp"

namespace xsim::xport {

struct ExportBundle {
	ExportJob      job;
	ExportResult   result;
	ExportManifest manifest;
};

} // namespace xsim::xport
