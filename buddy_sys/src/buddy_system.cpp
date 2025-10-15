/**
 * @file buddy_system.cpp
 * @brief 伙伴系统内存管理器模拟接口
 * @date 2025-10-14
 * @copyright Copyright (c) 2025 Zhaoyang-Liang. All rights reserved.
 */


#include "buddy_system.h"

namespace buddy_system {

#define NODE_UNUSED 0
#define NODE_USED 1  
#define NODE_SPLIT 2
#define NODE_FULL 3


struct buddy_st {
    int level;
    unsigned char tree[];
};

struct buddy_st* new_buddy(int level){
    int last_level_node_num = 1 << level ; 
    int all_node_num = 2 * last_level_node_num - 1;

    // 要给一个结构体分配结构体本身的大小加上柔性数组的大小；
    struct buddy_st* buddy = (struct buddy_st*)malloc(sizeof(struct buddy_st) + all_node_num * sizeof(unsigned char));

    buddy->level = level;
    memset(buddy->tree, NODE_UNUSED, all_node_num*sizeof(unsigned char));

    return buddy;
}

void buddy_delete(struct buddy_st* buddy){
    free(buddy);
}

void _mark_parent(struct buddy_st* buddy, int index){
    /**
     * @brief 在分配某个index节点的时候，父亲如果不满就是split，满了就是full
     * @param buddy : 头节点
     * @param index : cur_index （child）
     */

    while(true){
        int brother_node_index = brother_index(index);
        if(brother_node_index >= 0 &&( buddy->tree[brother_node_index] == NODE_USED || buddy->tree[brother_node_index] == NODE_FULL )){
            index = parent_index(index); // 更新index为父节点
            buddy->tree[index] = NODE_FULL;
            // 继续向上标记，不需要continue
        }else{
            break; // 标记结束
        }        
    }
    return ;
}


int buddy_alloc(struct buddy_st* buddy, int size_needed){
    /**
     * @brief 分配内存
     * @param buddy : 头节点
     * @param size_needed : 请求分配的内存大小
     * 
     * @return 分配的内存大小，失败返回-1
     */
    int size_to_alloc ;
    int max_level_length = 1 << buddy->level;

    if(size_needed == 0){
        // 如果要分配0个字节，则分配1个字节
        size_to_alloc = 1 ;
    }else{
        assert(size_needed > 0);
        size_to_alloc = next_pow_of_2(size_needed);
    } 

    if(max_level_length < size_to_alloc){
        return -1;
    }

    int cur_index = 0 ;
    int cur_level = 0 ;
    int cur_length = max_level_length;
    // 从根节点开始，找到第一个可以分配的节点

    while(true){
        // 当前可以就分配，不可看左侧，一直看左侧，到头看右侧，之后回溯看右侧
        if(size_to_alloc == cur_length){
            if(buddy->tree[cur_index] == NODE_UNUSED){
                buddy->tree[cur_index] = NODE_USED;
                _mark_parent(buddy, cur_index);
                return index2offset(cur_index, cur_level, buddy->level);
            }
        }else if(size_to_alloc < cur_length) { // 当前控制层的offset过大，不合适，需要分配各更小的
           
            if(buddy->tree[cur_index] == NODE_UNUSED){ // unused（是第一次分配控制节点或者是树枝）
                // 如果当前节点是 UNUSED，则拆分它并向左子继续

                buddy->tree[cur_index] = NODE_SPLIT;
                // buddy->tree[left_child_index(cur_index)] = NODE_UNUSED;
                // buddy->tree[right_child_index(cur_index)] = NODE_UNUSED;

                cur_index = left_child_index(cur_index);
                cur_length = cur_length / 2;
                cur_level++;
                continue;
            }else if (buddy->tree[cur_index] == NODE_SPLIT){ 
                // 节点被拆分过，继续向左子搜索
                cur_index = left_child_index(cur_index);
                cur_length /= 2;
                cur_level++;
                continue;
            }else if (buddy->tree[cur_index] == NODE_USED || buddy->tree[cur_index] == NODE_FULL){
                // 当前节点被使用或者满了，向右子搜索，如果右侧也不可以就回溯
            }
        }

        if((cur_index & 1) == 1){ // isleft
            cur_index ++ ; // 向右一个节点
            continue; // 看看右侧节点是否可行
        }

        while(true){ // 回溯到父亲节点
            cur_level -- ;
            cur_index = parent_index(cur_index);
            cur_length = cur_length * 2;
            if(cur_index < 0){
                std::cerr << "alloc failed" << std::endl;
                return -1;
            }
            if((cur_index & 1) == 1){
                cur_index++ ; // 看父亲的右侧节点
                break;
            }       
        }       
    }

    return -1; // 不可能达到
}


static void _combine_parent(struct buddy_st *self, int index) // 合并节点
{
    while(true){
        int buddy = index - 1 + (index & 1) * 2; // 计算兄弟节点
        if (buddy < 0 || self->tree[buddy] != NODE_UNUSED)
        { // 如果当前层的节点和自己是可以合并的状态，应该继续向上找，改变父亲的逻辑状态，以完成合并
            self->tree[index] = NODE_UNUSED; // 发现父亲所在的层已经不可合并了，且父亲的儿子们都可以合并，则将父亲标记为未使用
            while (((index = (index + 1) / 2 - 1) >= 0) && self->tree[index] == NODE_FULL) 
            { // 再看看自己的爹，如果自己的爹不是split状态，就全变成split状态
                self->tree[index] = NODE_SPLIT; 
            }
            return;
        }
        index = (index + 1) / 2 - 1;  // 找爹
    }
}


void buddy_free(struct buddy_st* buddy, int offset){
    /**
     * @brief 释放offset所在位置的内存，假释放，实际上为改变父节点的标记。
     * @note  使用 left,length,index三元组确定offset
     * @param buddy : 头节点
     * @param offset : 释放的内存偏移地址，且必须是allocate得到的最左侧
     */
    assert(offset >= 0 && offset < (1 << buddy->level));

    int cur_index = 0 ;
    // int cur_level = 0 ;
    int left = 0 ;
    int cur_length = 1 << buddy->level;

    while(true){
        if(buddy->tree[cur_index] == NODE_USED){
            assert(offset == left); 
            _combine_parent(buddy, cur_index);
            return;
        }else if(buddy->tree[cur_index] == NODE_SPLIT || buddy->tree[cur_index] == NODE_FULL){ //!!两种情况都是需要判断这个offset在哪里（我目前感觉应该如果是full肯定是左侧？）
            cur_length /= 2;
            if (offset < left + cur_length){
                cur_index = left_child_index(cur_index);
                continue;
            }else{
                cur_index = right_child_index(cur_index);
                left += cur_length;
                continue;
            }
        }else {
            std::cerr << "Invalid offset" << std::endl;
            return;
        }
    }
    return ;
}


int buddy_size(struct buddy_st* buddy, int offset){
    /**
     * @brief 释放offset所在位置的内存，假释放，实际上为改变父节点的标记。
     * @note  使用 left,length,index三元组确定offset
     * @param buddy : 头节点
     * @param offset : 释放的内存偏移地址，且必须是allocate得到的最左侧
     */
    assert(offset >= 0 && offset < (1 << buddy->level));

    int cur_index = 0 ;
    // int cur_level = 0 ;
    int left = 0 ;
    int cur_length = 1 << buddy->level;

    while(true){
        if(buddy->tree[cur_index] == NODE_USED){
            assert(offset == left); 
            return cur_length;
        }else if(buddy->tree[cur_index] == NODE_SPLIT || buddy->tree[cur_index] == NODE_FULL){ //!!两种情况都是需要判断这个offset在哪里（我目前感觉应该如果是full肯定是左侧？）
            cur_length /= 2;
            if (offset < left + cur_length){
                cur_index = left_child_index(cur_index);
                continue;
            }else{
                cur_index = right_child_index(cur_index);
                left += cur_length;
                continue;
            }
        }else {
            std::cerr << "Invalid offset" << std::endl;
            return -1;
        }
    }
    return -1;
}




void _show(struct buddy_st *self, int index, int level)
{
    switch (self->tree[index])
    {
    case NODE_UNUSED:
        printf("(%d:%d)", index2offset(index, level, self->level), 1 << (self->level - level));
        break;
    case NODE_USED:
        printf("[%d:%d]", index2offset(index, level, self->level), 1 << (self->level - level));
        break;
    case NODE_FULL:
        printf("{");
        _show(self, index * 2 + 1, level + 1);
        _show(self, index * 2 + 2, level + 1);
        printf("}");
        break;
    default:
        printf("(");
        _show(self, index * 2 + 1, level + 1);
        _show(self, index * 2 + 2, level + 1);
        printf(")");
        break;
    }
}

void buddy_show(struct buddy_st *self)
{
    _show(self, 0, 0);
    printf("\n");
}



} // namespace buddy_system
