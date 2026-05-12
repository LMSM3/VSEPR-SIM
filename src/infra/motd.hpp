#pragma once
/**
 * motd.hpp
 * --------
 * VSEPR-MOTD: banner and report printing for the boot sequence.
 *
 * User-facing name: VSEPR-MOTD
 * Internal namespace: vsepr::infra
 */

#include "infra/bootstrap_probe.hpp"
#include <string>

namespace vsepr {
namespace infra {

/**
 * Print the MOTD banner.
 *   ========================================================
 *    VSEPR-SIM :: MOTD
 *    Deterministic formation engine bootstrap
 *    Mode: Desktop
 Version: v5.0.0
 *   ========================================================
 */
void print_motd_banner(const std::string& mode, const std::string& version);

/**
 * Print a complete MotdReport ([HW], [INFRA], [BOOT] sections).
 */
void print_motd_report(const MotdReport& report);

/**
 * Print the boot-exit line:
 *   [BOOT] Entering desktop environment...
 * or the failure message.
 */
void print_boot_exit(const MotdReport& report, const std::string& mode);

} // namespace infra
} // namespace vsepr
