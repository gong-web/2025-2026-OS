WSL RISC-V OS Lab Isolated Environment
=====================================

目标
----
在当前操作系统实验仓库内部创建一个与其它课程/全局安装完全隔离的 RISC-V 交叉编译与 QEMU 模拟环境，避免污染已有 PATH 或干扰之前“编译系统原理”课程的工具链。

核心设计原则
------------
1. 目录内自包含：所有内容安装在仓库根目录下的 `riscv_isolated/`。
2. 显式激活/撤销：只在 `source activate_riscv_env.sh` 后才修改 PATH，退出后恢复。
3. 可重复、幂等：脚本多次运行仍保持可控；`--clean` 允许重置。
4. 可选写入 `~/.bashrc`（默认不写）以避免自动污染，需显式 `--append-bashrc`。

快速开始
--------
（以下命令请在 WSL 中、仓库根目录 `/mnt/d/gds/Documents/Operating_system` 下执行）

1. 选择（或下载）SiFive freedom-tools 预编译包网址（举例）：
   https://github.com/sifive/freedom-tools/releases
   示例：`https://github.com/sifive/freedom-tools/releases/download/v2020.12/freedom-tools-x86_64-centos7-2020.12.0.tar.gz`

2. 运行一键安装（构建 QEMU 4.1.1 与教程保持一致）：
   bash setup_wsl_riscv_env.sh \
       --toolchain-url https://github.com/sifive/freedom-tools/releases/download/v2020.12/freedom-tools-x86_64-centos7-2020.12.0.tar.gz

   如果你已经有本地下载的包：
   bash setup_wsl_riscv_env.sh --toolchain-url /mnt/d/downloads/freedom-tools-x86_64-centos7-2020.12.0.tar.gz

   使用更高版本 QEMU（例：8.2.2）：
   bash setup_wsl_riscv_env.sh --toolchain-url <url> --qemu-version 8.2.2

3. 激活环境：
   source riscv_isolated/scripts/activate_riscv_env.sh

4. 验证：
   riscv64-unknown-elf-gcc -v
   qemu-system-riscv64 --version

5. 完成后撤销：
   source riscv_isolated/scripts/deactivate_riscv_env.sh

参数说明
--------
| 参数 | 作用 |
|------|------|
| --toolchain-url | 指定 freedom-tools 预编译包（必填，URL 或本地文件） |
| --install-root  | 自定义安装根目录（默认 ./riscv_isolated） |
| --qemu-version  | 指定 QEMU 版本（默认 4.1.1） |
| --no-qemu       | 跳过 QEMU 构建 |
| --append-bashrc | 追加自动激活片段到 ~/.bashrc（默认不追加） |
| --clean         | 安装前清空旧目录 |
| -j / --jobs     | QEMU 并行编译线程数（默认 nproc） |
| -y              | 无需交互确认全部 yes |

为什么不用直接写死 PATH 到 ~/.bashrc？
--------------------------------------
直接写死会造成多个课程环境互相覆盖。通过激活脚本：
1. 需要时 `source`；
2. 不使用时 `deactivate`；
3. 明确标记变量 `RISCV_LAB_ENV_ACTIVE` 便于脚本检测。

与已有 env 目录的区别
----------------------
`labcode/env/` 目录里的脚本是早期实验的更通用版本；本方案专为 WSL 独立安装、遵循题述教程（QEMU 4.1.1 + freedom-tools 预编译包 + RISCV 变量）而定制。

升级/扩展建议
--------------
1. 增加自动探测最新 freedom-tools 版本（GitHub API）。
2. 缓存构建产物（ccache）缩短多次构建时间。
3. 增加 `make check` / 简单 smoke test 执行最小裸机程序。
4. 与 `run_auto_debug.sh` 脚本联动：未激活则提示激活。

常见问题
--------
Q: 提示 gcc 找不到？
A: 确认已执行 `source riscv_isolated/scripts/activate_riscv_env.sh`，并且 toolchain/bin 内有 riscv64-unknown-elf-gcc。

Q: QEMU 版本仍是旧的？
A: 终端可能还在旧 PATH，执行 `which qemu-system-riscv64` 检查；若仍指向系统路径，说明未成功激活或 bashrc 覆盖了 PATH。

Q: 想切换不同版本 QEMU？
A: 重新运行脚本带 `--clean --qemu-version <ver>` ；或在另一个 `--install-root` 并行安装。

许可证
------
该安装脚本本身使用 MIT 风格；freedom-tools 与 QEMU 仍遵循各自上游许可证。
