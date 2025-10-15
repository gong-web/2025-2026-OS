# Buddy System 设计思路文档

一个用C++实现的伙伴系统内存管理器。

## 项目结构

```
OS_lab/
├── CMakeLists.txt          # CMake构建文件
├── test.cpp               # 测试程序
├── include/               # 头文件目录
│   ├── buddy_system.h     # 伙伴系统头文件
│   └── index_caculate.h   # 索引计算头文件
└── src/                   # 源文件目录
    └── buddy_system.cpp   # 伙伴系统实现
```

## 构建和运行

### 使用CMake（推荐）

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译
make

# 运行测试
./test
```

## 功能特性

- 内存分配和释放
- 自动内存合并
- 内存大小查询
- 可视化内存树显示
- 支持0字节分配（转换为1字节）

## 详细设计思路

### 代码实现详解

#### 1. 数据结构定义

```cpp
struct buddy_st {
    int level;              // 最大层级，决定总内存大小
    unsigned char tree[];   // 柔性数组，存储二叉树节点状态
};
```

**代码讲解**：
这个结构体是整个伙伴系统的核心数据结构。`level`字段表示二叉树的最大层级，决定了总内存大小（2^level字节）。`tree[]`是一个柔性数组，用于存储二叉树中每个节点的状态。柔性数组的设计非常巧妙，它允许我们在分配结构体时同时分配数组空间，实现零内存浪费。

**设计要点**：
- 总节点数：`2 * (1 << level) - 1`（完全二叉树的节点数公式）
- 内存大小：`1 << level` 字节
- 每个节点用1个字节存储状态，内存效率高

#### 2. 节点状态系统

```cpp
#define NODE_UNUSED 0    // 未使用
#define NODE_USED 1      // 已使用（只有叶子节点可能）
#define NODE_SPLIT 2     // 已分割
#define NODE_FULL 3      // 已满
```

**状态转换规则**：
- 只有叶子节点可能为`NODE_USED`状态
- 内部节点只能是`NODE_UNUSED`、`NODE_SPLIT`或`NODE_FULL`

#### 3. 内存分配算法 (`buddy_alloc`)

**步骤1：大小对齐**
```cpp
if(size_needed == 0){
    size_to_alloc = 1;  // 0字节转换为1字节
} else {
    size_to_alloc = next_pow_of_2(size_needed);  // 向上舍入到2的幂次
}
```

**代码讲解**：
伙伴系统要求所有内存块都是2的幂次大小，因此需要将请求大小向上舍入。这里处理了边界情况：0字节请求被转换为1字节。`next_pow_of_2`函数使用位运算快速计算大于等于输入值的最小2的幂次，例如7会被舍入到8，15会被舍入到16。

**步骤2：深度优先搜索**
```cpp
while(true) {
    if(size_to_alloc == cur_length) {
        // 找到合适大小的块，尝试分配
        if(buddy->tree[cur_index] == NODE_UNUSED) {
            buddy->tree[cur_index] = NODE_USED;
            _mark_parent(buddy, cur_index);
            return index2offset(cur_index, cur_level, buddy->level);
        }
    } else if(size_to_alloc < cur_length) {
        // 块太大，需要分割
        if(buddy->tree[cur_index] == NODE_UNUSED) {
            buddy->tree[cur_index] = NODE_SPLIT;
            cur_index = left_child_index(cur_index);
            cur_length /= 2;
            continue;
        }
    }
    // 回溯和右移逻辑...
}
```

**代码讲解**：
这是分配算法的核心搜索逻辑。算法采用深度优先搜索策略，优先分配最左侧的可用块。当找到合适大小的块时，将其标记为`NODE_USED`并返回偏移量。当块太大时，将其标记为`NODE_SPLIT`并继续向左子节点搜索。这种策略确保了内存分配的局部性，减少了碎片。

**步骤3：父节点状态更新 (`_mark_parent`)**
```cpp
while(true) {
    int brother_node_index = brother_index(index);
    if(兄弟节点也被使用) {
        index = parent_index(index);
        buddy->tree[index] = NODE_FULL;
    } else {
        break;  // 停止向上更新
    }
}
```

**代码讲解**：
当分配一个节点后，需要向上更新父节点的状态。如果兄弟节点也被使用，则父节点应该标记为`NODE_FULL`，表示其所有子节点都被使用。这个函数会递归向上检查，直到遇到无法合并的节点为止。这种状态维护机制确保了树结构的一致性。

#### 4. 内存释放算法 (`buddy_free`)

**步骤1：定位节点**
```cpp
while(true) {
    if(buddy->tree[cur_index] == NODE_USED) {
        // 找到目标节点
        _combine_parent(buddy, cur_index);
        return;
    } else if(buddy->tree[cur_index] == NODE_SPLIT || buddy->tree[cur_index] == NODE_FULL) {
        // 继续向下搜索
        if(offset < left + cur_length) {
            cur_index = left_child_index(cur_index);
        } else {
            cur_index = right_child_index(cur_index);
            left += cur_length;
        }
    }
}
```

**代码讲解**：
释放算法首先需要定位到要释放的节点。通过比较偏移量和当前块的范围，算法可以确定目标节点在左子树还是右子树中。这种搜索方式的时间复杂度是O(log n)，非常高效。

**步骤2：节点合并 (`_combine_parent`)**
```cpp
while(true) {
    int buddy = index - 1 + (index & 1) * 2;  // 计算兄弟节点
    if(buddy < 0 || self->tree[buddy] != NODE_UNUSED) {
        // 无法合并，标记为未使用
        self->tree[index] = NODE_UNUSED;
        // 向上更新父节点状态
        while(((index = parent_index(index)) >= 0) && self->tree[index] == NODE_FULL) {
            self->tree[index] = NODE_SPLIT;
        }
        return;
    }
    index = parent_index(index);  // 继续向上合并
}
```

**代码讲解**：
这是伙伴系统的核心特性：相邻的空闲块会自动合并。算法首先计算兄弟节点的索引，如果兄弟节点也是空闲的，则向上合并到父节点。合并过程会递归进行，直到无法继续合并为止。

#### 5. 索引计算系统

**核心转换函数**：
```cpp
// 节点索引转内存偏移量
int index2offset(int index, int level, int max_level) {
    return ((index + 1) - (1 << level)) << (max_level - level);
}

