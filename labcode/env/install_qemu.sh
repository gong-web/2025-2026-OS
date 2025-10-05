#!/usr/bin/env bash
# 源码编译隔离 QEMU (只含 riscv64-softmmu) 防止与系统 qemu 冲突
# 允许变量覆盖: QEMU_OS_HOME, QEMU_VERSION

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export QEMU_OS_HOME="${QEMU_OS_HOME:-${SCRIPT_DIR}/qemu}"
mkdir -p "${QEMU_OS_HOME}" "${SCRIPT_DIR}/sources" "${SCRIPT_DIR}/build-qemu"

QEMU_VERSION="${QEMU_VERSION:-8.2.2}"  # 可按需升级
TARBALL="qemu-${QEMU_VERSION}.tar.xz"
URL="https://download.qemu.org/${TARBALL}"

if [ -x "${QEMU_OS_HOME}/bin/qemu-system-riscv64" ]; then
  echo "[qemu] 已存在 ${QEMU_OS_HOME}/bin/qemu-system-riscv64 (跳过)"
  exit 0
fi

cd "${SCRIPT_DIR}/sources"
if [ ! -f "${TARBALL}" ]; then
  echo "[qemu] 下载 ${URL}"
  wget "${URL}" --progress=dot:giga
fi

if [ ! -d "qemu-${QEMU_VERSION}" ]; then
  echo "[qemu] 解压 ${TARBALL}"
  tar xJf "${TARBALL}"
fi

cd "${SCRIPT_DIR}/build-qemu"
echo "[qemu] 配置..."
"${SCRIPT_DIR}/sources/qemu-${QEMU_VERSION}/configure" \
  --prefix="${QEMU_OS_HOME}" \
  --target-list=riscv64-softmmu \
  --disable-werror \
  --enable-slirp || { echo "[qemu] configure 失败"; exit 2; }

echo "[qemu] 编译... (并行: $(nproc))"
make -j"$(nproc)"
echo "[qemu] 安装..."
make install

echo "[qemu] 完成: ${QEMU_OS_HOME}/bin/qemu-system-riscv64"
"${QEMU_OS_HOME}/bin/qemu-system-riscv64" --version | head -n1
echo "[qemu] 现在执行: source labcode/env/activate_os_env.sh"
