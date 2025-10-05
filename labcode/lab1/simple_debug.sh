#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
source ../../riscv_isolated/scripts/activate_riscv_env.sh

echo "[INFO] 启动 QEMU (后台)..."
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios default \
  -kernel bin/kernel \
  -s -S \
  > qemu_session.log 2>&1 &

QEMU_PID=$!
echo "[INFO] QEMU PID: $QEMU_PID"
sleep 3

echo "[INFO] 启动 GDB 调试..."
riscv64-unknown-elf-gdb -batch -x manual_debug.gdb > manual_debug_output.txt 2>&1

echo "[INFO] 终止 QEMU..."
kill -9 $QEMU_PID 2>/dev/null || true

echo "[INFO] 调试完成，结果保存到 manual_debug_output.txt"
cat manual_debug_output.txt