// 内存偏移量转节点索引
int offset2index(int offset, int level, int max_level) {
    return ((offset + (1 << level)) - 1) >> (max_level - level);
}
```

**代码讲解**：
这两个函数是索引系统的核心，用于在二叉树节点索引和内存偏移量之间进行转换。`index2offset`将节点索引转换为对应的内存偏移量，`offset2index`则相反。这些函数使用位运算优化，避免了除法运算，性能极高。

**树遍历辅助函数**：
```cpp
int parent_index(int index)      // 父节点索引
int left_child_index(int index)  // 左子节点索引
int right_child_index(int index) // 右子节点索引
int brother_index(int index)     // 兄弟节点索引
```

**代码讲解**：
这些辅助函数提供了树遍历的基本操作。它们都使用简单的数学公式计算，例如左子节点索引为`index * 2 + 1`，右子节点索引为`index * 2 + 2`。这些函数使得树遍历操作变得简洁高效。

#### 6. 可视化系统 (`buddy_show`)

```cpp
void _show(struct buddy_st *self, int index, int level) {
    switch(self->tree[index]) {
        case NODE_UNUSED:
            printf("(%d:%d)", offset, size);  // (偏移:大小)
            break;
        case NODE_USED:
            printf("[%d:%d]", offset, size);  // [偏移:大小]
            break;
        case NODE_FULL:
            printf("{");  // 递归显示子节点
            _show(self, left_child, level + 1);
            _show(self, right_child, level + 1);
            printf("}");
            break;
        default:  // NODE_SPLIT
            printf("(");  // 递归显示子节点
            _show(self, left_child, level + 1);
            _show(self, right_child, level + 1);
            printf(")");
            break;
    }
}
```

**代码讲解**：
这个可视化系统使用递归方式显示整个内存树的状态。不同的符号表示不同的节点状态：`()`表示未使用块，`[]`表示已使用块，`{}`表示满节点，`()`表示分割节点。这种可视化方式使得调试和验证变得非常直观，可以清楚地看到内存的分配和释放过程。


#### 7. 2的幂次计算
```cpp
static inline int is_pow_of_2(int x){
    return !(x & (x - 1));
}

