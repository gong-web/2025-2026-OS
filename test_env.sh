#!/bin/bash

# 测试环境配置脚本

echo "==================================="
echo "测试操作系统实验环境"
echo "==================================="
echo ""

# 首先激活环境
echo ">>> 激活环境配置..."
source setup_os_env.sh

echo ""
echo "==================================="
echo "环境变量检查"
echo "==================================="
echo "RISCV = $RISCV"
echo "QEMU_HOME = $QEMU_HOME"
echo ""

echo "==================================="
echo "工具验证"
echo "==================================="
echo ""

# 测试 RISC-V GCC
echo ">>> 测试 RISC-V GCC"
if command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo "✓ riscv64-unknown-elf-gcc 找到:"
    riscv64-unknown-elf-gcc --version | head -n 1
    echo "  路径: $(which riscv64-unknown-elf-gcc)"
else
    echo "✗ riscv64-unknown-elf-gcc 未找到"
fi

echo ""

# 测试 QEMU
echo ">>> 测试 QEMU RISC-V 64"
if command -v qemu-system-riscv64 &> /dev/null; then
    echo "✓ qemu-system-riscv64 找到:"
    qemu-system-riscv64 --version | head -n 1
    echo "  路径: $(which qemu-system-riscv64)"
else
    echo "✗ qemu-system-riscv64 未找到"
fi

echo ""

# 测试编译一个简单的 C 程序
echo ">>> 测试编译简单程序"
cat > /tmp/test_riscv.c << 'EOF'
#include <stdio.h>

int main() {
    printf("Hello, RISC-V!\n");
    return 0;
}
EOF

if riscv64-unknown-elf-gcc /tmp/test_riscv.c -o /tmp/test_riscv 2>/dev/null; then
    echo "✓ 编译测试成功!"
    echo "  生成的文件: /tmp/test_riscv"
    file /tmp/test_riscv | grep -i riscv && echo "✓ 确认为 RISC-V 64位 可执行文件"
    rm -f /tmp/test_riscv /tmp/test_riscv.c
else
    echo "✗ 编译测试失败"
    rm -f /tmp/test_riscv.c
fi

echo ""
echo "==================================="
echo "环境测试完成!"
echo "==================================="
echo ""

# 检查其他必要工具
echo ">>> 其他工具检查"
for tool in make git; do
    if command -v $tool &> /dev/null; then
        echo "✓ $tool 已安装"
    else
        echo "✗ $tool 未安装 (建议安装: sudo apt install $tool)"
    fi
done

echo ""
echo "如果所有测试都通过,你可以开始实验了!"
echo "使用方法: source setup_os_env.sh"
