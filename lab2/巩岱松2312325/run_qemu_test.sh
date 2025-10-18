#!/bin/bash
# 运行QEMU SLUB测试脚本

cd /mnt/d/gds/Documents/Operating_system/lab2
source /mnt/d/gds/Documents/Operating_system/riscv_isolated/scripts/activate_riscv_env.sh

echo "Starting QEMU SLUB test..."
echo "========================================="

# 使用timeout命令限制运行时间，并将输出保存到文件
timeout 60s make qemu > qemu_full_output.log 2>&1 &
QEMU_PID=$!

# 等待一段时间让测试运行
sleep 30

# 检查进程是否还在运行
if kill -0 $QEMU_PID 2>/dev/null; then
    echo "Stopping QEMU..."
    kill $QEMU_PID
    wait $QEMU_PID 2>/dev/null
fi

echo "========================================="
echo "Test completed. Output saved to qemu_full_output.log"

# 显示输出内容
if [ -f qemu_full_output.log ]; then
    echo "Test output:"
    cat qemu_full_output.log
fi