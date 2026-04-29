/**
 * vsepr_live.cpp
 * --------------
 * Standalone zero-input launcher for the VSEPR-SIM Live Analysis Server.
 *
 * Starts on port 99998 with absolutely zero configuration required.
 * Just run: ./vsepr_live
 *
 * Opens an HTTP server that streams random gas-phase molecular analysis
 * as SSE events, JSON snapshots, and an auto-updating HTML dashboard.
 */

#include "core/live_server.hpp"

int main(int argc, char** argv) {
    return vsepr::live::serve_dispatch(argc, argv);
}
