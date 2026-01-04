#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    # a0: hartid
    # a1: dtb physical address

    # save hartid and dtb address
    # With -fno-pic, 'la' uses PC-relative addressing which works for physical addresses here.
    la      t0, boot_hartid
    sd      a0, 0(t0)
    la      t0, boot_dtb
    sd      a1, 0(t0)

    # t0 := 三级页表的物理地址
    la      t0, boot_page_table_sv39
    # t0 >>= 12，变为三级页表的物理页号
    srli    t0, t0, 12

    # t1 := 8 << 60，设置 satp 的 MODE 字段为 Sv39
    li      t1, 8 << 60
    # 将刚才计算出的预设三级页表物理页号附加到 satp 中
    or      t0, t0, t1
    # 将算出的 t0(即新的MODE|页表基址物理页号) 覆盖到 satp 中
    csrw    satp, t0
    # 使用 sfence.vma 指令刷新 TLB
    sfence.vma

    # 我们在虚拟内存空间中：随意将 sp 设置为虚拟地址！
    la      sp, bootstacktop
    li      t0, 0xffffffff40000000
    add     sp, sp, t0

    # 我们在虚拟内存空间中：随意跳转到虚拟地址！
    # 跳转到 kern_init
    la      t0, kern_init
    li      t1, 0xffffffff40000000
    add     t0, t0, t1
    jr      t0

.section .data
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:

.section .data
    # 由于我们要把这个页表放到一个页里面，因此必须 12 位对齐
    .align PGSHIFT
    .global boot_page_table_sv39
# 分配 4KiB 内存给预设的三级页表
boot_page_table_sv39:
    # 0x00000000_80000000 map to 0x80000000 (1G)
    .zero 8 * 2
    .quad (0x80000 << 10) | 0xcf # VRWXAD
    .zero 8 * 508
    # 0xffffffff_c0000000 map to 0x80000000 (1G)
    .quad (0x80000 << 10) | 0xcf # VRWXAD

    .global boot_hartid
boot_hartid:
    .quad 0
    .global boot_dtb
boot_dtb:
    .quad 0
