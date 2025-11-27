#!/bin/bash
source /mnt/d/gds/Documents/Operating_system/riscv_isolated/scripts/activate_riscv_env.sh
cd /mnt/d/gds/Documents/2025-2026-OS/lab5/lab5
make clean
make
make grade
