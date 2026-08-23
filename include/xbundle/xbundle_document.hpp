#pragma once
/**
 * include/xbundle/xbundle_document.hpp
 * =======================================
 * WO-72A  -  .X Bundle Format
 *
 * The .X format is a suite execution container.  One .X file packs one or
 * more .vsim scripts plus optional asset blobs into a single text archive.
 *
 * File layout
 * -----------
 * Line 1:  magic  XBUNDLE <version>
 *          e.g.   XBUNDLE 1
 *
 * [manifest] block  —  suite-level metadata
 *   name         = <string>
 *   description  = <string>          (optional)
 *   author       = <string>          (optional)
 *   created      = <ISO-8601 date>   (optional)
 *   entry_count  = <uint>            (informational; reader re-counts)
 *   entry_point  = <member_name>     (optional; first entry is default)
 *
 * Zero or more [[member]] blocks  —  each embeds one file
 *   name         = <logical name>    (unique within bundle; used as key)
 *   kind         = vsim | asset      (default: vsim)
 *   path         = <original path>   (optional; informational)
 *   size         = <byte count>      (optional; reader validates if present)
 *   >>>
 *   <raw file content, verbatim lines>
 *   <<<
 *
 * Role in the pipeline
 * --------------------
 * | Format | Role                                        |
 * |--------|---------------------------------------------|
 * | .vsim  | Single simulation script                    |
 * | .X     | Bundled suite execution container           |
 * | .dynx  | Post-compiled live visual / session archive |
 *
 * WO-72A | V5.1.4
 */

#include <cstdint>
#include <string>
#include <vector>

namespace vsim {
namespace xbundle {

// ---------------------------------------------------------------------------
// Format version
// ---------------------------------------------------------------------------
inline constexpr int XBUNDLE_FORMAT_VERSION = 1;
inline constexpr const char* XBUNDLE_MAGIC  = "XBUNDLE";

// ---------------------------------------------------------------------------
// Member kind
// ---------------------------------------------------------------------------
enum class XBundleEntryKind {
	vsim,   // embedded .vsim script
	asset   // arbitrary asset blob (text)
};

inline XBundleEntryKind entry_kind_from_string(const std::string& s) {
	if (s == "asset") return XBundleEntryKind::asset;
	return XBundleEntryKind::vsim;
}

inline const char* entry_kind_to_string(XBundleEntryKind k) {
	switch (k) {
		case XBundleEntryKind::asset: return "asset";
		default:                      return "vsim";
	}
}

// ---------------------------------------------------------------------------
// XBundleEntry  —  one embedded member
// ---------------------------------------------------------------------------
struct XBundleEntry {
	std::string      name;               // logical key, unique in bundle
	XBundleEntryKind kind   = XBundleEntryKind::vsim;
	std::string      path;               // original path (informational)
	std::uint64_t    declared_size = 0;  // 0 = not declared
	std::string      content;            // verbatim file content

	bool valid() const { return !name.empty(); }
};

// ---------------------------------------------------------------------------
// XBundleManifest  —  suite-level metadata from [manifest]
// ---------------------------------------------------------------------------
struct XBundleManifest {
	std::string name;
	std::string description;
	std::string author;
	std::string created;
	std::string entry_point;  // logical name of the default member to run
	int         format_version = XBUNDLE_FORMAT_VERSION;

	bool populated = false;
};

// ---------------------------------------------------------------------------
// XBundle  —  top-level in-memory representation of one .X file
// ---------------------------------------------------------------------------
struct XBundle {
	XBundleManifest            manifest;
	std::vector<XBundleEntry>  entries;

	// Source path (set by XBundleReader::read_file)
	std::string source_path;

	// Returns the entry to execute (entry_point name, or first vsim entry)
	const XBundleEntry* entry_point() const {
		if (!entries.empty() && !manifest.entry_point.empty()) {
			for (const auto& e : entries)
				if (e.name == manifest.entry_point)
					return &e;
		}
		for (const auto& e : entries)
			if (e.kind == XBundleEntryKind::vsim)
				return &e;
		return nullptr;
	}

	// Returns a member by logical name, or nullptr
	const XBundleEntry* find(const std::string& name) const {
		for (const auto& e : entries)
			if (e.name == name)
				return &e;
		return nullptr;
	}

	bool valid() const { return manifest.populated && !entries.empty(); }
};

} // namespace xbundle
} // namespace vsim
