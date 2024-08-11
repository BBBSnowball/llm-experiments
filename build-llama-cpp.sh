#!/bin/sh
set -xeo pipefail
cd "$(dirname "$0")/llama.cpp"
cmake -B build $CMAKE_ARGS
cmake --build build --config Release
# or: cmake --build build --config Release --clean-first

