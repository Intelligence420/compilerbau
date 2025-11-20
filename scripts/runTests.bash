#!/usr/bin/env bash

set -euo pipefail

make -C build-debug

for file in test/*; do
    echo ""
    echo "--------------------"
    [ -f "$file" ] || continue
    echo "Ausführen: $file"
    ./build-debug/civicc "$file"
done
