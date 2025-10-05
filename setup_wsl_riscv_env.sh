#!/usr/bin/env bash
#############################################################
# WSL RISC-V OS Lab Isolated Environment Setup Script
#
# Purpose:
#   - Create an isolated RISC-V toolchain + QEMU environment
#   - Avoid polluting existing global or other course toolchains
#   - Append (optionally) controlled markers to ~/.bashrc OR rely on activation script
#
# Default install root (inside the OS lab repo) so everything stays self‑contained:
#   ${REPO_ROOT}/riscv_isolated
#
# Usage examples:
#   bash setup_wsl_riscv_env.sh \
#       --toolchain-url https://github.com/sifive/freedom-tools/releases/download/v2020.12/freedom-tools-x86_64-centos7-2020.12.0.tar.gz
#
#   bash setup_wsl_riscv_env.sh \
#       --toolchain-url /mnt/d/downloads/freedom-tools-x86_64-linux-2020.12.0.tar.gz --qemu-version 4.1.1
#
# Parameters:
#   --install-root <path>   Where to install (default: ./riscv_isolated)
#   --toolchain-url <url|file>  Prebuilt freedom-tools tar.(gz|xz) (required)
#   --qemu-version <ver>    QEMU version to build (default: 4.1.1 to match tutorial; can use newer like 8.2.2)
#   --no-qemu               Skip building QEMU (if you already have an isolated qemu path)
#   --append-bashrc         Append activation marker block to ~/.bashrc automatically
#   --clean                 Remove existing install-root before proceeding
#   -j / --jobs <N>         Parallel build jobs for QEMU (default: nproc)
#   -y                      Non-interactive; assume yes
#
# Resulting layout:
#   <install-root>/
#       toolchain/   (extracted freedom-tools, chooses first directory match)
#       qemu/        (installed prefix for built QEMU)
#       scripts/
#           activate_riscv_env.sh
#           deactivate_riscv_env.sh
#
# After success:
#   source <install-root>/scripts/activate_riscv_env.sh
#   riscv64-unknown-elf-gcc -v
#   qemu-system-riscv64 --version
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

INSTALL_ROOT="${REPO_ROOT}/riscv_isolated"
TOOLCHAIN_URL=""
QEMU_VERSION="4.1.1"
BUILD_QEMU=1
APPEND_BASHRC=0
CLEAN_FIRST=0
JOBS="$(nproc || echo 4)"
ASSUME_YES=0

color() { local c="$1"; shift; printf "\033[%sm%s\033[0m\n" "$c" "$*"; }
info(){ color 36 "[INFO] $*"; }
warn(){ color 33 "[WARN] $*"; }
err(){ color 31 "[ERR ] $*" >&2; }

confirm() {
  if [ "$ASSUME_YES" -eq 1 ]; then return 0; fi
  read -rp "$1 [y/N]: " ans || true
  case "${ans:-}" in y|Y|yes|YES) return 0 ;; *) return 1 ;; esac
}

usage() {
  sed -n '1,120p' "$0" | grep -E '^#' | sed 's/^# \{0,1\}//'
  exit 1
}

while [ $# -gt 0 ]; do
  case "$1" in
    --install-root) INSTALL_ROOT="$2"; shift 2;;
    --toolchain-url) TOOLCHAIN_URL="$2"; shift 2;;
    --qemu-version) QEMU_VERSION="$2"; shift 2;;
    --no-qemu) BUILD_QEMU=0; shift;;
    --append-bashrc) APPEND_BASHRC=1; shift;;
    --clean) CLEAN_FIRST=1; shift;;
    -j|--jobs) JOBS="$2"; shift 2;;
    -y) ASSUME_YES=1; shift;;
    -h|--help) usage;;
    *) err "Unknown arg: $1"; usage;;
  esac
done

if [ -z "${TOOLCHAIN_URL}" ]; then
  err "--toolchain-url is required (can be a local file path)."; usage
fi

if [ "${CLEAN_FIRST}" -eq 1 ] && [ -d "${INSTALL_ROOT}" ]; then
  if confirm "Remove existing ${INSTALL_ROOT}?"; then
    rm -rf "${INSTALL_ROOT}"
  else
    err "User aborted clean."; exit 1
  fi
