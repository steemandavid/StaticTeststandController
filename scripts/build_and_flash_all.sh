#!/bin/bash
#
# Parallel Build and Flash Script for Static Test Stand Controller
# Builds and flashes both BASE and REMOTE units concurrently
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Project directory
PROJECT_DIR="/home/john/automaker/Static Test Stand Controller"

# ESP-IDF environment
source ~/esp/esp-idf/export.sh

echo "========================================================================"
echo "  Building and Flashing BASE and REMOTE in Parallel"
echo "========================================================================"
echo ""

# Function to flash BASE
flash_base() {
    echo "[BASE] Building..."
    cd "$PROJECT_DIR"

    if idf.py -D BUILD_TARGET=BASE build 2>&1 | tee /tmp/base_build.log | tail -5; then
        echo -e "${GREEN}[BASE] Build successful${NC}"

        echo "[BASE] Flashing to /dev/ttyACM0..."
        if idf.py -D BUILD_TARGET=BASE -p /dev/ttyACM0 flash 2>&1 | tee /tmp/base_flash.log | tail -5; then
            echo -e "${GREEN}[BASE] ✓ Flashed successfully${NC}"
            return 0
        else
            echo -e "${RED}[BASE] ✗ Flash failed${NC}"
            return 1
        fi
    else
        echo -e "${RED}[BASE] ✗ Build failed${NC}"
        return 1
    fi
}

# Function to flash REMOTE
flash_remote() {
    echo "[REMOTE] Building..."
    cd "$PROJECT_DIR"

    if idf.py -D BUILD_TARGET=REMOTE build 2>&1 | tee /tmp/remote_build.log | tail -5; then
        echo -e "${GREEN}[REMOTE] Build successful${NC}"

        echo "[REMOTE] Flashing to /dev/ttyACM1..."
        if idf.py -D BUILD_TARGET=REMOTE -p /dev/ttyACM1 flash 2>&1 | tee /tmp/remote_flash.log | tail -5; then
            echo -e "${GREEN}[REMOTE] ✓ Flashed successfully${NC}"
            return 0
        else
            echo -e "${RED}[REMOTE] ✗ Flash failed${NC}"
            return 1
        fi
    else
        echo -e "${RED}[REMOTE] ✗ Build failed${NC}"
        return 1
    fi
}

# Run both in parallel
echo "Starting parallel build and flash..."
echo ""

flash_base &
BASE_PID=$!

flash_remote &
REMOTE_PID=$!

# Wait for both
wait $BASE_PID
BASE_STATUS=$?

wait $REMOTE_PID
REMOTE_STATUS=$?

echo ""
echo "========================================================================"
echo "  Summary"
echo "========================================================================"

if [ $BASE_STATUS -eq 0 ]; then
    echo -e "BASE:   ${GREEN}✓ SUCCESS${NC}"
else
    echo -e "BASE:   ${RED}✗ FAILED${NC}"
fi

if [ $REMOTE_STATUS -eq 0 ]; then
    echo -e "REMOTE: ${GREEN}✓ SUCCESS${NC}"
else
    echo -e "REMOTE: ${RED}✗ FAILED${NC}"
fi

echo "========================================================================"

# Exit with error if either failed
if [ $BASE_STATUS -ne 0 ] || [ $REMOTE_STATUS -ne 0 ]; then
    exit 1
fi

exit 0
