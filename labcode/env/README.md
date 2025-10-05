# OS 实验独立 RISC-V/QEMU 环境 (WSL 专用)

本目录用于为“操作系统实验”建立一个与其它课程（如编译原理）完全隔离的 RISC-V 交叉编译与 QEMU 调试环境，避免路径、版本、依赖混淆。

## 目录结构

```
labcode/env/
  activate_os_env.sh        # 激活(按需 source)隔离环境，不永久污染全局 PATH
  deactivate_os_env.sh      # 撤销激活还原 PATH 与变量
  install_riscv_toolchain.sh# 下载/解压 SiFive 预编译交叉工具链
  install_qemu.sh           # 源码编译仅含 RISC-V 的 QEMU (riscv64-softmmu)
  test_env.sh               # 环境自检：版本、示例程序编译 & qemu 版本
  .gitignore                # 忽略下载缓存/构建产物
```

## 设计原则

1. 不修改/覆盖你已有课程的 RISCV/QEMU 安装。
2. 使用独立的变量名：`RISCV_OS_HOME` 与 `QEMU_OS_HOME`，避免与常见的 `RISCV` 冲突。
3. 缺省安装到仓库内部 `labcode/env/toolchain` 与 `labcode/env/qemu`，可通过环境变量覆盖。
4. 激活采用 `source labcode/env/activate_os_env.sh`，不强制写入 `~/.bashrc`。如需自动加载，可添加带标记块，易于撤销。
5. 可重复执行（幂等）：脚本检测已存在目录/二进制则跳过重建。

## 快速开始

在 WSL Ubuntu 终端进入仓库根目录：

```bash
cd /mnt/d/gds/Documents/Operating_system   # 根据你实际挂载路径调整
chmod +x labcode/env/*.sh                 # 赋予执行权限

# 1. 安装 RISC-V 交叉工具链（默认下载 SiFive Freedom Tools 压缩包）
./labcode/env/install_riscv_toolchain.sh

# 2. （可选）源码编译独立 QEMU，如你系统已有合适版本且想完全隔离再执行
./labcode/env/install_qemu.sh

# 3. 激活环境（每个新 shell 需要）
source labcode/env/activate_os_env.sh

# 4. 运行自检
./labcode/env/test_env.sh
```

## 可选：在 ~/.bashrc 添加自动激活块

```bash
# >>> OS_LAB_ENV (auto-activate, optional) >>>
if [ -f "/mnt/d/gds/Documents/Operating_system/labcode/env/activate_os_env.sh" ]; then
  source "/mnt/d/gds/Documents/Operating_system/labcode/env/activate_os_env.sh"
fi
# <<< OS_LAB_ENV <<<
```

添加后执行：`source ~/.bashrc` 生效。若与其它课程工具链冲突，暂时注释此块或执行 `deactivate_os_env.sh`。

## 卸载 / 清理

```bash
rm -rf labcode/env/toolchain labcode/env/qemu
git restore labcode/env/.gitignore 2>/dev/null || true
```

## 常见问题 (FAQ)

| 问题 | 可能原因 | 解决 |
|------|----------|------|
| 调用 `riscv64-unknown-elf-gcc` 仍是旧版本 | 未激活或 PATH 顺序错误 | `which riscv64-unknown-elf-gcc` 检查是否指向 labcode/env/toolchain 路径；重新 source |
| QEMU 版本过低 | 没有执行独立编译 | 运行 `install_qemu.sh`，确保 `qemu-system-riscv64 --version` 输出期望版本 |
| 激活后想恢复原样 | 需要撤销环境变量 | 执行 `deactivate_os_env.sh` |
| 工具链下载太慢 | GitHub 直连问题 | 预先下载压缩包放入 `labcode/env/sources` 再执行安装脚本 |

## 后续扩展

可在 `activate_os_env.sh` 中追加 GDB (riscv64-unknown-elf-gdb) 的别名、为 QEMU 启动脚本提供函数、或集成你自动化调试脚本的 PATH。

祝实验顺利！
