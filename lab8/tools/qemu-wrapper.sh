#!/bin/sh
echo "Wrapper args: $@" >&2
exec qemu-system-riscv64 $(echo "$@" | sed 's/-device loader,file=bin\/ucore.img,addr=0x80200000/-kernel bin\/kernel/') \
    -drive file=bin/swap.img,media=disk,cache=writeback \
    -drive file=bin/sfs.img,media=disk,cache=writeback
