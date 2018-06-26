#!/bin/sh
set -e
mkdir -p build
cd build

# Prepare makefile(s)
cmake ..

# Build
make
