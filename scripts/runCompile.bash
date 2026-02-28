#!/usr/bin/env bash

if [ $# -lt 2 ]; then
    echo "Verwendung: $0 <arg1> <arg2>"
    exit 1
fi

echo "Kompilieren..."
make -C build || exit 1

echo "Run civicc"
./build/civicc $1 -o $2 || exit 1

echo "Run civas"
./test/compile/vm/civas $2 || exit 1

echo "Run civvm"
./test/compile/vm/civvm civ.out || exit 1