fi

mkdir -p "${INSTALL_ROOT}" "${INSTALL_ROOT}/downloads" "${INSTALL_ROOT}/scripts"
DOWNLOADS="${INSTALL_ROOT}/downloads"

############################################
# 1. Acquire toolchain
############################################
info "Toolchain acquisition"
TOOLCHAIN_ARCHIVE="${TOOLCHAIN_URL}"
if [[ "${TOOLCHAIN_URL}" =~ ^https?:// ]]; then
  fname="${TOOLCHAIN_URL##*/}"; fname="${fname%%\?*}" # strip query
  TOOLCHAIN_ARCHIVE="${DOWNLOADS}/${fname}"
  if [ ! -f "${TOOLCHAIN_ARCHIVE}" ]; then
    info "Downloading toolchain: ${TOOLCHAIN_URL}";
    curl -L --fail -o "${TOOLCHAIN_ARCHIVE}" "${TOOLCHAIN_URL}"
  else
    info "Using cached ${TOOLCHAIN_ARCHIVE}";
  fi
else
  if [ ! -f "${TOOLCHAIN_ARCHIVE}" ]; then
    err "Local toolchain archive not found: ${TOOLCHAIN_ARCHIVE}"; exit 1
  fi
fi

info "Extracting toolchain..."
TOOLCHAIN_DIR="${INSTALL_ROOT}/toolchain"
mkdir -p "${TOOLCHAIN_DIR}"

case "${TOOLCHAIN_ARCHIVE}" in
  *.tar.gz|*.tgz) tar -xzf "${TOOLCHAIN_ARCHIVE}" -C "${TOOLCHAIN_DIR}" --strip-components=1 || true ;;
  *.tar.xz) tar -xJf "${TOOLCHAIN_ARCHIVE}" -C "${TOOLCHAIN_DIR}" --strip-components=1 || true ;;
  *.tar.bz2) tar -xjf "${TOOLCHAIN_ARCHIVE}" -C "${TOOLCHAIN_DIR}" --strip-components=1 || true ;;
  *) err "Unsupported archive format: ${TOOLCHAIN_ARCHIVE}"; exit 1 ;;
esac

if [ ! -x "${TOOLCHAIN_DIR}/bin/riscv64-unknown-elf-gcc" ]; then
  # Try unstripping if strip-components failed due to unknown layout
  if [ -z "$(find "${TOOLCHAIN_DIR}" -maxdepth 1 -type f -name riscv64-unknown-elf-gcc 2>/dev/null)" ]; then
    err "riscv64-unknown-elf-gcc not found after extraction. Inspect archive layout."; exit 1
  fi
fi

RISCV_PATH="${TOOLCHAIN_DIR}"

############################################
# 2. Build QEMU (optional)
############################################
QEMU_PREFIX="${INSTALL_ROOT}/qemu"
if [ "${BUILD_QEMU}" -eq 1 ]; then
  info "Building QEMU ${QEMU_VERSION} (targets: riscv64-softmmu,riscv32-softmmu)"
  QEMU_ARCHIVE="${DOWNLOADS}/qemu-${QEMU_VERSION}.tar.xz"
  if [ ! -f "${QEMU_ARCHIVE}" ]; then
    curl -L --fail -o "${QEMU_ARCHIVE}" "https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz"
  else
    info "Using cached ${QEMU_ARCHIVE}";
  fi
  SRC_DIR="${INSTALL_ROOT}/build-qemu-src"
  rm -rf "${SRC_DIR}" && mkdir -p "${SRC_DIR}"
  tar -xJf "${QEMU_ARCHIVE}" -C "${SRC_DIR}" --strip-components=1
  pushd "${SRC_DIR}" >/dev/null
    ./configure \
      --prefix="${QEMU_PREFIX}" \
      --target-list=riscv64-softmmu,riscv32-softmmu \
      --disable-werror > configure.log 2>&1
    make -j"${JOBS}" > build.log 2>&1
    make install > install.log 2>&1
  popd >/dev/null
  info "QEMU installed to ${QEMU_PREFIX}"
else
  warn "Skipping QEMU build (--no-qemu specified)."
fi

