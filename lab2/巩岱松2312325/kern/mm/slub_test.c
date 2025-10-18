#include <slub.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// 测试统计
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_BEGIN(name) \
    do { \
        cprintf("\n[TEST] %s ... ", name); \
    } while (0)

#define TEST_PASS() \
    do { \
        cprintf("PASS\n"); \
        tests_passed++; \
    } while (0)

#define TEST_FAIL(msg) \
    do { \
        cprintf("FAIL: %s\n", msg); \
        tests_failed++; \
    } while (0)

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while (0)

// =============================================================================
// 测试用例
// =============================================================================

/* test_basic_alloc_free - 基本分配和释放
 */
static void test_basic_alloc_free(void) {
    TEST_BEGIN("Basic allocation and free");
    
    // 分配单个对象
    void *obj = slub_alloc(64);
    TEST_ASSERT(obj != NULL, "Failed to allocate 64 bytes");
    
    // 写入数据验证可用性
    memset(obj, 0xAA, 64);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT(((uint8_t *)obj)[i] == 0xAA, "Memory corruption detected");
    }
    
    // 释放对象
    slub_free(obj);
    
    TEST_PASS();
}

/* test_multiple_allocs - 多对象分配
 */
static void test_multiple_allocs(void) {
    TEST_BEGIN("Multiple allocations");
    
    const int count = 10;
    void *objects[count];
    
    // 分配多个对象
    for (int i = 0; i < count; i++) {
        objects[i] = slub_alloc(128);
        TEST_ASSERT(objects[i] != NULL, "Allocation failed");
        
        // 写入唯一标识
        *(int *)objects[i] = i;
    }
    
    // 验证对象独立性
    for (int i = 0; i < count; i++) {
        TEST_ASSERT(*(int *)objects[i] == i, "Object overwritten");
    }
    
    // 验证地址不重叠
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            uintptr_t addr_i = (uintptr_t)objects[i];
            uintptr_t addr_j = (uintptr_t)objects[j];
            TEST_ASSERT(addr_i + 128 <= addr_j || addr_j + 128 <= addr_i,
                       "Objects overlap");
        }
    }
    
    // 释放所有对象
    for (int i = 0; i < count; i++) {
        slub_free(objects[i]);
    }
    
    TEST_PASS();
}

/* test_different_sizes - 不同大小分配
 */
static void test_different_sizes(void) {
    TEST_BEGIN("Different size allocations");
    
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    void *objects[8];
    
    // 分配不同大小
    for (int i = 0; i < 8; i++) {
        objects[i] = slub_alloc(sizes[i]);
        TEST_ASSERT(objects[i] != NULL, "Allocation failed");
        
        // 填充数据
        memset(objects[i], i, sizes[i]);
    }
    
    // 验证数据完整性
    for (int i = 0; i < 8; i++) {
        for (size_t j = 0; j < sizes[i]; j++) {
            TEST_ASSERT(((uint8_t *)objects[i])[j] == (uint8_t)i,
                       "Data corruption");
        }
    }
    
    // 释放
    for (int i = 0; i < 8; i++) {
        slub_free(objects[i]);
    }
    
    TEST_PASS();
}

/* test_realloc_same_size - 重复分配释放同一大小
 */
static void test_realloc_same_size(void) {
    TEST_BEGIN("Repeated alloc/free same size");
    
    const int iterations = 100;
    
    for (int i = 0; i < iterations; i++) {
        void *obj = slub_alloc(256);
        TEST_ASSERT(obj != NULL, "Allocation failed");
        
        // 写入测试数据
        *(int *)obj = i;
        TEST_ASSERT(*(int *)obj == i, "Data corrupted");
        
        slub_free(obj);
    }
    
    TEST_PASS();
}

/* test_cross_slab - 跨 slab 分配
 */
