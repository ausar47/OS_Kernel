.altmacro

# save
.macro SAVE reg1, reg2, offset
    sd \reg1, \offset*8(\reg2)
.endm

# save register xn
.macro SAVE_X_N reg, n
    SAVE x\n, \reg, \n
.endm

#save register sn
.macro SAVE_S_N reg, n
    SAVE s\n, \reg, \n
.endm

# load
.macro LOAD reg1, reg2, offset
    ld \reg1, \offset*8(\reg2)
.endm

# load register xn
.macro LOAD_X_N reg, n
    LOAD x\n, \reg, \n
.endm

# load register sn
.macro LOAD_S_N reg, n
    LOAD s\n, \reg, \n
.endm

    .section .text.entry
    .align 2
    .globl _traps
_traps:
    # YOUR CODE HERE
    # 1. save 32 registers and sepc to stack

    addi sp, sp, -33*8
    # x0 is always zero, no need to save
    SAVE x1, sp, 1
    # save x2 before sp is changed
    addi x1, sp, 33*8
    SAVE x1, sp, 2
    # loop 29 times beginning from x3 to sd registers
    .set n, 3
    .rept 29
        SAVE_X_N sp, %n
        .set n, n+1
    .endr
    # save sepc
    csrr t0, sepc
    SAVE t0, sp, 32

    # 2. call trap_handler

    # trap_handler needs two arguments a0 and a1, so we store the value of scause and sepc into a0 and a1 respectively
    csrr a0, scause
    csrr a1, sepc
    call trap_handler

    # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack

    LOAD t0, sp, 32
    csrw sepc, t0
    LOAD x1, sp, 1
    # loop 29 times beginning from x3 to ld registers
    .set n, 3
    .rept 29
        LOAD_X_N sp, %n
        .set n, n + 1
    .endr
    # restore x2, aka sp
    LOAD x2, sp, 2
    
    # 4. return from trap
    sret

    .global __dummy
__dummy:
    la t0, dummy
    csrw sepc, t0
    sret

    .global __switch_to
__switch_to:
    # save state to prev process
    # YOUR CODE HERE
    addi t0, a0, 40
    sd ra, 0(t0)
    sd sp, 8(t0)

    addi t0, t0, 16
    .set n, 0
    .rept 12
        SAVE_S_N t0, %n
        .set n, n + 1
    .endr

    # restore state from next process
    # YOUR CODE HERE
    addi t0, a1, 40
    ld ra, 0(t0)
    ld sp, 8(t0)

    addi t0, t0, 16
    .set n, 0
    .rept 12
        LOAD_S_N t0, %n
        .set n, n + 1
    .endr

    ret