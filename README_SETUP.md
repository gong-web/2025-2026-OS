# 操作系统课程环境配置指南

## 环境隔离方案

为了将操作系统课程和编译系统原理课程的工具链隔离开,我们采用**按需激活**的方式,而不是在 `~/.bashrc` 中全局配置。

## 快速开始

### 1. 下载 RISC-V 工具链

访问 https://github.com/sifive/freedom-tools/releases

下载适合 Ubuntu 的预编译工具链,例如:
- `riscv64-unknown-elf-toolchain-*.x86_64-linux-ubuntu*.tar.gz`

### 2. 解压工具链

```bash
# 进入 WSL
wsl

# 创建工具链目录(建议放在用户目录下)
mkdir -p ~/toolchains

# 解压下载的文件到指定目录
cd ~/Downloads  # 根据你的下载位置调整
tar -xzf riscv64-unknown-elf-toolchain-*.tar.gz -C ~/toolchains

# 重命名为简短的名字(可选)
mv ~/toolchains/riscv64-unknown-elf-toolchain-* ~/toolchains/riscv64-unknown-elf
```

### 3. 修改环境配置脚本

编辑 `setup_os_env.sh` 文件,修改 RISCV 路径:

```bash
# 将这一行:
export RISCV=/path/to/your/riscv-toolchain

# 修改为你的实际路径,例如:
export RISCV=$HOME/toolchains/riscv64-unknown-elf
```

### 4. 安装 QEMU (如果还没安装)

```bash
# 在 WSL 中执行

# 方法1: 使用 apt 安装 (可能版本较旧)
sudo apt update
sudo apt install qemu-system-misc

# 方法2: 从源码编译 (推荐,版本较新)
cd ~
wget https://download.qemu.org/qemu-7.0.0.tar.xz
tar xvJf qemu-7.0.0.tar.xz
cd qemu-7.0.0
./configure --target-list=riscv32-softmmu,riscv64-softmmu
make -j$(nproc)
sudo make install
```

如果遇到依赖问题,先安装必要的包:
```bash
sudo apt install build-essential libglib2.0-dev libpixman-1-dev ninja-build
```

### 5. 使用环境

**每次需要做操作系统实验时:**

```bash
# 进入 WSL
wsl

# 进入实验目录
cd /mnt/d/gds/Documents/Operating_system

# 激活操作系统课程环境
source setup_os_env.sh

# 现在可以使用 RISC-V 工具链了
riscv64-unknown-elf-gcc -v

# 编译和运行实验
cd labcode/lab1
make
make qemu
```

**退出实验时:**

```bash
# 恢复原始环境
deactivate_os_env

# 或者直接关闭终端
```

## 为什么这样做?

1. **环境隔离**: 只有在需要时才激活 OS 课程环境,不影响编译原理课程的工具链
2. **灵活切换**: 可以轻松在不同课程的环境之间切换
3. **避免冲突**: 不同工具链版本不会相互干扰
4. **易于管理**: 所有配置集中在一个脚本中

## 额外提示

### 在 VS Code 中使用

1. 打开 VS Code 的 WSL 连接
2. 在终端中运行 `source setup_os_env.sh`
3. 环境变量会在当前终端会话中生效

### 创建快捷命令(可选)

如果觉得每次都要输入 `source setup_os_env.sh` 太麻烦,可以在 `~/.bashrc` 中添加一个别名:

```bash
# 编辑 ~/.bashrc
nano ~/.bashrc

# 在文件末尾添加:
alias osenv='source /mnt/d/gds/Documents/Operating_system/setup_os_env.sh'

# 保存后执行:
source ~/.bashrc
```

之后只需输入 `osenv` 即可激活环境。

## 检查 QEMU overcommit 设置

如果 QEMU 运行出现问题,执行:

```bash
sudo sysctl vm.overcommit_memory=1
```

## 故障排查

### 问题: 找不到 riscv64-unknown-elf-gcc

**解决方案:**
1. 检查 `setup_os_env.sh` 中的 `RISCV` 路径是否正确
2. 确认工具链的 `bin` 目录存在
3. 重新 source 脚本: `source setup_os_env.sh`

### 问题: QEMU 版本过低

**解决方案:**
- 按照上面的方法2从源码编译安装

### 问题: 权限问题

**解决方案:**
```bash
chmod +x setup_os_env.sh
```

## 目录结构说明

```
Operating_system/
├── setup_os_env.sh          # 环境配置脚本
├── README_SETUP.md           # 本文档
├── labcode/
│   ├── lab1/                # 实验1
│   ├── lab2/                # 实验2
│   └── lab3/                # 实验3
└── ...
```

## 参考资源

- RISC-V 工具链: https://github.com/sifive/freedom-tools
- QEMU 官方: https://www.qemu.org/
- rCore Tutorial: https://rcore-os.github.io/rCore-Tutorial-Book-v3/

---

配置完成后,祝你实验顺利! 🚀
