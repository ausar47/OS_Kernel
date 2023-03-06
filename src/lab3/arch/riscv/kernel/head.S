.extern mm_init
.extern task_init
.extern start_kernel

    .section .text.init
    .globl _start
_start:
    # ------------------
    # - your code here -
    # ------------------
    la sp, boot_stack_top	# sp point to top of the stack
    jal mm_init             # 初始化内存管理系统

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

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size (4KB)

    .globl boot_stack_top
boot_stack_top:
