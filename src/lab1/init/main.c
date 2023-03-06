#include "print.h"
#include "sbi.h"
#include "defs.h"

extern void test();

int start_kernel() {
    //csr_write(sstatus, 0x8000000000006102);
    //register uint64 v = csr_read(sstatus);
    puti(2022);
    puts(" Hello RISC-V\n"); 
    
    test(); // DO NOT DELETE !!!

    return 0;
}
