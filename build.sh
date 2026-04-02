#!/bin/bash

echo "=== Removing old caches"
rm -rf ./build
rm -rf ./exec

echo "=== Building engine & app"
cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build && cmake --build build