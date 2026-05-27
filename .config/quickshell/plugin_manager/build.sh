#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
BLXSHELL_PATH="${BLXSHELL_PATH:-$HOME/.local/blxshell}"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQS_CONFIG_DIR="$BLXSHELL_PATH"
cmake --build build
echo
echo "Installed to $BLXSHELL_PATH/qml/PluginManager/"
echo
echo "Launch quickshell with:"
echo "  blxshell start"
echo "  (or: QML_IMPORT_PATH=\$BLXSHELL_PATH/qml qs -p \$BLXSHELL_PATH/shell.qml)"
echo
echo "In QML use: import PluginManager"
