#!/bin/bash

# ucore PoC Suite Runner

# Try to activate environment if toolchain is missing
if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
    if [ -f "/mnt/d/gds/Documents/Operating_system/riscv_isolated/scripts/activate_riscv_env.sh" ]; then
        echo "Activating RISC-V environment..."
        source "/mnt/d/gds/Documents/Operating_system/riscv_isolated/scripts/activate_riscv_env.sh"
    elif [ -f "../../labcode/env/activate_os_env.sh" ]; then
         echo "Activating local OS environment..."
         source "../../labcode/env/activate_os_env.sh"
    fi
fi

LOG_FILE="poc_suite.log"
UCORE_IMG="bin/ucore.img"
SWAP_IMG="bin/swap.img"

# Ensure bin directory exists
mkdir -p bin

# Create swap image if it doesn't exist
if [ ! -f "$SWAP_IMG" ]; then
    echo "Creating dummy swap image..."
    dd if=/dev/zero of="$SWAP_IMG" bs=1M count=128 2>/dev/null
fi

echo "Building and Running PoC Suite..." | tee $LOG_FILE

# Compile kernel with poc_suite
make build-poc_suite > build_poc_suite.log 2>&1

if [ $? -ne 0 ]; then
    echo "Build failed! Check build_poc_suite.log" | tee -a $LOG_FILE
    exit 1
fi

# Run QEMU
timeout --foreground 60s qemu-system-riscv64 \
    -machine virt \
    -nographic \
    -bios default \
    -device loader,file=$UCORE_IMG,addr=0x80200000 \
    -drive file=$SWAP_IMG,media=disk,cache=writeback \
    | tee -a $LOG_FILE

echo "Done."
