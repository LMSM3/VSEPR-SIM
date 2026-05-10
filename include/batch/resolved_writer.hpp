#pragma once
/**
 * include/batch/resolved_writer.hpp
 * ====================================
 * WO-VSIM-62C — run.vsim.resolved Writer
 *
 * Writes a self-contained, human-readable VSIM script to
 * cases/<run_id>/run.vsim.resolved.
 *
 * The file contains a header comment block identifying the run
 * followed by all resolved section fields in canonical VSIM format.
 *
 * Invariant: `vsper validate <path>` must succeed without access to
 * the original study file or template.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/vsim/vsim_document.hpp"
#include "include/batch/batch_expander.hpp"
#include <string>

namespace vsim {
namespace batch {

class ResolvedWriter {
public:
	static void write(const VsimDocument& resolved_doc,
					  const BatchRunSpec&  spec,
					  const std::string&   study_name,
					  const std::string&   output_path);
};

} // namespace batch
} // namespace vsim
