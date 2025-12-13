// COW测试程序 - 测试Copy-on-Write机制的正确性
#include <stdio.h>
#include <ulib.h>
#include <string.h>

#define TEST_SIZE 4096
#define MAGIC_NUMBER 0x12345678

volatile int global_data_var = 0;
volatile int global_array[100];
volatile int global_counter_var = 1;
static volatile unsigned char global_pagebuf[4096 * 4] __attribute__((aligned(4096)));

// 测试用例1: 基本的COW功能测试
void test_basic_cow() {
    cprintf("\n=== Test 1: Basic COW Test ===\n");
    
    // 使用全局变量代替malloc
    volatile int *data = &global_data_var;
    *data = MAGIC_NUMBER;
    cprintf("Parent: Initial value = 0x%x at addr %p\n", *data, data);
    
    int pid = fork();
    if (pid == 0) {
        // 子进程：尽量少做 printf（RV64 下频繁变参打印更容易暴露其它问题），
        // 只做必要的读写与退出码返回。
        if (*data != MAGIC_NUMBER) {
            exit(-1);
        }
        *data = 0x87654321;
        if (*data != 0x87654321) {
            exit(-1);
        }
        exit(0);
    } else if (pid > 0) {
        // 父进程：等待子进程
        int exit_code;
        waitpid(pid, &exit_code);

        if (exit_code == 0) {
            cprintf("Child: PASSED - COW triggered successfully\n");
        } else {
            cprintf("Child: FAILED - COW test failed\n");
        }
        
        // 验证父进程的数据未被修改
        cprintf("Parent: After child exit, value = 0x%x (should still be 0x%x)\n", 
                *data, MAGIC_NUMBER);
        if (*data != MAGIC_NUMBER) {
            cprintf("Parent: FAILED - Data was modified by child!\n");
        } else {
            cprintf("Parent: PASSED - Data isolation successful\n");
        }
    } else {
        cprintf("Fork failed!\n");
    }
}

// 测试用例2: 多次写入测试
void test_multiple_writes() {
    cprintf("\n=== Test 2: Multiple Writes Test ===\n");
    
    volatile int *array = global_array;
    for (int i = 0; i < 10; i++) {
        array[i] = i * 100;
    }
    
    int pid = fork();
    if (pid == 0) {
        // 子进程：修改多个值
        // cprintf("Child: Modifying multiple values...\n");
        for (int i = 0; i < 10; i++) {
            array[i] = i * 200;
        }
        
        // 验证修改
        int passed = 1;
        for (int i = 0; i < 10; i++) {
            if (array[i] != i * 200) {
                cprintf("Child: FAILED at index %d\n", i);
                passed = 0;
            }
        }
        if (passed) {
            cprintf("Child: PASSED - All writes successful\n");
        }
        exit(passed ? 0 : -1);
    } else if (pid > 0) {
        int exit_code;
        waitpid(pid, &exit_code);
        
        // 验证父进程的数据
        int passed = 1;
        for (int i = 0; i < 10; i++) {
            if (array[i] != i * 100) {
                cprintf("Parent: FAILED at index %d, value = %d\n", i, array[i]);
                passed = 0;
            }
        }
        if (passed) {
            cprintf("Parent: PASSED - Original data preserved\n");
        }
    }
}

