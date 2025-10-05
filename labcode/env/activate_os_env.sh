#!/usr/bin/env bash
# 激活 OS 实验独立 RISC-V / QEMU 环境
# 使用: source labcode/env/activate_os_env.sh

if [ "${OS_LAB_ENV_ACTIVATED:-0}" = "1" ]; then
  echo "[os-env] 已激活，跳过。"
  return 0 2>/dev/null || exit 0
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../../.." && pwd)"

# 允许用户自定义安装根；默认放仓库内，避免与系统/其它课程共享
export RISCV_OS_HOME="${RISCV_OS_HOME:-${SCRIPT_DIR}/toolchain}"  # 将来包含 bin/ gcc
export QEMU_OS_HOME="${QEMU_OS_HOME:-${SCRIPT_DIR}/qemu}"        # 将来包含 bin/ qemu-system-riscv64

# 保存原始 PATH 供撤销使用
export OS_LAB_OLD_PATH="${OS_LAB_OLD_PATH:-$PATH}"

# 前置添加（使本环境优先）
_new_path_parts=()
if [ -d "${RISCV_OS_HOME}/bin" ]; then _new_path_parts+=("${RISCV_OS_HOME}/bin"); fi
if [ -d "${QEMU_OS_HOME}/bin" ]; then _new_path_parts+=("${QEMU_OS_HOME}/bin"); fi

if [ ${#_new_path_parts[@]} -gt 0 ]; then
  export PATH="$(IFS=:; echo "${_new_path_parts[*]}"):${PATH}"
fi

export OS_LAB_ENV_ACTIVATED=1

echo "[os-env] 激活成功"
echo "[os-env] RISCV_OS_HOME=${RISCV_OS_HOME}"
echo "[os-env] QEMU_OS_HOME=${QEMU_OS_HOME}"
echo "[os-env] which riscv64-unknown-elf-gcc => $(command -v riscv64-unknown-elf-gcc 2>/dev/null || echo '未找到')"
echo "[os-env] which qemu-system-riscv64 => $(command -v qemu-system-riscv64 2>/dev/null || echo '未找到')"