static void test_cross_slab(void) {
    TEST_BEGIN("Cross-slab allocation");
    
    struct kmem_cache *cache = get_cache_for_size(64);
    TEST_ASSERT(cache != NULL, "Cache not found");
    
    unsigned int obj_per_slab = cache->objects_per_slab;
    cprintf("(objects per slab: %u) ", obj_per_slab);
    
    // 分配超过一个 slab 的对象
    int count = obj_per_slab * 2 + 5;
    void **objects = (void **)slub_alloc(count * sizeof(void *));
    TEST_ASSERT(objects != NULL, "Failed to allocate tracking array");
    
    for (int i = 0; i < count; i++) {
        objects[i] = slub_alloc(64);
        TEST_ASSERT(objects[i] != NULL, "Allocation failed");
        *(int *)objects[i] = i;
    }
    
    // 验证所有对象
    for (int i = 0; i < count; i++) {
        TEST_ASSERT(*(int *)objects[i] == i, "Object corrupted");
    }
    
    // 释放
    for (int i = 0; i < count; i++) {
        slub_free(objects[i]);
    }
    slub_free(objects);
    
    TEST_PASS();
}

/* test_mixed_operations - 混合分配释放
 */
static void test_mixed_operations(void) {
    TEST_BEGIN("Mixed allocation and free");
    
    void *obj1 = slub_alloc(32);
    void *obj2 = slub_alloc(64);
    void *obj3 = slub_alloc(128);
    
    TEST_ASSERT(obj1 && obj2 && obj3, "Allocations failed");
    
    // 部分释放
    slub_free(obj2);
    
    // 继续分配
    void *obj4 = slub_alloc(64);  // 应该重用 obj2 的空间
    void *obj5 = slub_alloc(256);
    
    TEST_ASSERT(obj4 && obj5, "Allocations failed");
    
    // 写入验证
    *(int *)obj1 = 1;
    *(int *)obj3 = 3;
    *(int *)obj4 = 4;
    *(int *)obj5 = 5;
    
    TEST_ASSERT(*(int *)obj1 == 1, "obj1 corrupted");
    TEST_ASSERT(*(int *)obj3 == 3, "obj3 corrupted");
    TEST_ASSERT(*(int *)obj4 == 4, "obj4 corrupted");
    TEST_ASSERT(*(int *)obj5 == 5, "obj5 corrupted");
    
    // 全部释放
    slub_free(obj1);
    slub_free(obj3);
    slub_free(obj4);
    slub_free(obj5);
    
    TEST_PASS();
}

/* test_boundary_sizes - 边界大小测试
 */
static void test_boundary_sizes(void) {
    TEST_BEGIN("Boundary size allocations");
    
    // 最小尺寸
    void *obj_min = slub_alloc(1);
    TEST_ASSERT(obj_min != NULL, "Min size allocation failed");
    *(uint8_t *)obj_min = 0xFF;
    TEST_ASSERT(*(uint8_t *)obj_min == 0xFF, "Min size data corrupted");
    
    // 边界尺寸
    void *obj_16 = slub_alloc(16);
    void *obj_17 = slub_alloc(17);  // 应该使用 32 字节 cache
    void *obj_2048 = slub_alloc(2048);
    void *obj_2049 = slub_alloc(2049);  // 应该直接分配页
    
    TEST_ASSERT(obj_16 && obj_17 && obj_2048 && obj_2049, 
               "Boundary allocations failed");
    
    // 验证分配策略
    struct Page *page_16 = virt_to_page(obj_16);
    struct Page *page_17 = virt_to_page(obj_17);
    struct Page *page_2048 = virt_to_page(obj_2048);
    struct Page *page_2049 = virt_to_page(obj_2049);
    
    TEST_ASSERT(page_16->flags & PG_slab, "obj_16 should be slab");
    TEST_ASSERT(page_17->flags & PG_slab, "obj_17 should be slab");
    TEST_ASSERT(page_2048->flags & PG_slab, "obj_2048 should be slab");
    TEST_ASSERT(!(page_2049->flags & PG_slab), "obj_2049 should not be slab");
    
    slub_free(obj_min);
    slub_free(obj_16);
    slub_free(obj_17);
    slub_free(obj_2048);
    slub_free(obj_2049);
    
    TEST_PASS();
}

