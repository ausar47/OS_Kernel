#include "defs.h"

.extern mm_init
.extern task_init
.extern start_kernel
.extern early_pgtbl
.extern setup_vm
.extern setup_vm_final

    .section .text.init
    .globl _start
_start:
    # ------------------
    # - your code here -
    # ------------------
    la sp, boot_stack_top	# sp point to top of the stack

    jal setup_vm
    jal relocate

    jal mm_init             # 初始化内存管理系统

    jal setup_vm_final

    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0
    # set sie[STIE] = 1
    csrr t0, sie
    ori t0, t0, 1<<5
    csrw sie, t0
    # set first time interrupt
    jal clock_set_next_event
    # set sstatus[SIE] = 1
    csrsi sstatus, 1<<1

    jal task_init

    jal start_kernel		# call function start_kernel()

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)

    ######################
    #   YOUR CODE HERE   #
    ######################
    li t0, PA2VA_OFFSET
    add ra, ra, t0
    add sp, sp, t0

    # set satp with early_pgtbl

    ######################
    #   YOUR CODE HERE   #
    ######################
    li t0, 8
    slli t0, t0, 60     # Sv39, Mode Value = 8
    la t1, early_pgtbl
    srli t1, t1, 12     # page table base address >> 12 == PPN 
    add t0, t0, t1
    csrw satp, t0

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size (4KB)

    .globl boot_stack_top
boot_stack_top:
