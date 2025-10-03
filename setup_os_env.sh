#!/bin/bash

# 操作系统课程 - 独立环境配置脚本
# 使用方法: source setup_os_env.sh

# 保存原始环境变量(如果需要恢复)
if [ -z "$ORIGINAL_PATH" ]; then
    export ORIGINAL_PATH="$PATH"
fi

# 设置 RISC-V 工具链路径 (已配置为实际安装路径)
export RISCV=/mnt/d/gds/Documents/Operating_system/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14

# 设置 QEMU 路径
export QEMU_HOME=/opt/qemu

# 将 RISC-V 工具链和 QEMU 添加到 PATH (优先级高于系统路径)
export PATH=$RISCV/bin:$QEMU_HOME/bin:$ORIGINAL_PATH

# 设置工作目录
export OS_LAB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 验证工具链是否正确配置
echo "==================================="
echo "操作系统课程环境配置"
echo "==================================="
echo "RISCV 工具链路径: $RISCV"
echo "OS 实验目录: $OS_LAB_DIR"
echo ""

# 检查 RISC-V GCC 是否可用
if command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo "✓ RISC-V GCC 已找到:"
    riscv64-unknown-elf-gcc --version | head -n 1
else
    echo "✗ 错误: 未找到 riscv64-unknown-elf-gcc"
    echo "  请检查 RISCV 路径是否正确设置"
fi

echo ""

# 检查 QEMU 是否可用
if command -v qemu-system-riscv64 &> /dev/null; then
    echo "✓ QEMU RISC-V 已找到:"
    qemu-system-riscv64 --version | head -n 1
else
    echo "✗ 警告: 未找到 qemu-system-riscv64"
    echo "  请确保已安装 QEMU"
fi

echo "==================================="
echo ""
echo "环境已激活!"
echo "使用 'deactivate_os_env' 命令可以恢复原始环境"
echo ""

# 提供一个恢复原始环境的函数
deactivate_os_env() {
    if [ -n "$ORIGINAL_PATH" ]; then
        export PATH="$ORIGINAL_PATH"
        echo "已恢复原始 PATH 环境变量"
    fi
    unset RISCV
    unset OS_LAB_DIR
    unset -f deactivate_os_env
}

export -f deactivate_os_env
