#pragma once
/**
 * include/xbundle/xbundle_writer.hpp
 * =====================================
 * WO-72A  -  .X Bundle Format  —  Writer / Packer
 *
 * Serialises an XBundle to the .X text-archive format.
 *
 * Usage:
 *   XBundleWriter::write_file(bundle, "suite.X");
 *   std::string text = XBundleWriter::write_string(bundle);
 *
 * WO-72A | V5.1.4
 */

#include "include/xbundle/xbundle_document.hpp"
#include <string>

namespace vsim {
namespace xbundle {

class XBundleWriter {
public:
	static void        write_file  (const XBundle& bundle,
									const std::string& path);
	static std::string write_string(const XBundle& bundle);

private:
	static std::string serialise(const XBundle& bundle);
};

} // namespace xbundle
} // namespace vsim
