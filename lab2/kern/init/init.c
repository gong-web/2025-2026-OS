#include <console.h>
#include <defs.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <dtb.h>
#include <slub.h>  // 学号：2312325

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);
void print_kerninfo(void);

/* *
 * print_kerninfo - print the information about kernel, including the location
 * of kernel entry, the start addresses of data and text segements, the start
 * address of free memory and how many memory that kernel has used.
 * */
void print_kerninfo(void) {
    extern char etext[], edata[], end[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%016lx (virtual)\n", (uintptr_t)kern_init);
    cprintf("  etext  0x%016lx (virtual)\n", etext);
    cprintf("  edata  0x%016lx (virtual)\n", edata);
    cprintf("  end    0x%016lx (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n",
            (end - (char*)kern_init + 1023) / 1024);
}

int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    pmm_init();  // init physical memory management

    // 学号：2312325 - 初始化 SLUB 分配器
    cprintf("\n========================================\n");
    cprintf("Initializing SLUB allocator...\n");
    cprintf("Student ID: 2312325\n");
    cprintf("========================================\n");
    slub_init();
    cprintf("SLUB allocator initialized successfully!\n\n");

    // 学号：2312325 - 运行 SLUB 测试
    cprintf("Running SLUB tests...\n");
    slub_test();
    cprintf("\n========================================\n");
    cprintf("All SLUB tests completed!\n");
    cprintf("========================================\n\n");

    /* do nothing */
    while (1)
        ;
}

