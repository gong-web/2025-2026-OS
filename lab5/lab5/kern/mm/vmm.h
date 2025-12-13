#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>

// pre define
struct mm_struct;

// the virtual continuous memory area(vma), [vm_start, vm_end),
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end
// 虚拟内存区域 (VMA) 结构体
// 描述一段连续的虚拟内存区域 [vm_start, vm_end)
// 属于该 VMA 的地址满足: vma.vm_start <= addr < vma.vm_end
struct vma_struct
{
    struct mm_struct *vm_mm; // the set of vma using the same PDT (所属的 mm_struct)
    uintptr_t vm_start;      // start addr of vma (起始地址)
    uintptr_t vm_end;        // end addr of vma, not include the vm_end itself (结束地址，不包含自身)
    uint32_t vm_flags;       // flags of vma (标志位: RWX)
    list_entry_t list_link;  // linear list link which sorted by start addr of vma (链表节点，按地址排序)
};

#define le2vma(le, member) \
    to_struct((le), struct vma_struct, member)

#define VM_READ 0x00000001  // 只读权限
#define VM_WRITE 0x00000002 // 可写权限
#define VM_EXEC 0x00000004  // 可执行权限
#define VM_STACK 0x00000008 // 栈段标志

// the control struct for a set of vma using the same PDT
// 内存描述符 mm_struct
// 描述一个进程的所有虚拟内存区域 (VMA) 集合
struct mm_struct
{
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma (VMA 链表头)
    struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose (当前访问的 VMA 缓存，用于加速查找)
    pde_t *pgdir;                  // the PDT of these vma (页目录基地址)
    int map_count;                 // the count of these vma (VMA 数量)
    void *sm_priv;                 // the private data for swap manager (交换管理器私有数据)
    int mm_count;                  // the number ofprocess which shared the mm (共享该 mm 的进程数 / 引用计数)
    lock_t mm_lock;                // mutex for using dup_mmap fun to duplicat the mm (互斥锁，用于 dup_mmap 操作)
};

// 锁定mm结构
static inline void lock_mm(struct mm_struct *mm) {
    if (mm != NULL) {
        lock(&(mm->mm_lock));
    }
}

// 解锁mm结构
static inline void unlock_mm(struct mm_struct *mm) {
    if (mm != NULL) {
        unlock(&(mm->mm_lock));
    }
}

// 尝试获取锁
static inline bool try_lock_mm(struct mm_struct *mm) {
    if (mm != NULL) {
        return try_lock(&(mm->mm_lock));
    }
    return 0;
}

struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

struct mm_struct *mm_create(void);
void mm_destroy(struct mm_struct *mm);

void vmm_init(void);
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store);
int mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len);
int dup_mmap(struct mm_struct *to, struct mm_struct *from);
void exit_mmap(struct mm_struct *mm);
uintptr_t get_unmapped_area(struct mm_struct *mm, size_t len);
int mm_brk(struct mm_struct *mm, uintptr_t addr, size_t len);

extern volatile unsigned int pgfault_num;
extern struct mm_struct *check_mm_struct;

// 检查用户内存访问权限
bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write);
// 从用户空间复制数据到内核空间
bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable);
// 从内核空间复制数据到用户空间
bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len);

static inline int
mm_count(struct mm_struct *mm)
{
    return mm->mm_count;
}

static inline void
set_mm_count(struct mm_struct *mm, int val)
{
    mm->mm_count = val;
}

static inline int
mm_count_inc(struct mm_struct *mm)
{
    mm->mm_count += 1;
    return mm->mm_count;
}

static inline int
mm_count_dec(struct mm_struct *mm)
{
    mm->mm_count -= 1;
    return mm->mm_count;
}

// 处理缺页异常的核心函数
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

#endif /* !__KERN_MM_VMM_H__ */
