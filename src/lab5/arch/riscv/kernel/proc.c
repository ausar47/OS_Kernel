#include "proc.h"
#include "printk.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "elf.h"
#include <string.h>

extern void __dummy();
extern void __switch_to(struct task_struct* prev, struct task_struct* next);
extern char uapp_start[];
extern char uapp_end[];
extern unsigned long swapper_pg_dir[];

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/* 创建多级⻚表映射关系 */
void create_mapping_user(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    /*
    pgtbl 为根⻚表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的⼤⼩
    perm 为映射的读写权限，可设置不同section所在⻚的属性，完成对不同section的保护
    创建多级⻚表的时候可以使⽤ kalloc() 来获取⼀⻚作为⻚表⽬录
    可以使⽤ V bit 来判断⻚表项是否存在
    */
    uint64* pgtbls[3];      // 三个页表的起始地址
    unsigned int vpn[3];    // 虚拟地址的3个VPN
    uint64 pte;             // 页表中查到的entry
    uint64 end = va + sz;   // 映射结尾

    while (va < end) {
        // 第一级页表
        pgtbls[2] = pgtbl;
        vpn[2] = (va & 0x0000007FC0000000) >> 30;
        pte = *(uint64*)((uint64)pgtbls[2] + (vpn[2] << 3));
        // 如果页表项不存在，使用kalloc()来获取一页
        if (!(pte & 1)) {
            uint64* pg = (uint64*)kalloc();
            pte = ((((uint64)pg - PA2VA_OFFSET) >> 12) << 10) | 0x11;   // 现在都在虚拟地址上运行，要分配物理地址就还要转换一下
        }
        *(uint64*)((uint64)pgtbls[2] + (vpn[2] << 3)) = pte;

        // 第二级页表
        pgtbls[1] = (uint64*)((pte >> 10) << 12);
        vpn[1] = (va & 0x000000003FE00000) >> 21;
        pte = *(uint64*)((uint64)pgtbls[1] + (vpn[1] << 3) + PA2VA_OFFSET);
        // 如果页表项不存在，使用kalloc()来获取一页
        if (!(pte & 1)) {
            uint64* pg = (uint64*)kalloc();
            pte = ((((uint64)pg - PA2VA_OFFSET) >> 12) << 10) | 0x11;   // 现在都在虚拟地址上运行，要分配物理地址就还要转换一下
        }
        *(uint64*)((uint64)pgtbls[1] + (vpn[1] << 3) + PA2VA_OFFSET) = pte;

        // 第三级页表
        pgtbls[0] = (uint64*)((pte >> 10) << 12);
        vpn[0] = (va & 0x00000000001FF000) >> 12;
        pte = (perm & 15) | ((pa >> 12) << 10) | 0x10;
        *(uint64*)((uint64)pgtbls[0] + (vpn[0] << 3) + PA2VA_OFFSET) = pte;

        va += PGSIZE;
        pa += PGSIZE;
    }

    return;
}

void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    /* YOUR CODE HERE */
    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;
    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该线程在线程数组中的下标。
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址, `sp` 设置为该线程申请的物理页的高地址
    /* YOUR CODE HERE */
    for (int i = 1; i < NR_TASKS; i++) {
        task[i] = (struct task_struct*)kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand() % (PRIORITY_MAX - PRIORITY_MIN + 1) + PRIORITY_MIN;
        task[i]->pid = i;
        task[i]->thread.ra = (uint64_t)__dummy;
        task[i]->thread.sp = (uint64_t)task[i] + PGSIZE;

        pagetable_t pgtbl = (pagetable_t)kalloc();
        for(int j = 0; j < 512; j++){
            pgtbl[j] = swapper_pg_dir[j];
        } // 内核页表也复制到进程的页表中

        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)uapp_start;
        uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
        int phdr_cnt = ehdr->e_phnum;
        Elf64_Phdr* phdr;
        int load_phdr_cnt = 0;
        for (int i = 0; i < phdr_cnt; i++) {
            phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
            if (phdr->p_type == PT_LOAD) {
                // copy the program section to another space
                uint64_t pg_num = (PGOFFSET(phdr->p_vaddr) + phdr->p_memsz - 1) / PGSIZE + 1;
                uint64_t uapp_new = alloc_pages(pg_num);
                uint64_t load_addr = (uapp_start + phdr->p_offset);
                memcpy((void*)(uapp_new + PGOFFSET(phdr->p_vaddr)), (void*)(load_addr), phdr->p_memsz);
                // mapping the program section with corresponding size and flag
                create_mapping_user(pgtbl, PGROUNDDOWN(phdr->p_vaddr), VA2PA(uapp_new), pg_num * PGSIZE, ((phdr->p_flags >> 2) << 1) | ((phdr->p_flags & 0x2) << 1) | ((phdr->p_flags & 0x1) << 3) | 0x1);
            }
        }
        // pc for the user program
        task[i]->thread.sepc = ehdr->e_entry;
        // other task setting keep same

        // // 映射 UAPP
        // uint64_t pg_num = (uapp_end - uapp_start - 1) / PGSIZE + 1;
        // uint64_t uapp_new = alloc_pages(pg_num);                            // 分配内存
        // memcpy((void*)(uapp_new), (void*)(&uapp_start), pg_num * PGSIZE);   // 复制uapp
        // create_mapping_user(pgtbl, USER_START, VA2PA(uapp_new), uapp_end - uapp_start + 1, 15);

        // 映射 U-Mode Stack
        uint64_t va = USER_END - PGSIZE;
        uint64_t pa = (uint64_t)kalloc() - PA2VA_OFFSET; // 申请一个空的页面来作为 U-Mode Stack
        create_mapping_user(pgtbl, va, pa, PGSIZE, 7);

        uint64_t satp = csr_read(satp);
        satp = (satp >> 44) << 44;
        satp |= ((uint64_t)pgtbl - PA2VA_OFFSET) >> 12;
        task[i]->pgd = satp;

        // task[i]->thread.sepc = USER_START;
        uint64_t sstatus = csr_read(sstatus);
        sstatus &= ~(1ul << 8);     // set sstatus[SPP] = 0
        sstatus |= 1ul << 5;        // set sstatus[SPIE] = 1
        sstatus |= 1ul << 18;       // set sstatus[SUM] = 1
        task[i]->thread.sstatus = sstatus;
        task[i]->thread.sscratch = USER_END;
    }
    printk("...proc_init done!\n");
}

