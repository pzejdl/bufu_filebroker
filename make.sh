#!/bin/sh
set -e

mkdir -p build
cd build

# Prepare makefiles
cmake ../src

# Building
make
