#!/bin/bash
# SLUB 快速测试脚本
# 学号：2312325

cd /mnt/d/gds/Documents/Operating_system/labcode/lab2

# 激活环境
source /mnt/d/gds/Documents/Operating_system/riscv_isolated/scripts/activate_riscv_env.sh

# 重新编译
echo "=========================================="
echo "Recompiling..."
echo "=========================================="
make

echo "=========================================="
echo "Running SLUB Test - Student ID: 2312325"
echo "=========================================="

# 运行 QEMU，30秒后自动退出
timeout --foreground 30s qemu-system-riscv64 \
    -machine virt \
    -nographic \
    -bios default \
    -device loader,file=bin/ucore.img,addr=0x80200000 \
    2>&1 | tee full_test.log

echo ""
echo "=========================================="
echo "Test completed. Check full_test.log"
echo "=========================================="
