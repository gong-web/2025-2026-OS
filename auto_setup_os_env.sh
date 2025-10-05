#!/usr/bin/env bash
# --------------------------------------------------------------
# OS Lab 一键隔离环境安装脚本 (WSL 专用)
# 目标: 在 /mnt/d/gds/oslab_env 下安装独立的 RISC-V 交叉编译工具链与 QEMU
# 不污染现有其它课程的环境变量 (不使用全局 RISCV)
# --------------------------------------------------------------
# 使用方式 (在 WSL Bash 中):
#   1) 赋予权限: chmod +x ./auto_setup_os_env.sh
#   2) 执行 (需手动提供工具链 URL):
#        ./auto_setup_os_env.sh --toolchain-url "<SiFive freedom-tools tar.gz 直链>" \
#                              --qemu-version 8.2.2
#   3) 激活环境:
#        source /mnt/d/gds/oslab_env/activate_os_env.sh
#   4) 自检:
#        /mnt/d/gds/oslab_env/test_env.sh
# --------------------------------------------------------------
# 可传入参数:
#   --install-root <路径>        (默认 /mnt/d/gds/oslab_env)
#   --toolchain-url <URL>        (必填, SiFive 预编译工具链, 例如包含 riscv64-elf)
#   --qemu-version <版本号>      (默认 8.2.2)
#   --no-qemu                    (仅安装工具链, 不编译 QEMU)
#   --help                       (显示帮助)
# --------------------------------------------------------------
set -euo pipefail

INSTALL_ROOT="/mnt/d/gds/oslab_env"
TOOLCHAIN_URL=""
QEMU_VERSION="8.2.2"
INSTALL_QEMU=1

red() { printf "\033[31m%s\033[0m\n" "$*"; }
green() { printf "\033[32m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }

usage() {
  cat <<EOF
用法: $0 --toolchain-url <URL> [--install-root <DIR>] [--qemu-version <VER>] [--no-qemu]

示例:
  $0 --toolchain-url "https://github.com/sifive/freedom-tools/releases/download/2024.05.1/riscv64-elf-ubuntu-22.04-2024.05.1.tar.gz"
  $0 --toolchain-url <URL> --qemu-version 8.2.2
  $0 --toolchain-url <URL> --install-root /mnt/d/gds/custom_riscv_env --no-qemu
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-root)
      INSTALL_ROOT="$2"; shift 2;;
    --toolchain-url)
      TOOLCHAIN_URL="$2"; shift 2;;
    --qemu-version)
      QEMU_VERSION="$2"; shift 2;;
    --no-qemu)
      INSTALL_QEMU=0; shift;;
    --help|-h)
      usage; exit 0;;
    *)
      red "未知参数: $1"; usage; exit 1;;
  esac
done

if [ -z "${TOOLCHAIN_URL}" ]; then
  red "必须提供 --toolchain-url (到 SiFive freedom-tools 的 tar.gz 直链)";
  usage; exit 2;
fi

green "[1/7] 准备目录: ${INSTALL_ROOT}";
mkdir -p "${INSTALL_ROOT}" "${INSTALL_ROOT}/sources" || true

TOOLCHAIN_ARCHIVE="${INSTALL_ROOT}/sources/toolchain.tar.gz"

if [ ! -d "${INSTALL_ROOT}/toolchain/bin" ]; then
  green "[2/7] 下载工具链: ${TOOLCHAIN_URL}";
  if [ ! -f "${TOOLCHAIN_ARCHIVE}" ]; then
    wget -O "${TOOLCHAIN_ARCHIVE}" "${TOOLCHAIN_URL}" --progress=dot:giga
  else
    yellow "已存在缓存: ${TOOLCHAIN_ARCHIVE} (跳过下载)";
  fi
  green "[3/7] 解压工具链";
  pushd "${INSTALL_ROOT}/sources" >/dev/null
  tar xzf toolchain.tar.gz
  EXTRACT_DIR="$(find . -maxdepth 1 -type d -name 'riscv*' | head -n1)"
  if [ -z "${EXTRACT_DIR}" ]; then
    red "未找到解压目录 (匹配 riscv*)"; exit 3
  fi
  rsync -a "${EXTRACT_DIR}/" "${INSTALL_ROOT}/toolchain/"
  popd >/dev/null
else
  yellow "工具链已存在: ${INSTALL_ROOT}/toolchain (跳过安装)";
fi

