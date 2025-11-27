#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>

#define MAX_ARGS            5

static inline int
syscall(int64_t num, ...) {
    va_list ap;
    va_start(ap, num);
    uint64_t a[MAX_ARGS];
    int i;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);

    register int64_t a0 asm("a0") = num;
    register int64_t a1 asm("a1") = a[0];
    register int64_t a2 asm("a2") = a[1];
    register int64_t a3 asm("a3") = a[2];
    register int64_t a4 asm("a4") = a[3];
    register int64_t a5 asm("a5") = a[4];

    asm volatile (
        "ecall\n"
        : "+r" (a0)
        : "r" (a1), "r" (a2), "r" (a3), "r" (a4), "r" (a5)
        : "memory"
    );
    return (int)a0;
}

int
sys_exit(int64_t error_code) {
    return syscall(SYS_exit, error_code, 0, 0, 0, 0);
}

int
sys_fork(void) {
    return syscall(SYS_fork, 0, 0, 0, 0, 0);
}

int
sys_wait(int64_t pid, int *store) {
    return syscall(SYS_wait, pid, store, 0, 0, 0);
}

int
sys_yield(void) {
    return syscall(SYS_yield, 0, 0, 0, 0, 0);
}

int
sys_kill(int64_t pid) {
    return syscall(SYS_kill, pid, 0, 0, 0, 0);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid, 0, 0, 0, 0, 0);
}

int
sys_putc(int64_t c) {
    return syscall(SYS_putc, c, 0, 0, 0, 0);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir, 0, 0, 0, 0, 0);
}

