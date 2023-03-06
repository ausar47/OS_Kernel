#include "defs.h"
#include "mm.h"
#include "printk.h"
#include "string.h"

extern unsigned long swapper_pg_dir[];

void setup_vm(void);

void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm);

void setup_vm_final(void);