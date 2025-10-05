#!/usr/bin/env bash
# 撤销 OS 实验独立环境
# 使用: source labcode/env/deactivate_os_env.sh

if [ "${OS_LAB_ENV_ACTIVATED:-0}" != "1" ]; then
  echo "[os-env] 当前未激活或已撤销。"
  return 0 2>/dev/null || exit 0
fi

if [ -n "${OS_LAB_OLD_PATH:-}" ]; then
  export PATH="${OS_LAB_OLD_PATH}"
  unset OS_LAB_OLD_PATH
fi

unset RISCV_OS_HOME
unset QEMU_OS_HOME
unset OS_LAB_ENV_ACTIVATED

echo "[os-env] 已撤销环境。"
