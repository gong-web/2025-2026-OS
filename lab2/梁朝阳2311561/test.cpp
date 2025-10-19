#include "include/buddy_system.h"

// 测试分配函数：分配指定大小的内存并显示结果
static int
test_alloc(struct buddy_system::buddy_st *b, int sz)
{
    int r = buddy_system::buddy_alloc(b, sz);
    printf("alloc at offset: %d (sz= %d)\n", r, sz); // 显示分配的偏移量和请求大小
    buddy_system::buddy_show(b); // 显示分配后的内存树状态
    return r;
}

// 测试释放函数：释放指定偏移量的内存并显示结果
static void
test_free(struct buddy_system::buddy_st *b, int addr)
{
    printf("free %d\n", addr); // 显示要释放的偏移量
    buddy_system::buddy_free(b, addr);
    buddy_system::buddy_show(b); // 显示释放后的内存树状态
}

// 测试大小查询函数：查询指定偏移量内存块的实际大小
static void
test_size(struct buddy_system::buddy_st *b, int addr)
{
    int s = buddy_system::buddy_size(b, addr);
    printf("size %d (sz = %d)\n", addr, s); // 显示偏移量和实际大小
}

int main()
{
    // 创建一个5级的buddy system，总内存大小为2^5=32字节
    struct buddy_system::buddy_st *b = buddy_system::new_buddy(5);
    buddy_system::buddy_show(b); // 显示初始状态：整个32字节都是空闲的
    
    // === 测试1：基本分配功能 ===
    int m1 = test_alloc(b, 4); // 分配4字节，应该返回偏移量0
    test_size(b, m1); // 验证分配的大小是否正确

    // === 测试2：分配更大的块 ===
    int m2 = test_alloc(b, 9); // 分配9字节，会被向上舍入到16字节
    test_size(b, m2); // 验证分配的大小

    // === 测试3：分配小块内存 ===
    int m3 = test_alloc(b, 3); // 分配3字节，会被向上舍入到4字节
    test_size(b, m3); // 验证分配的大小

    // === 测试4：分配中等大小的块 ===
    int m4 = test_alloc(b, 7); // 分配7字节，会被向上舍入到8字节
    
    // === 测试5：内存释放和合并功能 ===
    test_free(b, m3); // 释放3字节块，测试小块释放
    test_free(b, m1); // 释放4字节块，测试相邻块合并
    test_free(b, m4); // 释放8字节块
    test_free(b, m2); // 释放16字节块，测试大块释放和完全合并

    // === 测试6：分配整个内存空间 ===
    int m5 = test_alloc(b, 32); // 分配整个32字节，测试最大分配
    test_free(b, m5); // 立即释放，测试大块释放

    // === 测试7：边界情况 - 分配0字节 ===
    int m6 = test_alloc(b, 0); // 分配0字节，应该被转换为1字节
    test_free(b, m6); // 释放1字节块

    // 清理buddy system
    buddy_system::buddy_delete(b);
    return 0;
}