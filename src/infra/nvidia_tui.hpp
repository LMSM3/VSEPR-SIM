#pragma once
/**
 * nvidia_tui.hpp
 * ---------------
 * Secret NVIDIA subsystem monitoring TUI.
 * Accessed via hidden keypress during the VSEPR-MOTD boot sequence.
 *
 * Secret keys (during 2s boot window):
 *   '1'  →  NVIDIA subsystem monitor (live TUI)
 *   '0'  →  list all secret keys
 *
 * The boot window is invisible — no prompt, no cursor change.
 * You either know about it or you don't.
 */

namespace vsepr {
namespace infra {

/**
 * Brief non-blocking input window during boot.
 * Waits up to timeout_ms for a keypress.
 * Returns the key character, or '\0' if nothing was pressed.
 * No visible prompt — truly secret.
 */
char check_secret_keys(int timeout_ms = 2000);

/**
 * Handle a secret key dispatch.
 * Returns true if a known secret key was handled.
 */
bool dispatch_secret_key(char key);

/**
 * Enter the NVIDIA subsystem monitoring TUI.
 * Live-updating dashboard: GPU hardware, processes, VSEPR subsystem.
 * Shells out to nvidia-smi every 2 seconds.
 * Blocks until user presses 'q' or ESC.
 */
void enter_nvidia_tui();

} // namespace infra
} // namespace vsepr
