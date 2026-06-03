#!/bin/bash
# Build script for Parallels VM where host CPU features aren't available to the guest
export CMAKE_EXTRA_ARGS="-DMT_GGML_NATIVE=OFF"
exec "$(dirname "$0")/build-linux.sh"
