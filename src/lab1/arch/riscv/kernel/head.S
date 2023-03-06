.extern start_kernel

    .section .text.entry
    .globl _start
_start:
    # ------------------
    # - your code here -
    # ------------------
    la sp, boot_stack_top	# sp point to top of the stack
    jal start_kernel		# call function start_kernel()

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size (4KB)

    .globl boot_stack_top
boot_stack_top:
