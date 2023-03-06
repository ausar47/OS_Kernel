#pragma once
#include "stdint.h"

#define SYS_WRITE 64
#define SYS_GETPID 172

struct pt_regs {
  uint64_t x[32];
  uint64_t sepc;
  uint64_t sstatus;
};

uint64_t sys_write(unsigned int fd, const char* buf, uint64_t count);

uint64_t sys_getpid();

void syscall(struct pt_regs* regs);