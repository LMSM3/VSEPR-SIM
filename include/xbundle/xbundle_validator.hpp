#pragma once
/**
 * include/xbundle/xbundle_validator.hpp
 * ========================================
 * WO-72A  -  .X Bundle Format  —  Validator
 *
 * Validates a parsed XBundle and returns a list of error strings.
 * An empty list means the bundle is valid.
 *
 * Rules enforced:
 *   V-01  manifest.populated must be true
 *   V-02  manifest.name must not be empty
 *   V-03  At least one entry must exist
 *   V-04  Every entry must have a non-empty name
 *   V-05  Entry names must be unique within the bundle
 *   V-06  Every vsim entry must have non-empty content
 *   V-07  entry_point (if set) must name an existing entry
 *   V-08  declared_size (if non-zero) must match actual content byte count
 *
 * WO-72A | V5.1.4
 */

#include "include/xbundle/xbundle_document.hpp"
#include <string>
#include <vector>

namespace vsim {
namespace xbundle {

class XBundleValidator {
public:
	static std::vector<std::string> validate(const XBundle& bundle);
};

} // namespace xbundle
} // namespace vsim
