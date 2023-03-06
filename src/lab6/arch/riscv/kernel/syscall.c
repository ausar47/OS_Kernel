#include "syscall.h"
#include "defs.h"
#include "printk.h"
#include "proc.h"
#include "string.h"
#include "vm.h"
#include "mm.h"

extern struct task_struct* current;

uint64_t sys_write(unsigned int fd, const char* buf, uint64_t count) {
    uint64_t res = 0;
    for (uint64_t i = 0; i < count; i++) {
        if (fd == 1) {
            printk("%c", buf[i]);
            res++;
        }
    }
    return res;
}

uint64_t sys_getpid() {
    return current->pid;
}

extern struct task_struct* task[];
extern uint64_t __ret_from_fork;
extern unsigned long* swapper_pg_dir;

uint64_t sys_clone(struct pt_regs *regs) {
    /*
    1. 参考 task_init 创建⼀个新的 task, 将的 parent task 的整个⻚复制到新创建的
    task_struct ⻚上(这⼀步复制了哪些东⻄?）。将 thread.ra 设置为
    __ret_from_fork, 并正确设置 thread.sp
    (仔细想想，这个应该设置成什么值?可以根据 child task 的返回路径来倒推)
    2. 利⽤参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
    并将其中的 a0, sp, sepc 设置成正确的值(为什么还要设置 sp?)
    3. 为 child task 申请 user stack, 并将 parent task 的 user stack
    数据复制到其中。
    3.1. 同时将⼦ task 的 user stack 的地址保存在 thread_info->
    user_sp 中，如果你已经去掉了 thread_info，那么⽆需执⾏这⼀步
    4. 为 child task 分配⼀个根⻚表，并仿照 setup_vm_final 来创建内核空间的映射
    5. 根据 parent task 的⻚表和 vma 来分配并拷⻉ child task 在⽤户态会⽤到的内存
    6. 返回⼦ task 的 pid
    */

    // 创建一个新的task
    int pid = 1;
    while(task[pid] && pid < NR_TASKS) pid++; // 找到第一个pid还没有被占用的task
    if (pid == NR_TASKS) {
        printk("Maximum threads number exceeds.\n");
        return -1;
    }

    task[pid] = (struct task_struct*)kalloc();
    memcpy(task[pid], current, PGSIZE);                     // 将 parent task 的整个⻚复制到新创建的 task_struct ⻚上
    task[pid]->pid = pid;
    task[pid]->thread.ra = (uint64_t)(&__ret_from_fork);    // 修改 task_struct->thread.ra ，让程序ret时，直接跳转到设置的 symbol __ret_from_fork
    
    // 利⽤参数 regs 来计算出 child task 的对应的 pt_regs 的地址
    struct pt_regs *child_regs = (struct pt_regs*)((uint64_t)task[pid] + PGOFFSET((uint64_t)regs));
    task[pid]->thread.sp = (uint64_t)child_regs;
    child_regs->reg[9] = 0;                     // x10，sys_clone后返回值在x10中，父进程返回子进程的pid，子进程返回0（fork的原理）
    child_regs->reg[1] = task[pid]->thread.sp;  // x2，改成子进程的sp (为什么还要设置 sp? ———— trap_handler之前存进的是内核的sp)
    child_regs->sepc = regs->sepc + 4;          // 父进程异常处理完后在trap_handler中执行sepc加四，子进程就在这里完成sepc加四

    // 为 child task 分配⼀个根⻚表，并仿照 setup_vm_final 来创建内核空间的映射
    pagetable_t pgtbl = (pagetable_t)kalloc();
    memcpy((void*)pgtbl, (void*)(&swapper_pg_dir), PGSIZE);
    task[pid]->pgd = (pagetable_t)VA2PA((uint64_t)pgtbl);

    // 根据 parent task 的⻚表和 vma 来分配并拷⻉ child task 在⽤户态会⽤到的内存
    for (int i = 0; i < current->vma_cnt; i++) {
        struct vm_area_struct *vma = &(current->vmas[i]);
        // 只拷贝已经分配并且映射的页
        if (vma->has_alloc) {
            uint64_t cur = vma->vm_start;
            while (cur < vma->vm_end) {
                uint64_t page = kalloc();
                create_mapping((uint64*)PA2VA((uint64_t)task[pid]->pgd), PGROUNDDOWN(cur), VA2PA(page), 1, vma->vm_flags | 16 | 1);
                memcpy((void*)page, (void*)PGROUNDDOWN(cur), PGSIZE);
                cur += PGSIZE;
            }
        }
    }

    // 返回⼦ task 的 pid
    printk("[S] New task: %d\n", pid);
    return pid;
}