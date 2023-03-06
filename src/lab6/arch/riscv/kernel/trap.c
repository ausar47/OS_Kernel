#include "clock.h"
#include "printk.h"
#include "types.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"
#include "mm.h"
#include "vm.h"
#include "string.h"

extern struct task_struct* current;

void do_page_fault(struct pt_regs *regs) {
    /*
    1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
    2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
    3. 分配⼀个⻚，将这个⻚映射到对应的⽤户地址空间
    4. 通过 (vma->vm_flags | VM_ANONYM) 获得当前的 VMA 是否是匿名空间
    5. 根据 VMA 匿名与否决定将新的⻚清零或是拷⻉ uapp 中的内容
    */
    if (regs->scause == 0xc)
        printk("[S] Supervisor Page Fault, scause: %lx, stval: %lx\n", regs->scause, regs->stval);
    else
        printk("[S] Supervisor Page Fault, scause: %lx, stval: %lx, sepc: %lx\n", regs->scause, regs->stval, regs->sepc);
    // 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
    struct vm_area_struct* vma = find_vma(current, regs->stval);

    // 如果找到的是NULL，说明 Bad Address 不在 vma 中，即地址无效，直接返回
    if (!vma) {
        return;
    }

    // 分配⼀个⻚，将这个⻚映射到对应的⽤户地址空间
    uint64_t page = alloc_page();
    create_mapping((uint64*)PA2VA((uint64_t)current->pgd), PGROUNDDOWN(regs->stval), VA2PA(page), 1, vma->vm_flags | 16 | 1);

    // 非匿名空间，说明不是U-Stack，需要拷贝uapp中的内容
    if (!(vma->vm_flags & VM_ANONYM)) {
        uint64_t load_addr = (vma->file_offset_on_disk + vma->vm_content_offset_in_file);
        memcpy((void*)(page + PGOFFSET(regs->stval)), (void*)(load_addr) + regs->stval - vma->vm_start, MIN(PGSIZE - PGOFFSET(regs->stval), vma->vm_content_size_in_file - regs->stval + vma->vm_start));
    }
    // 匿名空间，将该新的页清零
    else {
        memset((void*)page, 0, PGSIZE);
    }

    // 记录下这个vma有地址分配过页了
    vma->has_alloc = 1;
}


void trap_handler(uint64 scause, uint64 sepc, struct pt_regs *regs) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` ⻅ 4.5 节
    // 其他interrupt / exception 可以直接忽略

    // 判断是否是interrupt
    if (scause & (1UL << 63)) {
        // timer interrupt
        if (scause - (1UL << 63) == 5) {
            printk("[S] Supervisor Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
        }
        // 忽略其他interrupt
        else {}
    }
    // 处理ECALL_FROM_U_MODE exception
    else if (scause == 8) {
        if (regs->reg[16] == SYS_WRITE)
            regs->reg[9] = sys_write(regs->reg[9], (const char*)regs->reg[10], regs->reg[11]);
        else if (regs->reg[16] == SYS_GETPID)
            regs->reg[9] = sys_getpid();
        else if (regs->reg[16] == SYS_CLONE)
            regs->reg[9] = sys_clone(regs);
        regs->sepc += 4;
    }
    // 处理pagefault exception
    else if (scause == 12 || scause == 13 || scause == 15) {
        do_page_fault(regs);
    }
    else {
        printk("[S] Unhandled trap, ");
        printk("scause: %lx, ", scause);
        printk("stval: %lx, ", regs->stval);
        printk("sepc: %lx\n", regs->sepc);
        while (1);
    }
}