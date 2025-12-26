#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <slub.h>
#include <dtb.h>
#include <vmm.h>
#include <proc.h>
#include <kmonitor.h>
#include <ide.h>
#include <fs.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    
    cons_init(); // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    dtb_init(); // init dtb
    pmm_init(); // init physical memory management
    slub_init(); // init slub allocator

    pic_init(); // init interrupt controller
    idt_init(); // init interrupt descriptor table

    vmm_init(); // init virtual memory management
    cprintf("vmm_init done\n");
    sched_init();
    cprintf("sched_init done\n");
    proc_init(); // init process table
    cprintf("proc_init done\n");

    ide_init();  // init ide devices
    fs_init();   // init fs

    clock_init();  // init clock interrupt
    cprintf("clock_init done\n");
    intr_enable(); // enable irq interrupt

    cpu_idle(); // run idle process
}

