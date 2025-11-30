#include <stdio.h>
#include <ulib.h>
#include <string.h>
#include <error.h>

int main(void);

// --- Test Functions ---

void test_code_write() {
    cprintf("TEST: Code Segment Write\n");
    volatile unsigned char *addr = (unsigned char *)main;
    cprintf("Attempting to write to %p\n", addr);
    *addr = 0xCC; 
    cprintf("FAIL: Survived code write!\n");
    exit(0);
}

void test_rodata_write() {
    cprintf("TEST: Read-Only Data Write\n");
    const char *msg = "This is read-only data";
    volatile char *addr = (char *)msg;
    cprintf("Attempting to write to %p\n", addr);
    *addr = 'X';
    cprintf("FAIL: Survived rodata write!\n");
    exit(0);
}

void test_kernel_write() {
    cprintf("TEST: Kernel Memory Write\n");
    // KERNBASE for RISC-V 64 in ucore
    volatile char *addr = (char *)0xFFFFFFFFC0200000; 
    cprintf("Attempting to write to %p\n", addr);
    *addr = 'X';
    cprintf("FAIL: Survived kernel write!\n");
    exit(0);
}

#define N_CHILDREN 20
#define ARRAY_SIZE 4096
int shared_data[ARRAY_SIZE];

void test_fork_race() {
    cprintf("TEST: COW Fork Race (Stress Test)\n");
    
    int i, pid;
    for (i = 0; i < ARRAY_SIZE; i++) shared_data[i] = 0;
    
    for (i = 0; i < N_CHILDREN; i++) {
        pid = fork();
        if (pid == 0) {
            int j;
            for (j = 0; j < ARRAY_SIZE; j++) shared_data[j] = i + 1;
            for (j = 0; j < ARRAY_SIZE; j++) {
                if (shared_data[j] != i + 1) exit(1);
            }
            exit(0);
        }
    }
    
    int status;
    int failed = 0;
    for (i = 0; i < N_CHILDREN; i++) {
        if (waitpid(0, &status) != 0 || status != 0) failed = 1;
    }
    
    if (failed) {
        cprintf("FAIL: Fork race test failed\n");
        exit(1);
    } else {
        cprintf("PASS: Fork race test passed\n");
        exit(0);
    }
}

// --- Runner ---

struct test_case {
    const char *name;
    void (*func)(void);
    int expect_crash;
};

struct test_case tests[] = {
    {"Code Write", test_code_write, 1},
    {"Rodata Write", test_rodata_write, 1},
    {"Kernel Write", test_kernel_write, 1},
    {"Fork Race", test_fork_race, 0},
    {NULL, NULL, 0}
};

int main(void) {
    cprintf("Starting PoC Suite...\n");
    int passed = 0;
    int total = 0;
    
    for (int i = 0; tests[i].name != NULL; i++) {
        total++;
        cprintf("Running %s...\n", tests[i].name);
        int pid = fork();
        if (pid == 0) {
            tests[i].func();
            exit(0);
        }
        
        int status;
        waitpid(pid, &status);
        
        int success = 0;
        if (tests[i].expect_crash) {
            // -E_KILLED is -9
            if (status == -E_KILLED) { 
                success = 1;
                cprintf("PASS: Process killed as expected.\n");
            } else {
                cprintf("FAIL: Process exited with %d, expected crash (%d).\n", status, -E_KILLED);
            }
        } else {
            if (status == 0) {
                success = 1;
                cprintf("PASS: Test completed successfully.\n");
            } else {
                cprintf("FAIL: Process failed with %d.\n", status);
            }
        }
        
        if (success) passed++;
        cprintf("--------------------------------\n");
    }
    
    cprintf("Summary: %d/%d tests passed.\n", passed, total);
    return 0;
}
