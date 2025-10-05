#!/usr/bin/env bash
# 自检脚本：验证工具链 + QEMU + 简单程序编译
set -euo pipefail

echo "[test] PATH = $PATH"
echo "[test] which riscv64-unknown-elf-gcc => $(command -v riscv64-unknown-elf-gcc || echo '未找到')"
echo "[test] which qemu-system-riscv64 => $(command -v qemu-system-riscv64 || echo '未找到')"

if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  echo "[test] 未找到交叉编译器，请先运行 install_riscv_toolchain.sh 并激活环境"; exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT
cat > "${TMP_DIR}/hello.c" <<'EOF'
#include <stdio.h>
int main(){
    volatile int sum=0; for(int i=0;i<10;i++) sum+=i; (void)sum; return 0; }
EOF

echo "[test] 编译最小程序 (只验证工具链可用, 不运行)"
riscv64-unknown-elf-gcc -Os -nostdlib -nostartfiles -Ttext=0x80000000 -o "${TMP_DIR}/hello.elf" "${TMP_DIR}/hello.c"
echo "[test] 生成文件大小: $(stat -c '%s' "${TMP_DIR}/hello.elf" 2>/dev/null || wc -c <"${TMP_DIR}/hello.elf") bytes"

if command -v qemu-system-riscv64 >/dev/null 2>&1; then
  echo "[test] QEMU 版本: $(qemu-system-riscv64 --version | head -n1)"
else
  echo "[test] (可选) 尚未安装独立 QEMU"
fi

echo "[test] 完成"
