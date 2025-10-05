#!/bin/bash
# 自动化执行 Lab1 启动调试流程
# 生成: auto_debug_log.txt （完整GDB输出）
#       qemu_stdout.txt     （QEMU输出）
#       auto_debug_summary.md （结果摘要）

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

LOG=auto_debug_log.txt
QEMU_LOG=qemu_stdout.txt
SUMMARY=auto_debug_summary.md
GDB_CMD=auto_gdb_cmds.txt

echo "[INFO] 激活环境..." >&2
if [ -f ../../riscv_isolated/scripts/activate_riscv_env.sh ]; then
  # shellcheck disable=SC1091
  source ../../riscv_isolated/scripts/activate_riscv_env.sh
elif [ -f ../../setup_os_env.sh ]; then
  # shellcheck disable=SC1091
  source ../../setup_os_env.sh
else
  echo "[WARN] 未找到环境激活脚本，假设已激活" >&2
fi

if ! command -v qemu-system-riscv64 >/dev/null 2>&1; then
  echo "[ERROR] 未找到 qemu-system-riscv64" >&2
  exit 1
fi
if ! command -v riscv64-unknown-elf-gdb >/dev/null 2>&1; then
  echo "[ERROR] 未找到 riscv64-unknown-elf-gdb" >&2
  exit 1
fi

echo "[INFO] 清理并编译内核..." >&2
make -s clean || true
make -s || { echo "[ERROR] make 失败" >&2; exit 1; }

if [ ! -f bin/kernel ]; then
  echo "[ERROR] 未生成 bin/kernel" >&2
  exit 1
fi

cat > "$GDB_CMD" <<'EOF'
set pagination off
set confirm off
file bin/kernel
set arch riscv:rv64
target remote :1234

# 阶段1: 复位向量
printf "\n==== 阶段1: 复位向量 (应为 0x1000) ====\n"
info registers pc
x/12i $pc

# 单步前5条观测
printf "\n---- 单步执行前5条指令 ----\n"
si
x/6i $pc
si
x/6i $pc
si
x/6i $pc
si
x/6i $pc
si
x/6i $pc

printf "\n寄存器 (t0/a0/a1) 状态: \n"
info registers t0 a0 a1

# 继续到 OpenSBI 初始化阶段一段时间, 然后设置断点
printf "\n==== 阶段2: 设置内核入口断点并继续执行 ====\n"
define hook-stop
  if $pc == 0x80200000
    printf "\n==== 阶段3: 到达内核入口 0x80200000 ====\n"
    info registers pc t0 a0 a1 sp ra
    x/16i 0x80200000
  end
end
b *0x80200000
continue

# 额外：转储内核入口前 32 字节
printf "\n内核入口前后 32 字节原始数据:\n"
x/32bx 0x80200000

quit
EOF

echo "[INFO] 启动 QEMU (GDB stub 暂停)..." >&2
set +e
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -m 256M \
  -smp 1 \
  -bios default \
  -kernel bin/kernel \
  -s -S >"$QEMU_LOG" 2>&1 &
QPID=$!
set -e

if [ -z "${QPID:-}" ]; then
  echo "[ERROR] 无法获取 QEMU 进程 PID" >&2
  exit 1
fi

if ! ps -p "$QPID" >/dev/null 2>&1; then
  echo "[ERROR] QEMU 进程未启动 (PID $QPID 不存在)" >&2
  exit 1
fi

echo "[INFO] 等待 QEMU GDB 端口..." >&2
# 简单等待 (避免依赖 nc)
READY=0
for i in $(seq 1 80); do
  if timeout 0.2 bash -c '</dev/tcp/127.0.0.1/1234' 2>/dev/null; then
    READY=1
    break
  fi
  if ! ps -p "$QPID" >/dev/null 2>&1; then
    echo "[ERROR] QEMU 进程意外退出, 查看 $QEMU_LOG" >&2
    exit 1
  fi
  sleep 0.1
done

if [ $READY -ne 1 ]; then
  echo "[ERROR] GDB 端口 (1234) 未在超时内就绪" >&2
  exit 1
fi

echo "[INFO] 运行 GDB 批处理..." >&2
riscv64-unknown-elf-gdb -q -batch -x "$GDB_CMD" >"$LOG" 2>&1 || true

# 结束 QEMU
if ps -p "$QPID" >/dev/null 2>&1; then
  kill "$QPID" 2>/dev/null || true
  wait "$QPID" 2>/dev/null || true
fi

# 解析关键信息
RESET_PC=$(grep -E "pc[[:space:]]+0x" "$LOG" | head -n1 | awk '{for(i=1;i<=NF;i++){if($i~/0x/){print $i;break}}}')
T0_VAL=$(grep -E "t0[[:space:]]+0x" "$LOG" | head -n1 | awk '{for(i=1;i<=NF;i++){if($i~/0x/){print $i;break}}}')
A0_VAL=$(grep -E "a0[[:space:]]+0x" "$LOG" | head -n1 | awk '{for(i=1;i<=NF;i++){if($i~/0x/){print $i;break}}}')
A1_VAL=$(grep -E "a1[[:space:]]+0x" "$LOG" | head -n1 | awk '{for(i=1;i<=NF;i++){if($i~/0x/){print $i;break}}}')
KERNEL_PC_LINE=$(grep -n "阶段3: 到达内核入口" "$LOG" | cut -d: -f1 | head -n1)
ENTRY_DUMP=""
if [ -n "$KERNEL_PC_LINE" ]; then
  ENTRY_DUMP=$(tail -n +$((KERNEL_PC_LINE+1)) "$LOG" | head -n 40)
fi

cat > "$SUMMARY" <<EOF
# 自动化调试摘要

生成时间: $(date '+%Y-%m-%d %H:%M:%S')

## 关键寄存器与地址
- 复位初始 PC: ${RESET_PC:-未捕获}
- t0 (跳转 OpenSBI 目标估计): ${T0_VAL:-未捕获}
- a0 (hartid): ${A0_VAL:-未捕获}
- a1 (DTB 地址): ${A1_VAL:-未捕获}

## 启动流程判断
1. 复位向量应位于 0x1000 (若上方 PC 为 0x1000 说明正确)
2. t0 通常加载 OpenSBI 入口 (期望 ~0x80000000)
3. 内核入口断点命中地址应为 0x80200000

## 内核入口附近反汇编 (截取)


auto_debug_log.txt 片段（阶段3之后的部分）：
```
${ENTRY_DUMP}
```

## 原始 GDB 输出
详见: $LOG

## QEMU 原始输出
详见: $QEMU_LOG

---
自动脚本: run_auto_debug.sh
EOF

echo "[OK] 自动调试完成. 结果文件:"
echo " - $LOG"
echo " - $SUMMARY"
echo " - $QEMU_LOG"