void dummy() {
    uint64_t MOD = 1000000007;
    uint64_t auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if (last_counter == -1 || current->counter != last_counter) {
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. thread space begin at = 0x%lx. \n", current->pid, (uint64_t)current);
        }
    }
}

void switch_to(struct task_struct* next) {
    /* YOUR CODE HERE */
    if (next != current) {
        #ifdef SJF
            printk("\n");
            printk("switch to [PID = %d COUNTER = %d]\n", next->pid, next->counter);
        #endif 

        #ifdef PRIORITY
            printk("\n");
            printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n", next->pid, next->priority, next->counter);
        #endif

        struct task_struct* prev = current;
        current = next;
        __switch_to(prev, next);
    }
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度
    /* YOUR CODE HERE */
    if (current == idle) {
        schedule();
    } else {
        current->counter--;
        if (current->counter <= 0) {
            schedule();
        }
    }
}

void schedule(void) {
    /* YOUR CODE HERE */
    #ifdef SJF
        SJF_schedule();
    #endif

    #ifdef PRIORITY
        Priority_schedule();
    #endif
}

void SJF_schedule() {
    uint64_t min = COUNTER_MAX + 1;
    struct task_struct* min_task;

    while (1) {
        for (int i = 1; i < NR_TASKS; i++) {
            if (task[i]->counter < min && task[i]->counter > 0 && task[i]->state == TASK_RUNNING) {
                min = task[i]->counter;
                min_task = task[i];
            }
        }
        // 所有运行状态下的线程运行剩余时间都为0，对 task[1] ~ task[NR_TASKS-1] 的运行剩余时间重新赋值，之后再重新进行调度。
        if (min == COUNTER_MAX + 1) {
            for (int i = 1; i < NR_TASKS; i++) {
                task[i]->counter = rand() % (COUNTER_MAX - COUNTER_MIN + 1) + COUNTER_MIN;
                // if (i == 1) {
                //     printk("\n");
                // }
                // printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
            }
        } else break;
    }
    
    switch_to(min_task);
}

void Priority_schedule() {
    uint64_t max = PRIORITY_MIN - 1;
    struct task_struct* max_task;

    while (1) {
        for (int i = 1; i < NR_TASKS; i++) {
            if (task[i]->priority > max && task[i]->counter > 0 && task[i]->state == TASK_RUNNING) {
                max = task[i]->priority;
                max_task = task[i];
            }
        }
        // 所有运行状态下的线程运行剩余时间都为0，对 task[1] ~ task[NR_TASKS-1] 的运行剩余时间重新赋值，之后再重新进行调度。
        if (max == PRIORITY_MIN - 1) {
            for (int i = 1; i < NR_TASKS; i++) {
                task[i]->counter = rand() % (COUNTER_MAX - COUNTER_MIN + 1) + COUNTER_MIN;
                // if (i == 1) {
                //     printk("\n");
                // }
                // printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
            }
        } else break;
    }
    
    switch_to(max_task);
}