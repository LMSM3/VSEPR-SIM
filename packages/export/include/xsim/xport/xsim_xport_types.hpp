#pragma once
/**
 * xsim_xport_types.hpp
 * ====================
 * Forward declarations for all xsim::xport types.
 * Include this to name types without pulling in full definitions.
 *
 * namespace xsim::xport
 */

namespace xsim::xport {

struct ExportConfig;
struct ExportJob;
struct ExportResult;
struct ExportManifest;
struct ExportBundle;
struct VerifyResult;

class IWriter;
class WriterRegistry;
class ExportPackage;
class ExportXParser;
class XportVerifier;

} // namespace xsim::xport
