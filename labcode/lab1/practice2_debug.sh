#!/bin/bash
# 同步调试脚本 - 避免后台进程问题
set -euo pipefail

cd "$(dirname "$0")"
OUTPUT="practice2_debug_result.txt"

echo "========== Lab1 练习2: GDB 验证启动流程 ==========" | tee "$OUTPUT"
echo "实验时间: $(date)" | tee -a "$OUTPUT"
echo "" | tee -a "$OUTPUT"

# GDB 命令
cat > gdb_sync.cmd <<'GDBEOF'
set pagination off
set confirm off
file bin/kernel
set arch riscv:rv64
target remote :1234

printf "\n========== 阶段1: 复位向量地址 ==========\n"
info registers pc
printf "\n复位向量处前10条指令:\n"
x/10i $pc

printf "\n========== 单步执行观察 ==========\n"
si
printf "步骤1: PC = 0x%lx\n", $pc
x/3i $pc
si
printf "步骤2: PC = 0x%lx\n", $pc
x/3i $pc
si
printf "步骤3: PC = 0x%lx\n", $pc
info registers t0 a0 a1

printf "\n========== 设置内核入口断点并继续 ==========\n"
b *0x80200000
continue

printf "\n========== 阶段3: 内核入口点 0x80200000 ==========\n"
info registers pc sp ra a0 a1
printf "\n内核入口指令:\n"
x/12i 0x80200000

quit
GDBEOF

# 启动 QEMU  在后台，获取 PID
echo "[INFO] 启动 QEMU (QEMU 4.1.1)..." | tee -a "$OUTPUT"
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios default \
  -kernel bin/kernel \
  -s -S \
  >> "$OUTPUT" 2>&1 &

QEMU_PID=$!
echo "[INFO] QEMU PID: $QEMU_PID" | tee -a "$OUTPUT"

# 等待 QEMU gdbstub 就绪
sleep 4

# 检查 QEMU 是否还在运行
if ! kill -0 $QEMU_PID 2>/dev/null; then
  echo "[ERROR] QEMU 已退出" | tee -a "$OUTPUT"
  exit 1
fi

echo "[INFO] 连接 GDB..." | tee -a "$OUTPUT"

# 运行 GDB
riscv64-unknown-elf-gdb -batch -x gdb_sync.cmd >> "$OUTPUT" 2>&1

# 终止 QEMU
kill -9 $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo "" | tee -a "$OUTPUT"
echo "========== 调试完成 ==========" | tee -a "$OUTPUT"
echo "结果已保存到: $OUTPUT" | tee -a "$OUTPUT"

# 显示结果
cat "$OUTPUT"
