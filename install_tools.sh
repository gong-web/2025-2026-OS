#!/bin/bash

# 操作系统课程 - 工具安装脚本
# 此脚本用于在 WSL Ubuntu 中安装必要的工具

set -e  # 遇到错误立即退出

echo "==================================="
echo "操作系统课程 - 工具安装向导"
echo "==================================="
echo ""

# 检查是否在 WSL 中运行
if ! grep -q microsoft /proc/version; then
    echo "警告: 此脚本设计用于 WSL Ubuntu 环境"
    read -p "是否继续? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ">>> 步骤 1: 更新包管理器"
sudo apt update

echo ""
echo ">>> 步骤 2: 安装基础编译工具"
sudo apt install -y build-essential git wget curl

echo ""
echo ">>> 步骤 3: 安装 QEMU 依赖"
sudo apt install -y libglib2.0-dev libpixman-1-dev ninja-build pkg-config

echo ""
echo "是否安装 QEMU? (需要较长时间编译)"
echo "1) 从 apt 安装 (快速,但版本可能较旧)"
echo "2) 从源码编译 (推荐,版本 7.0.0)"
echo "3) 跳过"
read -p "请选择 (1/2/3): " qemu_choice

case $qemu_choice in
    1)
        echo ""
        echo ">>> 正在从 apt 安装 QEMU..."
        sudo apt install -y qemu-system-misc
        ;;
    2)
        echo ""
        echo ">>> 正在下载并编译 QEMU 7.0.0..."
        cd ~
        if [ ! -f "qemu-7.0.0.tar.xz" ]; then
            wget https://download.qemu.org/qemu-7.0.0.tar.xz
        fi
        tar xvJf qemu-7.0.0.tar.xz
        cd qemu-7.0.0
        ./configure --target-list=riscv32-softmmu,riscv64-softmmu
        make -j$(nproc)
        sudo make install
        cd ..
        echo "QEMU 编译安装完成!"
        ;;
    3)
        echo "跳过 QEMU 安装"
        ;;
    *)
        echo "无效选择,跳过 QEMU 安装"
        ;;
esac

echo ""
echo ">>> 步骤 4: 设置 QEMU overcommit"
sudo sysctl vm.overcommit_memory=1

# 使其永久生效
if ! grep -q "vm.overcommit_memory" /etc/sysctl.conf 2>/dev/null; then
    echo "vm.overcommit_memory=1" | sudo tee -a /etc/sysctl.conf
fi

echo ""
echo "==================================="
echo "基础工具安装完成!"
echo "==================================="
echo ""
echo "下一步:"
echo "1. 下载 RISC-V 工具链:"
echo "   https://github.com/sifive/freedom-tools/releases"
echo ""
echo "2. 解压到 ~/toolchains 目录"
echo ""
echo "3. 编辑 setup_os_env.sh,设置正确的 RISCV 路径"
echo ""
echo "4. 运行 'source setup_os_env.sh' 激活环境"
echo ""

# 检查安装结果
echo "==================================="
echo "安装验证:"
echo "==================================="

if command -v gcc &> /dev/null; then
    echo "✓ GCC: $(gcc --version | head -n 1)"
else
    echo "✗ GCC 未找到"
fi

if command -v make &> /dev/null; then
    echo "✓ Make: $(make --version | head -n 1)"
else
    echo "✗ Make 未找到"
fi

if command -v qemu-system-riscv64 &> /dev/null; then
    echo "✓ QEMU: $(qemu-system-riscv64 --version | head -n 1)"
else
    echo "✗ QEMU 未找到 (需要手动安装)"
fi

echo ""
echo "安装完成! 请按照上述步骤继续配置。"
