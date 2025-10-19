/**
 * @file index_caculate.h
 * @brief 索引计算工具函数
 * @date 2025-10-14
 * @copyright Copyright (c) 2025 Zhaoyang-Liang. All rights reserved.
 */

#ifndef __INDEX_CALCULATE_H__
#define __INDEX_CALCULATE_H__

static inline int index2offset(int index, int level, int max_level){ // 计算index对应某一层的offset
    /*
       ( index+1 - 2^level ) * 2^(max_level - level)
        相对偏移*块大小
    */
    return ((index + 1) - (1 << level)) << (max_level - level);
} 

static inline int offset2index(int offset, int level, int max_level){
    return ((offset + (1 << level)) - 1) >> (max_level - level);
}

static inline int brother_index(int index){
    return index - 1 + (index & 1) * 2;
}

static inline int parent_index(int index){
    return (index + 1) / 2 - 1;
}

static inline int left_child_index(int index){
    return index * 2 + 1;
}

static inline int right_child_index(int index){
    return index * 2 + 2;
}

static inline int is_pow_of_2(int x){
    return !(x & (x - 1));
}

static inline int next_pow_of_2(int x){
    if (is_pow_of_2(x))
        return x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

#endif