/* test_stress - 压力测试
 */
static void test_stress(void) {
    TEST_BEGIN("Stress test");
    
    const int iterations = 500;
    const int batch_size = 20;
    
    for (int iter = 0; iter < iterations; iter++) {
        void *batch[batch_size];
        
        // 批量分配
        for (int i = 0; i < batch_size; i++) {
            size_t size = 16 << (iter % 8);  // 轮换不同大小
            batch[i] = slub_alloc(size);
            
            if (batch[i] == NULL) {
                // 内存耗尽，释放已分配的并退出
                for (int j = 0; j < i; j++) {
                    slub_free(batch[j]);
                }
                cprintf("(memory exhausted at iteration %d) ", iter);
                goto stress_done;
            }
            
            // 写入唯一标识
            *(int *)batch[i] = i;
        }
        
        // 验证
        for (int i = 0; i < batch_size; i++) {
            TEST_ASSERT(*(int *)batch[i] == i, "Stress test data corrupted");
        }
        
        // 批量释放
        for (int i = 0; i < batch_size; i++) {
            slub_free(batch[i]);
        }
    }
    
stress_done:
    TEST_PASS();
}

/* test_cache_statistics - 统计信息测试
 */
static void test_cache_statistics(void) {
    TEST_BEGIN("Cache statistics");
    
    // 获取初始统计
    struct kmem_cache *cache = get_cache_for_size(128);
    TEST_ASSERT(cache != NULL, "Cache not found");
    
    unsigned long alloc_before = cache->alloc_count;
    unsigned long free_before = cache->free_count;
    
    // 执行分配释放
    const int count = 10;
    void *objects[count];
    
    for (int i = 0; i < count; i++) {
        objects[i] = slub_alloc(128);
    }
    
    TEST_ASSERT(cache->alloc_count == alloc_before + count,
               "Alloc count incorrect");
    
    for (int i = 0; i < count; i++) {
        slub_free(objects[i]);
    }
    
    TEST_ASSERT(cache->free_count == free_before + count,
               "Free count incorrect");
    
    TEST_PASS();
}

/* test_null_handling - NULL 处理测试
 */
static void test_null_handling(void) {
    TEST_BEGIN("NULL pointer handling");
    
    // 应该安全处理 NULL
    slub_free(NULL);  // 不应该崩溃
    
    void *obj = slub_alloc(0);  // 应该返回 NULL
    TEST_ASSERT(obj == NULL, "Zero size should return NULL");
    
    TEST_PASS();
}

// =============================================================================
// 主测试函数
// =============================================================================

int slub_test(void) {
    cprintf("\n");
    cprintf("=====================================\n");
    cprintf("   SLUB Allocator Test Suite\n");
    cprintf("=====================================\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    // 运行所有测试
    test_basic_alloc_free();
    test_multiple_allocs();
    test_different_sizes();
    test_realloc_same_size();
    test_cross_slab();
    test_mixed_operations();
    test_boundary_sizes();
    test_null_handling();
    test_cache_statistics();
    test_stress();  // 最后运行压力测试
    
    // 完整性检查
    cprintf("\n");
    slub_check();
    
    // 打印统计
    slub_print_stats();
    
    // 总结
    cprintf("\n");
    cprintf("=====================================\n");
    cprintf("   Test Summary\n");
    cprintf("-------------------------------------\n");
    cprintf("   Passed: %d\n", tests_passed);
    cprintf("   Failed: %d\n", tests_failed);
    cprintf("   Total:  %d\n", tests_passed + tests_failed);
    cprintf("=====================================\n");
    
    if (tests_failed == 0) {
        cprintf("\n✓ All tests PASSED!\n\n");
        return 0;
    } else {
        cprintf("\n✗ Some tests FAILED!\n\n");
        return -1;
    }
}
