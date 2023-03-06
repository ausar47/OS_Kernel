.altmacro

# sd onto the stack
.macro SAVE reg, offset
    sd \reg, \offset*8(sp)
.endm

.macro SAVE_N n
    SAVE x\n, \n
.endm

# ld from the stack
.macro LOAD reg, offset
    ld  \reg, \offset*8(sp)
.endm

.macro LOAD_N n
    LOAD  x\n, \n
.endm

    .section .text.entry
    .align 2
    .globl _traps
_traps:
    # YOUR CODE HERE
    # 1. save 32 registers and sepc to stack

    addi sp, sp, -33*8
    # x0 is always zero, no need to save
    SAVE x1, 1
    # save x2 before sp is changed
    addi x1, sp, 33*8
    SAVE x1, 2
    # loop 29 times beginning from x3 to sd registers
    .set n, 3
    .rept 29
        SAVE_N %n
        .set n, n+1
    .endr
    # save sepc
    csrr t0, sepc
    SAVE t0, 32

    # 2. call trap_handler

    # trap_handler needs two arguments a0 and a1, so we store the value of scause and sepc into a0 and a1 respectively
    csrr a0, scause
    csrr a1, sepc
    call trap_handler

    # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack

    LOAD t0, 32
    csrw sepc, t0
    LOAD x1, 1
    # loop 29 times beginning from x3 to ld registers
    .set n, 3
    .rept 29
        LOAD_N %n
        .set n, n + 1
    .endr
    # restore x2, aka sp
    LOAD x2, 2
    
    # 4. return from trap
    sret