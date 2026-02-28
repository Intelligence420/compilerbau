#!/usr/bin/env bash

# Paths to tools
CIVCC=./build-debug/civicc
CIVAS=${HOME}/toolchain-linux-x64-static/bin/civas
CIVVM=${HOME}/toolchain-linux-x64-static/bin/civvm

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo "Running Codegen Tests..."
echo "--------------------------"

failed=0
total=0

cd test/codegen_tests/functional

for f in *.cvc; do
    total=$((total+1))
    name=${f%.cvc}
    echo -n "Testing $name: "
    
    # 1. Compile
    if ! ../../../$CIVCC -o $name.s $f > /dev/null 2>&1; then
        echo -e "${RED}FAILED (Compiling)${NC}"
        failed=$((failed+1))
        continue
    fi
    
    # 2. Assemble
    if ! $CIVAS $name.s -o $name.o > /dev/null 2>&1; then
        echo -e "${RED}FAILED (Assembling)${NC}"
        failed=$((failed+1))
        continue
    fi
    
    # 3. Run
    $CIVVM $name.o > $name.res 2>&1
    
    # 4. Compare (if .out exists)
    if [ -f $name.out ]; then
        if diff -u $name.out $name.res > /dev/null 2>&1; then
            echo -e "${GREEN}SUCCESS${NC}"
        else
            echo -e "${RED}FAILED (Output Mismatch)${NC}"
            diff -u $name.out $name.res
            failed=$((failed+1))
        fi
    else
        echo -e "${GREEN}GENERATED OUTPUT${NC}"
        mv $name.res $name.out
    fi
    
    # Cleanup
    rm -f $name.s $name.o $name.res
done

echo "--------------------------"
echo "Total: $total, Failed: $failed"

if [ $failed -eq 0 ]; then
    exit 0
else
    exit 1
fi
