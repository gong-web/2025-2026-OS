#!/usr/bin/env bash
# 下载并解压 SiFive 预编译 RISC-V 工具链 (仅一次)
# 可选覆盖变量: RISCV_OS_HOME, RISCV_TOOLCHAIN_URL
# 默认使用 Freedom Tools 发布中的 riscv64-unknown-elf 版本

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export RISCV_OS_HOME="${RISCV_OS_HOME:-${SCRIPT_DIR}/toolchain}"
mkdir -p "${RISCV_OS_HOME}" "${SCRIPT_DIR}/sources"

cd "${SCRIPT_DIR}/sources"

# 版本可按需调整；建议与课程需要匹配
TOOLCHAIN_VERSION="2024.05.1"  # 示例版本号，必要时查看最新 release
ARCHIVE_PATTERN="riscv64-elf-${TOOLCHAIN_VERSION}-*linux-ubuntu*"  # 模糊匹配

# 如果用户自定义直链
DEFAULT_URL=""  # 留空自动匹配
RISCV_TOOLCHAIN_URL="${RISCV_TOOLCHAIN_URL:-${DEFAULT_URL}}"

if [ -d "${RISCV_OS_HOME}/bin" ] && command -v "${RISCV_OS_HOME}/bin/riscv64-unknown-elf-gcc" >/dev/null 2>&1; then
  echo "[toolchain] 已存在：${RISCV_OS_HOME} (跳过下载)"
  exit 0
fi

if [ -n "${RISCV_TOOLCHAIN_URL}" ]; then
  echo "[toolchain] 使用自定义 URL: ${RISCV_TOOLCHAIN_URL}"
  FILENAME="$(basename "${RISCV_TOOLCHAIN_URL}")"
  if [ ! -f "${FILENAME}" ]; then
    wget -O "${FILENAME}" "${RISCV_TOOLCHAIN_URL}" --progress=dot:giga
  fi
else
  echo "[toolchain] 自动搜索 SiFive Freedom Tools release (请手动确认版本是否存在)"
  echo "          请访问: https://github.com/sifive/freedom-tools/releases 复制合适 tar.gz URL"
  echo "          或设置 RISCV_TOOLCHAIN_URL 后重运行。"
  echo "[toolchain] 为避免脚本内置失效链接，这里不强行下载。"
  exit 1
fi

echo "[toolchain] 解压..."
case "${FILENAME}" in
  *.tar.gz) tar xzf "${FILENAME}" ;;
  *.tar.xz) tar xJf "${FILENAME}" ;;
  *.tgz)    tar xzf "${FILENAME}" ;;
  *) echo "[toolchain] 不支持的压缩格式: ${FILENAME}"; exit 2 ;;
esac

EXTRACT_DIR="$(find . -maxdepth 1 -type d -name "riscv*" | head -n1)"
if [ -z "${EXTRACT_DIR}" ]; then
  echo "[toolchain] 未找到解压目录"; exit 3
fi

echo "[toolchain] 同步文件到 ${RISCV_OS_HOME}"
rsync -a "${EXTRACT_DIR}/" "${RISCV_OS_HOME}/"

echo "[toolchain] 完成。可执行: ${RISCV_OS_HOME}/bin/riscv64-unknown-elf-gcc"
"${RISCV_OS_HOME}/bin/riscv64-unknown-elf-gcc" -v | tail -n1 || true

echo "[toolchain] 现在执行: source labcode/env/activate_os_env.sh"
