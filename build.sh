#!/bin/bash
set -e
bazelisk build //:app
mkdir -p build
cp -f bazel-bin/src/app.elf build/app.elf
cp -f bazel-bin/src/app.uf2 build/app.uf2
echo "Build complete: build/app.elf and build/app.uf2 ready for Wokwi."
