#!/usr/bin/env bash

if [ $# -lt 1 ]; then
    echo "Verwendung: $0 <arg1>"
    exit 1
fi

# Betriebssystem erkennen
if [[ "$(uname)" == "Darwin" ]]; then
    CIVAS="./test/compile/vm/macos/civas"
    CIVVM="./test/compile/vm/macos/civvm"
else
    CIVAS="./test/compile/vm/linux/civas"
    CIVVM="./test/compile/vm/linux/civvm"
fi

echo "Kompilieren..."
make -C build-debug  || exit 1

echo "Run civicc"
./build-debug/civicc $1 -o test/compile/out || exit 1

echo "Run civas"
$CIVAS test/compile/out || exit 1

echo "Run civvm"
$CIVVM civ.out  || exit 1