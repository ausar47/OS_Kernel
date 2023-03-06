#include "defs.h"
#include "mm.h"
#include "printk.h"
#include <string.h>

extern char _stext[];
extern char _etext[];
extern char _srodata[];
extern char _erodata[];
extern char _sdata[];

/* early_pgtbl: ⽤于 setup_vm 进⾏ 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void) {
    /*
    1. 由于是进⾏ 1GB 的映射 这⾥不需要使⽤多级⻚表
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 ⻚内偏移 这⾥注意到 30 = 9 + 9 + 12， 即我们只使⽤根⻚表， 根⻚表的每个 entry 都对应 1GB 的区
        域。
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    // 初始化一个4KB的顶级物理页表
    memset(early_pgtbl, 0x0, PGSIZE);

    // 等值映射
    unsigned long PA = PHY_START, VA = PHY_START;
    unsigned int VPN2 = (VA & 0x0000007FC0000000) >> 30;
    // b'000001111 = 0xf, V | R | W | X 设置为1，剩下设置为0
    early_pgtbl[VPN2] = ((PA & 0x00FFFFFFC0000000) >> 2) | 0xf;

    // 映射到高地址
    VA = VM_START;
    VPN2 = (VA & 0x0000007FC0000000) >> 30;
    // b'000001111 = 0xf, V | R | W | X 设置为1，剩下设置为0
    early_pgtbl[VPN2] = ((PA & 0x00FFFFFFC0000000) >> 2) | 0xf;
}

/* swapper_pg_dir: kernel pagetable 根⽬录， 在 setup_vm_final 进⾏映射。*/
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

/* 创建多级⻚表映射关系 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    /*
    pgtbl 为根⻚表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的⼤⼩
    perm 为映射的读写权限，可设置不同section所在⻚的属性，完成对不同section的保护
    创建多级⻚表的时候可以使⽤ kalloc() 来获取⼀⻚作为⻚表⽬录
    可以使⽤ V bit 来判断⻚表项是否存在
    */
    uint64* pgtbls[3];      // 三个页表的起始地址
    unsigned int vpn[3];    // 虚拟地址的3个VPN
    uint64 pte;             // 页表中查到的entry
    uint64 end = va + sz;   // 映射结尾

    while (va < end) {
        // 第一级页表
        pgtbls[2] = pgtbl;
        vpn[2] = (va & 0x0000007FC0000000) >> 30;
        pte = *(uint64*)((uint64)pgtbls[2] + (vpn[2] << 3));
        // 如果页表项不存在，使用kalloc()来获取一页
        if (!(pte & 1)) {
            uint64* pg = (uint64*)kalloc();
            pte = ((((uint64)pg - PA2VA_OFFSET) >> 12) << 10) | 1;   // 现在都在虚拟地址上运行，要分配物理地址就还要转换一下
        }
        *(uint64*)((uint64)pgtbls[2] + (vpn[2] << 3)) = pte;

        // 第二级页表
        pgtbls[1] = (uint64*)((pte >> 10) << 12);
        vpn[1] = (va & 0x000000003FE00000) >> 21;
        // pte = pgtbls[1][vpn[1]];
        pte = *(uint64*)((uint64)pgtbls[1] + (vpn[1] << 3) + PA2VA_OFFSET);
        // 如果页表项不存在，使用kalloc()来获取一页
        if (!(pte & 1)) {
            uint64* pg = (uint64*)kalloc();
            pte = ((((uint64)pg - PA2VA_OFFSET) >> 12) << 10) | 1;   // 现在都在虚拟地址上运行，要分配物理地址就还要转换一下
        }
        *(uint64*)((uint64)pgtbls[1] + (vpn[1] << 3) + PA2VA_OFFSET) = pte;

        // 第三级页表
        pgtbls[0] = (uint64*)((pte >> 10) << 12);
        vpn[0] = (va & 0x00000000001FF000) >> 12;
        pte = (perm & 15) | ((pa >> 12) << 10);
        *(uint64*)((uint64)pgtbls[0] + (vpn[0] << 3) + PA2VA_OFFSET) = pte;

        va += PGSIZE;
        pa += PGSIZE;
    }

    return;
}

void setup_vm_final(void) {
    memset(swapper_pg_dir, 0x0, PGSIZE);

    // No OpenSBI mapping required
    
    // mapping kernel text X|-|R|V
    uint64 va = VM_START + OPENSBI_SIZE;
    uint64 pa = PHY_START + OPENSBI_SIZE;
    uint64 section_length = _srodata - _stext;
    create_mapping(swapper_pg_dir, va, pa, section_length, 11);

    // mapping kernel rodata -|-|R|V
    va += section_length;
    pa += section_length;
    section_length = _sdata - _srodata;
    create_mapping(swapper_pg_dir, va, pa, section_length, 3);

    // mapping other memory -|W|R|V
    va += section_length;
    pa += section_length;
    section_length = PHY_SIZE - OPENSBI_SIZE - (_sdata - _stext);
    create_mapping(swapper_pg_dir, va, pa, section_length, 7);

    // set satp with swapper_pg_dir
    uint64 pg_dir = (uint64)swapper_pg_dir - PA2VA_OFFSET;
    __asm__ volatile (
        "li t0, 8\n"
        "slli t0, t0, 60\n"
        "mv t1, %0\n"
        "srli t1, t1, 12\n"
        "add t0, t0, t1\n"
        "csrw satp, t0"
        :
        : "r"(pg_dir)
        : "memory"
    );

    // flush TLB
    __asm__ volatile ("sfence.vma zero, zero");

    // unsigned long int*p = 0xffffffe000202000;
    // unsigned long int raw = *p;
    // *p = raw;

    return;
}