static inline int next_pow_of_2(int x){ // 把右侧所有位都变成1
    if (is_pow_of_2(x)) 
        return x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}
```

**代码讲解**：
这两个函数是伙伴系统的关键工具函数，用于处理2的幂次计算。

`is_pow_of_2`函数使用位运算技巧快速判断一个数是否为2的幂次。原理是：如果x是2的幂次，那么x的二进制表示中只有一个1，而x-1会将这个1变为0，并将右边的所有0变为1。因此`x & (x-1)`的结果为0，取反后返回1。

`next_pow_of_2`函数计算大于等于x的最小2的幂次。算法使用位运算技巧：通过一系列右移和或运算，将x的最高位1右边的所有位都设置为1，然后加1得到下一个2的幂次。例如，对于x=7（二进制111），经过运算后得到15（二进制1111），加1后得到16（二进制10000）。

**性能优势**：
- 使用位运算，避免了循环和除法运算
- 时间复杂度为O(1)，性能极高
- 内联函数设计，减少函数调用开销


### 整体设计思路总结

#### 核心思想
1. **二叉树映射**：将内存空间映射到完全二叉树，每个节点代表一个内存块
2. **状态驱动**：通过节点状态精确描述内存使用情况
3. **递归操作**：分配和释放都采用递归策略，自动维护树结构一致性

#### 算法特点
- **时间复杂度**：O(log n)，其中n是内存池大小
- **空间效率**：使用柔性数组，零内存浪费
- **内存对齐**：自动向上舍入到2的幂次，减少碎片
- **缓存友好**：连续的内存访问模式


## 测试说明


### 完整测试流程

测试程序包含7个测试用例，验证了伙伴系统的各种功能：
1. 基本分配功能
2. 大块分配
3. 小块分配
4. 中等块分配
5. 内存释放和合并
6. 最大分配测试
7. 边界情况测试

### 详细结果分析

#### 测试1：基本分配功能

**测试代码**：
```cpp
int m1 = test_alloc(b, 4);  // 分配4字节
test_size(b, m1);           // 验证大小
```

**预期结果**：
```
alloc at offset: 0 (sz= 4)
((([0:4](4:4))(8:8))(16:16))
size 0 (sz = 4)
```

**结果分析**：
- 成功分配4字节内存，返回偏移量0
- 内存树显示：`[0:4]`表示0-3字节被分配，其余部分保持空闲
- 大小查询返回4，验证分配正确
- 系统将32字节内存分割为4个8字节块，然后进一步分割第一个8字节块

#### 测试2：大块分配

**测试代码**：
```cpp
int m2 = test_alloc(b, 9);  // 分配9字节，向上舍入到16字节
test_size(b, m2);
```

**预期结果**：
```
alloc at offset: 16 (sz= 9)
((([0:4](4:4))(8:8))[16:16])
size 16 (sz = 16)
```

**结果分析**：
- 请求9字节，系统向上舍入到16字节（2^4）
- 分配在偏移量16处，占用16-31字节
- 内存树显示：`[16:16]`表示16字节块被完全使用
- 实际分配大小是16字节，符合伙伴系统的2的幂次要求

#### 测试3：小块分配

**测试代码**：
```cpp
int m3 = test_alloc(b, 3);  // 分配3字节，向上舍入到4字节
test_size(b, m3);
```

**预期结果**：
```
alloc at offset: 4 (sz= 3)
(({[0:4][4:4]}(8:8))[16:16])
size 4 (sz = 4)
```

**结果分析**：
- 请求3字节，系统向上舍入到4字节（2^2）
- 分配在偏移量4处，占用4-7字节
- 内存树显示：`[4:4]`表示4字节块被使用
- 父节点变为`{}`，表示其子节点都被使用

#### 测试4：中等块分配

**测试代码**：
```cpp
int m4 = test_alloc(b, 7);  // 分配7字节，向上舍入到8字节
```

**预期结果**：
```
alloc at offset: 8 (sz= 7)
{{{[0:4][4:4]}[8:8]}[16:16]}
```

**结果分析**：
- 请求7字节，系统向上舍入到8字节（2^3）
- 分配在偏移量8处，占用8-15字节
- 内存树显示：`[8:8]`表示8字节块被使用
- 根节点变为`{}`，表示整个内存树都被分割

#### 测试5：内存释放和合并

**测试代码**：
```cpp
test_free(b, m3);  // 释放3字节块
test_free(b, m1);  // 释放4字节块，测试相邻块合并
test_free(b, m4);  // 释放8字节块
test_free(b, m2);  // 释放16字节块，测试完全合并
```

**预期结果**：
```
free 4
((([0:4](4:4))[8:8])[16:16])
free 0
(((0:8)[8:8])[16:16])
free 8
((0:16)[16:16])
free 16
(0:32)
```

**结果分析**：
- **释放m3（偏移4）**：4字节块被释放，父节点从`{}`变为`()`
- **释放m1（偏移0）**：0-3字节被释放，与4-7字节合并成8字节块`(0:8)`
- **释放m4（偏移8）**：8-15字节被释放，与0-7字节合并成16字节块`(0:16)`
- **释放m2（偏移16）**：16-31字节被释放，与0-15字节合并成完整的32字节`(0:32)`
- 验证了伙伴系统的核心特性：相邻空闲块自动合并

#### 测试6：最大分配测试

**测试代码**：
```cpp
int m5 = test_alloc(b, 32);  // 分配整个32字节
test_free(b, m5);            // 立即释放
```

**预期结果**：
```
alloc at offset: 0 (sz= 32)
[0:32]
free 0
(0:32)
```

**结果分析**：
- 成功分配整个32字节内存空间
- 内存树显示：`[0:32]`表示整个内存被使用
- 立即释放后恢复为空闲状态`(0:32)`
- 验证了系统的最大分配能力

#### 测试7：边界情况测试

**测试代码**：
```cpp
int m6 = test_alloc(b, 0);  // 分配0字节，转换为1字节
test_free(b, m6);           // 释放1字节块
```

**预期结果**：
```
alloc at offset: 4 (sz= 0)
((([0:4](([4:1](5:1))(6:2)))[8:8])[16:16])
free 4
((([0:4](4:4))[8:8])[16:16])
```

**结果分析**：
- 0字节请求被转换为1字节（最小分配单位）
- 系统分配了1字节内存，但实际占用4字节（最小块大小）
- 内存树显示复杂的嵌套结构，体现了精细的内存分割
- 释放后恢复到之前的状态
- 验证了边界情况的正确处理

### 测试结果总结

**内存分配效率**：
- 所有分配请求都成功处理
- 大小向上舍入符合伙伴系统要求
- 分配策略优先使用左侧内存，保持局部性

**内存合并效果**：
- 相邻空闲块能够正确合并
- 释放顺序影响合并效果
- 最终能够完全回收所有内存

**系统稳定性**：
- 边界情况（0字节）得到正确处理
- 最大分配测试验证了系统容量
- 内存树状态始终保持一致性

**性能表现**：
- 分配和释放操作都是O(log n)时间复杂度
- 内存利用率较高，碎片较少
- 可视化输出清晰，便于调试和验证
