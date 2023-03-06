#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "printk.h"
#include "defs.h"
#include "string.h"
#include "vm.h"
#include "elf.h"

extern void __dummy();
extern uint64_t uapp_start;
extern uint64_t uapp_end;
extern unsigned long* swapper_pg_dir;

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

static uint64_t load_elf_program(struct task_struct* task) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)(&uapp_start);
    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;
    Elf64_Phdr* phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // 代码和数据区域
            uint64_t pg_num = (PGOFFSET(phdr->p_vaddr) + phdr->p_memsz - 1) / PGSIZE + 1;
            do_mmap(task, phdr->p_vaddr, pg_num * PGSIZE, (phdr->p_flags << 1), (uint64_t)&uapp_start, phdr->p_offset, phdr->p_filesz);
        }
    }

    // 用户栈，范围为 [USER_END - PGSIZE, USER_END) ，权限为 VM_READ | VM_WRITE, 并且是匿名的区域。
    do_mmap(task, USER_END - PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, (uint64_t)&uapp_start, 0, 0);

    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // other task setting keep same

    uint64_t sstatus = csr_read(sstatus);
    sstatus &= ~(1ul << 8);     // set sstatus[SPP] = 0
    sstatus |= 1ul << 5;        // set sstatus[SPIE] = 1
    sstatus |= 1ul << 18;       // set sstatus[SUM] = 1
    task->thread.sstatus = sstatus;
    task->thread.sscratch = USER_END;
}

void task_init() {
    memset(task, 0, NR_TASKS * sizeof(task));

    uint64_t addr_idle = kalloc();
    idle = (struct task_struct*)addr_idle;
    idle->state = TASK_RUNNING;
    idle->counter = idle->priority = 0;
    idle->pid = 0;
    idle->pgd = swapper_pg_dir;
    idle->thread.sscratch = 0;

    current = task[0] = idle;

    task[1] = (struct task_struct*)kalloc();
    task[1]->state = TASK_RUNNING;
    task[1]->counter = 0;
    task[1]->priority = rand() % 10 + 1;
    task[1]->pid = 1;

    pagetable_t pgtbl = (pagetable_t)kalloc();
    memcpy((void*)(pgtbl), (void*)((&swapper_pg_dir)), PGSIZE); // 内核页表也复制到进程的页表中
    task[1]->pgd = (pagetable_t)VA2PA((uint64_t)pgtbl);

    load_elf_program(task[1]);

    task[1]->thread.ra = (uint64_t)__dummy;
    task[1]->thread.sp = (uint64_t)task[1] + PGSIZE;

    printk("...proc_init done!\n");
}

// 创建⼀个新的 vma
void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags, uint64_t file_offset_on_disk, uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file) {
    struct vm_area_struct* vma = &(task->vmas[task->vma_cnt++]);
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_flags = flags;
    vma->file_offset_on_disk = file_offset_on_disk;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
    vma->has_alloc = 0;
}

// 查找包含某个 addr 的 vma
struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr) {
    for (int i = 0; i < task->vma_cnt; i++) {
        if (task->vmas[i].vm_start <= addr && addr < task->vmas[i].vm_end) {
            return &(task->vmas[i]);
        }
    }
    return NULL;
}


extern void __switch_to(struct task_struct* prev, struct task_struct* next);

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
        if (current->counter == 0) {
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

void SJF_schedule(){
    uint64_t min = 20;
    struct task_struct* min_task;

    while (1) {
        for (int i = 1; i < NR_TASKS; i++) {
            if (task[i] == NULL) continue;
            if (task[i]->counter < min && task[i]->counter > 0 && task[i]->state == TASK_RUNNING) {
                min = task[i]->counter;
                min_task = task[i];
            }
        }
        // 所有运行状态下的线程运行剩余时间都为0，对 task[1] ~ task[NR_TASKS-1] 的运行剩余时间重新赋值，之后再重新进行调度。
        if (min == 20) {
            for (int i = 1; i < NR_TASKS; i++) {
                if (task[i] == NULL) continue;
                task[i]->counter = rand() % 10 + 1;
                if (i == 1) {
                    printk("\n");
                }
                printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
            }
        } else break;
    }
    if (min_task) {
        switch_to(min_task);
    }
}

void Priority_schedule(){
    uint64_t max = -1;
    struct task_struct* max_task;

    while (1) {
        for (int i = 1; i < NR_TASKS; i++) {
            if (task[i] == NULL) continue;
            if (task[i]->priority > max && task[i]->counter > 0 && task[i]->state == TASK_RUNNING) {
                max = task[i]->priority;
                max_task = task[i];
            }
        }
        // 所有运行状态下的线程运行剩余时间都为0，对 task[1] ~ task[NR_TASKS-1] 的运行剩余时间重新赋值，之后再重新进行调度。
        if (max == -1) {
            for (int i = 1; i < NR_TASKS; i++) {
                if (task[i] == NULL) continue;
                task[i]->counter = rand() % 10 + 1;
                if (i == 1) {
                    printk("\n");
                }
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
            }
        } else break;
    }
    
    switch_to(max_task);
}