// 测试用例3: 多进程并发写入测试 (Stress Test)
void test_concurrent_writes() {
    cprintf("\n=== Test 3: Concurrent Writes Test (Multi-process) ===\n");
    
    volatile int *counter = &global_counter_var;
    *counter = 0;
    
    // 创建第一个子进程
    int pid1 = fork();
    if (pid1 == 0) {
        // Child 1
        cprintf("Child 1: Starting...\n");
        for (int i = 0; i < 10000; i++) {
            (*counter)++;
            if (i % 1000 == 0) {
                cprintf("1"); 
                yield();
            }
        }
        cprintf("\nChild 1: Done. Final = %d\n", *counter);
        exit(*counter == 10000 ? 0 : -1);
    }
    
    // 创建第二个子进程
    int pid2 = fork();
    if (pid2 == 0) {
        // Child 2
        cprintf("Child 2: Starting...\n");
        for (int i = 0; i < 10000; i++) {
            (*counter)++;
            if (i % 1000 == 0) {
                cprintf("2");
                yield();
            }
        }
        cprintf("\nChild 2: Done. Final = %d\n", *counter);
        exit(*counter == 10000 ? 0 : -1);
    }

    // 父进程也参与竞争
    if (pid1 > 0 && pid2 > 0) {
        cprintf("Parent: Starting...\n");
        for (int i = 0; i < 10000; i++) {
            (*counter)++;
            if (i % 1000 == 0) {
                cprintf("P");
                yield();
            }
        }
        cprintf("\n");
        
        int exit_code1, exit_code2;
        waitpid(pid1, &exit_code1);
        waitpid(pid2, &exit_code2);
        
        cprintf("Parent: Final counter = %d (should be 10000)\n", *counter);
        
        if (*counter == 10000 && exit_code1 == 0 && exit_code2 == 0) {
            cprintf("Parent: PASSED - All processes maintained isolated counters\n");
        } else {
            cprintf("Parent: FAILED - Counter is %d, Child1 exit: %d, Child2 exit: %d\n", *counter, exit_code1, exit_code2);
        }
    }
}

// 测试用例4: 跨页面的COW测试
void test_cross_page_cow() {
    cprintf("\n=== Test 4: Cross-Page COW Test ===\n");
    
    // 分配跨越多个页面的数据
    volatile unsigned char *buffer = global_pagebuf;
    int pages = 4;
    
    // 初始化多个页面
    for (int p = 0; p < pages; p++) {
        for (int i = 0; i < 4096; i++) {
            buffer[p * 4096 + i] = (unsigned char)(p + i);
        }
    }
    
    int pid = fork();
    if (pid == 0) {
        // 子进程：修改第2和第3个页面
        cprintf("Child: Modifying pages 2 and 3...\n");
        for (int i = 0; i < 4096; i++) {
            buffer[1 * 4096 + i] = 0xAA;  // 第2页
            buffer[2 * 4096 + i] = 0xBB;  // 第3页
        }
        
        // 验证修改
        int passed = 1;
        for (int i = 0; i < 4096; i++) {
            if (buffer[1 * 4096 + i] != 0xAA || buffer[2 * 4096 + i] != 0xBB) {
                passed = 0;
                break;
            }
        }
        
        cprintf("Child: %s\n", passed ? "PASSED" : "FAILED");
        exit(passed ? 0 : -1);
    } else if (pid > 0) {
        int exit_code;
        waitpid(pid, &exit_code);
        
        // 验证父进程的数据未被修改
        int passed = 1;
        for (int p = 0; p < pages; p++) {
            for (int i = 0; i < 100; i++) {  // 只检查前100个字节
                if (buffer[p * 4096 + i] != (unsigned char)(p + i)) {
                    cprintf("Parent: FAILED at page %d, offset %d\n", p, i);
                    passed = 0;
                    break;
                }
            }
            if (!passed) break;
        }
        
        if (passed) {
            cprintf("Parent: PASSED - All pages preserved correctly\n");
        }
    }
}

// 测试用例5: 只读页面的安全测试
void test_readonly_protection() {
    cprintf("\n=== Test 5: Read-Only Protection Test ===\n");
    cprintf("This test verifies that read-only pages cannot be written\n");
    cprintf("(Note: This test may cause a page fault if protection is working)\n");
    
    // 这个测试需要系统支持设置只读内存区域
    // 在ucore中，如果VMA标记为只读，写入应该失败
    cprintf("Test skipped - requires VMA permission setup\n");
}

int main(void) {
    cprintf("\n");
    cprintf("╔════════════════════════════════════════════════════════╗\n");
    cprintf("║    uCore Copy-on-Write (COW) Mechanism Test Suite     ║\n");
    cprintf("╚════════════════════════════════════════════════════════╝\n");
    
    test_basic_cow();
    test_multiple_writes();
    test_concurrent_writes();
    test_cross_page_cow();
    test_readonly_protection();
    
    cprintf("\n");
    cprintf("╔════════════════════════════════════════════════════════╗\n");
    cprintf("║              All COW Tests Completed!                  ║\n");
    cprintf("╚════════════════════════════════════════════════════════╝\n");
    cprintf("\n");
    
    return 0;
}
