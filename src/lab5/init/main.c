#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();

extern void schedule();

int start_kernel() {
    printk("[S-MODE] Hello RISC-V\n");
    
    schedule();

    test(); // DO NOT DELETE !!!

    return 0;
}
