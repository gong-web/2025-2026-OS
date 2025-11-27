#!/bin/bash
make clean
make touch
make "DEFS+=-DTEST=dirtycow_test -DTESTSTART=_binary_obj___user_dirtycow_test_out_start -DTESTSIZE=_binary_obj___user_dirtycow_test_out_size"
qemu-system-riscv64 -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
