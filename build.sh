#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Build Script (wrapper)
# Thin wrapper that calls the canonical build script.
# All build logic is in scripts/build/build.sh
################################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANONICAL_SCRIPT="$SCRIPT_DIR/scripts/build/build.sh"

if [ ! -f "$CANONICAL_SCRIPT" ]; then
    echo "Error: Canonical build script not found: $CANONICAL_SCRIPT"
    exit 1
fi

exec bash "$CANONICAL_SCRIPT" "$@"
