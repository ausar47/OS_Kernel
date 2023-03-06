#include "syscall.h"
#include "printk.h"
#include "proc.h"

extern struct task_struct* current;

uint64_t sys_write(unsigned int fd, const char* buf, uint64_t count) {
    // 标准输出1
    if (fd == 1) {
        return fd = printk(buf);
    }
}

uint64_t sys_getpid() {
    return current->pid;
}

void syscall(struct pt_regs* regs) {
    if (regs->x[17] == SYS_WRITE) {
        regs->x[10] = sys_write(regs->x[10], (const char*)regs->x[11], regs->x[12]);
    }
    else if (regs->x[17] == SYS_GETPID) {
        regs->x[10] = sys_getpid(); // x10 = a0
    }
}