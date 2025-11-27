#include <stdio.h>
#include <ulib.h>
#include <string.h>

#define MAGIC 0x12345678

int main(void) {
    cprintf("Dirty COW test starting...\n");

    // 1. Allocate a shared variable (in data segment)
    volatile int *shared_var = (int *)0x1000; // Assuming this is a valid user address or we use a static var
    // Actually, let's just use a static variable which is in the data segment.
    static volatile int global_var = MAGIC;

    cprintf("Parent: Initial value = 0x%x\n", global_var);

    int pid = fork();

    if (pid == 0) {
        // Child process
        cprintf("Child: Read value = 0x%x\n", global_var);
        cprintf("Child: Attempting to write 0xdeadbeef...\n");
        
        // This write should trigger COW
        global_var = 0xdeadbeef;
        
        cprintf("Child: Wrote value = 0x%x\n", global_var);
        exit(0);
    } else {
        // Parent process
        int exit_code;
        waitpid(pid, &exit_code);
        
        cprintf("Parent: Child exited.\n");
        cprintf("Parent: Value is now = 0x%x\n", global_var);
        
        if (global_var == MAGIC) {
            cprintf("Dirty COW Test PASSED: Parent memory was NOT modified by child.\n");
        } else {
            cprintf("Dirty COW Test FAILED: Parent memory WAS modified by child!\n");
        }
    }

    return 0;
}
