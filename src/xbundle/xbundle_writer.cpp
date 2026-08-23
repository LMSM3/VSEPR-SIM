/**
 * src/xbundle/xbundle_writer.cpp
 * ================================
 * WO-72A  -  .X Bundle Format  —  Writer / Packer implementation
 *
 * Serialises an XBundle to the canonical .X text-archive format.
 *
 * WO-72A | V5.1.4
 */

#include "include/xbundle/xbundle_writer.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace xbundle {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void XBundleWriter::write_file(const XBundle& bundle,
								const std::string& path) {
	const std::string text = write_string(bundle);
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("XBundleWriter: cannot open for write: " + path);
	f << text;
	if (!f)
		throw std::runtime_error("XBundleWriter: write error: " + path);
}

std::string XBundleWriter::write_string(const XBundle& bundle) {
	return serialise(bundle);
}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------

std::string XBundleWriter::serialise(const XBundle& bundle) {
	std::ostringstream o;

	// Magic line
	o << XBUNDLE_MAGIC << " " << bundle.manifest.format_version << "\n";
	o << "\n";

	// [manifest]
	o << "[manifest]\n";
	o << "name         = " << bundle.manifest.name << "\n";
	if (!bundle.manifest.description.empty())
		o << "description  = " << bundle.manifest.description << "\n";
	if (!bundle.manifest.author.empty())
		o << "author       = " << bundle.manifest.author << "\n";
	if (!bundle.manifest.created.empty())
		o << "created      = " << bundle.manifest.created << "\n";
	o << "entry_count  = " << bundle.entries.size() << "\n";
	if (!bundle.manifest.entry_point.empty())
		o << "entry_point  = " << bundle.manifest.entry_point << "\n";
	o << "\n";

	// [[member]] blocks
	for (const auto& e : bundle.entries) {
		o << "[[member]]\n";
		o << "name  = " << e.name << "\n";
		o << "kind  = " << entry_kind_to_string(e.kind) << "\n";
		if (!e.path.empty())
			o << "path  = " << e.path << "\n";
		// Always write size so readers can validate
		o << "size  = " << e.content.size() << "\n";
		o << ">>>\n";
		o << e.content;
		if (!e.content.empty() && e.content.back() != '\n')
			o << "\n";
		o << "<<<\n";
		o << "\n";
	}

	return o.str();
}

} // namespace xbundle
} // namespace vsim
