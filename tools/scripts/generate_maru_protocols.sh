#!/bin/bash
set -e

PROTO_DIR="src/core/linux/wayland/protocols"
XML_DIR="$PROTO_DIR/xml"
GEN_DIR="$PROTO_DIR/generated"

# Check if wayland-scanner is available
if ! command -v wayland-scanner &> /dev/null; then
    echo "Error: wayland-scanner not found in PATH."
    exit 1
fi

mkdir -p "$GEN_DIR"

# Helper function
gen_proto() {
    local xml="$1"
    local base=$(basename "$xml" .xml)
    
    echo "Generating protocols for $base..."
    
    wayland-scanner client-header "$xml" "$GEN_DIR/$base-client-protocol.h"
    wayland-scanner private-code "$xml" "$GEN_DIR/$base-client-protocol.inc.h"

    python3 tools/scripts/generate_maru_wayland_helpers.py "$xml" "$GEN_DIR/maru-$base-helpers.h"
}

# Generate for selected XML files
gen_proto "$XML_DIR/wayland.xml"
gen_proto "$XML_DIR/xdg-shell.xml"
gen_proto "$XML_DIR/xdg-decoration-unstable-v1.xml"

echo "Done."