############################################
# 3. Activation / Deactivation scripts
############################################
ACTIVATE="${INSTALL_ROOT}/scripts/activate_riscv_env.sh"
DEACTIVATE="${INSTALL_ROOT}/scripts/deactivate_riscv_env.sh"

cat >"${ACTIVATE}" <<EOF
#!/usr/bin/env bash
# Auto-generated activation script for isolated RISC-V OS lab env
if [ -n "\${RISCV_LAB_ENV_ACTIVE:-}" ]; then
  echo "[riscv-env] Already active: \${RISCV}" >&2
  return 0 2>/dev/null || exit 0
fi
export RISCV="${RISCV_PATH}"
export PATH="${RISCV_PATH}/bin${BUILD_QEMU:+:${QEMU_PREFIX}/bin}:">${PATH}" # placeholder, replaced below
export _OLD_RISCV_LAB_PATH="${PATH}"
export PATH="${RISCV_PATH}/bin${BUILD_QEMU:+:${QEMU_PREFIX}/bin}:$PATH"
export RISCV_LAB_ENV_ACTIVE=1
echo "[riscv-env] Activated. RISCV=\${RISCV}";
command -v riscv64-unknown-elf-gcc >/dev/null || echo "[riscv-env][WARN] gcc not found in PATH";
EOF

cat >"${DEACTIVATE}" <<'EOF'
#!/usr/bin/env bash
if [ -z "${RISCV_LAB_ENV_ACTIVE:-}" ]; then
  echo "[riscv-env] Not active" >&2
  return 0 2>/dev/null || exit 0
fi
if [ -n "${_OLD_RISCV_LAB_PATH:-}" ]; then
  export PATH="${_OLD_RISCV_LAB_PATH}"; unset _OLD_RISCV_LAB_PATH
fi
unset RISCV
unset RISCV_LAB_ENV_ACTIVE
echo "[riscv-env] Deactivated"
EOF

chmod +x "${ACTIVATE}" "${DEACTIVATE}"

############################################
# 4. Optional bashrc Appending
############################################
if [ "${APPEND_BASHRC}" -eq 1 ]; then
  BASHRC="${HOME}/.bashrc"
  BLOCK_BEGIN="# >>> RISC-V OS LAB ISOLATED ENV >>>"
  BLOCK_END="# <<< RISC-V OS LAB ISOLATED ENV <<<"
  if grep -Fq "${BLOCK_BEGIN}" "${BASHRC}"; then
    warn "Marker block already in ~/.bashrc; skipping append."
  else
    info "Appending marker activation snippet to ~/.bashrc"
    {
      echo "${BLOCK_BEGIN}"
      echo "# Source the isolated env activation if present (non-intrusive)"
      echo "if [ -f '${ACTIVATE}' ]; then"
      echo "  source '${ACTIVATE}' >/dev/null 2>&1 || true"
      echo "fi"
      echo "${BLOCK_END}"
    } >> "${BASHRC}"
  fi
fi

############################################
# 5. Validation
############################################
info "Validating toolchain..."
"${RISCV_PATH}/bin/riscv64-unknown-elf-gcc" -v >/dev/null 2>&1 || { err "GCC test failed"; exit 1; }

if [ "${BUILD_QEMU}" -eq 1 ]; then
  info "Validating QEMU..."
  "${QEMU_PREFIX}/bin/qemu-system-riscv64" --version | head -n1 || { err "QEMU test failed"; exit 1; }
fi

cat <<SUMMARY

============================================================
 RISC-V OS Lab isolated environment setup COMPLETE
------------------------------------------------------------
 Install root : ${INSTALL_ROOT}
 Toolchain    : ${RISCV_PATH}
 QEMU         : ${BUILD_QEMU:+${QEMU_PREFIX} (built ${QEMU_VERSION})}${BUILD_QEMU:+' (enabled)'}${BUILD_QEMU:+''}
 Activate     : source ${ACTIVATE}
 Deactivate   : source ${DEACTIVATE}
 Bashrc hook  : $([ ${APPEND_BASHRC} -eq 1 ] && echo Added || echo Not added)
------------------------------------------------------------
 Next steps:
   source ${ACTIVATE}
   riscv64-unknown-elf-gcc -v
   qemu-system-riscv64 --version (if built)
============================================================
SUMMARY

exit 0
