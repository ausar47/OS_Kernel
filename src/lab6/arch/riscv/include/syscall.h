#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "stdint.h"
#include "proc.h"

#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_CLONE 220

uint64_t sys_write(unsigned int fd, const char* buf, uint64_t count);

uint64_t sys_getpid();

uint64_t sys_clone(struct pt_regs *regs); 

#endif