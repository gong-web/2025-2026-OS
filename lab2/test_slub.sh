#!/bin/bash
# SLUB 测试脚本
# 学号：2312325 巩岱松
# 日期：2025年10月11日

echo "========================================="
echo "SLUB 内存分配器测试脚本"
echo "学号：2312325"
echo "========================================="
echo ""

# 检查 RISC-V 工具链
echo "[1] 检查 RISC-V 工具链..."
if command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo "✓ 找到 riscv64-unknown-elf-gcc"
    riscv64-unknown-elf-gcc --version | head -1
elif command -v riscv64-linux-gnu-gcc &> /dev/null; then
    echo "✓ 找到 riscv64-linux-gnu-gcc"
    echo "  注意：需要修改 Makefile 中的 GCCPREFIX"
    riscv64-linux-gnu-gcc --version | head -1
else
    echo "✗ 未找到 RISC-V 工具链"
    echo ""
    echo "请安装 RISC-V 工具链："
    echo "  Ubuntu/Debian: sudo apt-get install gcc-riscv64-unknown-elf"
    echo "  或从源码编译: https://github.com/riscv-collab/riscv-gnu-toolchain"
    exit 1
fi
echo ""

# 检查 QEMU
echo "[2] 检查 QEMU..."
if command -v qemu-system-riscv64 &> /dev/null; then
    echo "✓ 找到 qemu-system-riscv64"
    qemu-system-riscv64 --version | head -1
else
    echo "✗ 未找到 qemu-system-riscv64"
    echo ""
    echo "请安装 QEMU："
    echo "  Ubuntu/Debian: sudo apt-get install qemu-system-misc"
    exit 1
fi
echo ""

# 检查 SLUB 文件
echo "[3] 检查 SLUB 源文件..."
FILES=(
    "kern/mm/slub.h"
    "kern/mm/slub.c"
    "kern/mm/slub_test.c"
)

ALL_EXIST=true
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "✓ $file"
    else
        echo "✗ $file (缺失)"
        ALL_EXIST=false
    fi
done

if [ "$ALL_EXIST" = false ]; then
    echo ""
    echo "错误：缺少必要的 SLUB 源文件"
    exit 1
fi
echo ""

# 清理
echo "[4] 清理旧的编译产物..."
make clean > /dev/null 2>&1
echo "✓ 清理完成"
echo ""

# 编译
echo "[5] 编译 uCore with SLUB..."
echo "    (这可能需要几分钟...)"
if make > build.log 2>&1; then
    echo "✓ 编译成功"
    echo ""
    echo "生成的文件:"
    ls -lh bin/kernel bin/ucore.img 2>/dev/null
else
    echo "✗ 编译失败"
    echo ""
    echo "错误信息（最后 20 行）:"
    tail -20 build.log
    echo ""
    echo "完整日志保存在: build.log"
    exit 1
fi
echo ""

# 运行测试
echo "[6] 运行 SLUB 测试..."
echo "    按 Ctrl+A 然后 X 退出 QEMU"
echo ""
echo "========================================="
echo "启动 QEMU..."
echo "========================================="
echo ""

# 运行 QEMU 并捕获输出
timeout 30s make qemu 2>&1 | tee qemu.log

echo ""
echo "========================================="
echo "测试完成"
echo "========================================="
echo ""

# 分析输出
if grep -q "SLUB.*initialized" qemu.log 2>/dev/null; then
    echo "✓ SLUB 初始化成功"
fi

if grep -q "All tests PASSED" qemu.log 2>/dev/null; then
    echo "✓ 所有测试通过"
elif grep -q "TEST.*PASS" qemu.log 2>/dev/null; then
    PASS_COUNT=$(grep -c "PASS" qemu.log)
    echo "✓ 通过 $PASS_COUNT 个测试"
fi

if grep -q "FAIL\|ERROR\|panic" qemu.log 2>/dev/null; then
    echo "✗ 发现错误，请查看 qemu.log"
fi

echo ""
echo "日志文件:"
echo "  - build.log : 编译日志"
echo "  - qemu.log  : 运行日志"
echo ""
