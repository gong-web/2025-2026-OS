#!/bin/sh
echo "GDB args: $@" > gdb.log
riscv64-unknown-elf-gdb "$@" >> gdb.log 2>&1 &
# Sleep to keep QEMU alive
sleep 60
