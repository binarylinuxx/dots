#!/bin/sh
# Built with blxshell's own build (CMake target blxshell-native-example), so
# the ABI tag matches. Standalone, against an installed blxshell and vw:
#   clang++ -std=c++23 -O2 -fPIC -shared src/plugin.cpp -o lib/plugin.so \
#     $(pkg-config --cflags vw) -I"$PREFIX/include"
mkdir -p lib
clang++ -std=c++23 -O2 -fPIC -shared src/plugin.cpp -o lib/plugin.so $(pkg-config --cflags vw 2>/dev/null) ${BLXSHELL_INCLUDE:+-I"$BLXSHELL_INCLUDE"}