if [ ${INSTALL_QEMU} -eq 1 ]; then
  if [ ! -x "${INSTALL_ROOT}/qemu/bin/qemu-system-riscv64" ]; then
    green "[4/7] 准备编译 QEMU ${QEMU_VERSION}";
    sudo apt-get update
    sudo apt-get install -y build-essential pkg-config libglib2.0-dev libpixman-1-dev python3 wget ninja-build git
    pushd "${INSTALL_ROOT}/sources" >/dev/null
    if [ ! -f "qemu-${QEMU_VERSION}.tar.xz" ]; then
      wget "https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz" --progress=dot:giga
    fi
    if [ ! -d "qemu-${QEMU_VERSION}" ]; then
      tar xJf "qemu-${QEMU_VERSION}.tar.xz"
    fi
    popd >/dev/null
    mkdir -p "${INSTALL_ROOT}/build-qemu"
    pushd "${INSTALL_ROOT}/build-qemu" >/dev/null
    green "[5/7] 配置 QEMU"
    "${INSTALL_ROOT}/sources/qemu-${QEMU_VERSION}/configure" \
      --prefix="${INSTALL_ROOT}/qemu" \
      --target-list=riscv64-softmmu \
      --disable-werror \
      --enable-slirp
    green "[6/7] 编译 & 安装 QEMU (耗时视 CPU 而定)";
    make -j"$(nproc)"
    make install
    popd >/dev/null
  else
    yellow "QEMU 已存在: ${INSTALL_ROOT}/qemu/bin/qemu-system-riscv64 (跳过)";
  fi
else
  yellow "跳过 QEMU 安装 (--no-qemu)";
fi

green "[7/7] 生成 activate / deactivate / test 脚本"

ACTIVATE="${INSTALL_ROOT}/activate_os_env.sh"
DEACTIVATE="${INSTALL_ROOT}/deactivate_os_env.sh"
TESTER="${INSTALL_ROOT}/test_env.sh"

cat >"${ACTIVATE}" <<'EOF'
#!/usr/bin/env bash
if [ "${OS_LAB_ENV_ACTIVATED:-0}" = "1" ]; then
  echo "[os-env] already active"; return 0 2>/dev/null || exit 0
fi
export OS_LAB_OLD_PATH="$PATH"
export RISCV_OS_HOME="__INSTALL_ROOT__/toolchain"
export QEMU_OS_HOME="__INSTALL_ROOT__/qemu"
ADD_PATH=""
[ -d "$RISCV_OS_HOME/bin" ] && ADD_PATH="$RISCV_OS_HOME/bin"
[ -d "$QEMU_OS_HOME/bin" ] && ADD_PATH="${ADD_PATH:+$ADD_PATH:}$QEMU_OS_HOME/bin"
[ -n "$ADD_PATH" ] && export PATH="$ADD_PATH:$PATH"
export OS_LAB_ENV_ACTIVATED=1
export PS1="(OSLAB) $PS1"
echo "[os-env] activated"
echo "  riscv-gcc => $(command -v riscv64-unknown-elf-gcc || echo missing)"
echo "  qemu      => $(command -v qemu-system-riscv64 || echo missing)"
EOF

cat >"${DEACTIVATE}" <<'EOF'
#!/usr/bin/env bash
if [ "${OS_LAB_ENV_ACTIVATED:-0}" != "1" ]; then
  echo "[os-env] not active"; return 0 2>/dev/null || exit 0
fi
[ -n "${OS_LAB_OLD_PATH:-}" ] && export PATH="$OS_LAB_OLD_PATH"
unset OS_LAB_OLD_PATH RISCV_OS_HOME QEMU_OS_HOME OS_LAB_ENV_ACTIVATED
if [ -f ~/.bashrc ]; then source ~/.bashrc >/dev/null 2>&1 || true; fi
echo "[os-env] deactivated"
EOF

cat >"${TESTER}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
echo "[test] PATH=$PATH"
echo "[test] riscv64-unknown-elf-gcc => $(command -v riscv64-unknown-elf-gcc || echo missing)"
echo "[test] qemu-system-riscv64      => $(command -v qemu-system-riscv64 || echo missing)"
if command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  TMP=$(mktemp -d)
  cat >"$TMP/mini.c" <<'C'
int main(){volatile int s=0;for(int i=0;i<10;i++)s+=i;return 0;}
C
  riscv64-unknown-elf-gcc -Os -nostdlib -nostartfiles -Ttext=0x80000000 -o "$TMP/mini.elf" "$TMP/mini.c"
  echo "[test] mini.elf size: $(stat -c %s "$TMP/mini.elf" 2>/dev/null || wc -c <"$TMP/mini.elf") bytes"
  rm -rf "$TMP"
fi
if command -v qemu-system-riscv64 >/dev/null 2>&1; then
  qemu-system-riscv64 --version | head -n1
fi
echo "[test] done"
EOF

# 替换占位符 __INSTALL_ROOT__
sed -i "s#__INSTALL_ROOT__#${INSTALL_ROOT}#g" "${ACTIVATE}"

chmod +x "${ACTIVATE}" "${DEACTIVATE}" "${TESTER}"

green "安装完成"
cat <<INFO
============================================================
激活环境:
  source ${ACTIVATE}

自检:
  ${TESTER}

测试 OpenSBI (可选):
  qemu-system-riscv64 -machine virt -nographic -bios default

撤销环境:
  source ${DEACTIVATE}

如需在任何 shell 自动加载，可在 ~/.bashrc 末尾追加:
  # >>> AUTO OSLAB >>>
  [ -f ${ACTIVATE} ] && source ${ACTIVATE}
  # <<< AUTO OSLAB <<<

============================================================
INFO

exit 0
