// trap.c
#include "printk.h"
#include "clock.h"
#include "proc.h"

void trap_handler(unsigned long scause, unsigned long sepc) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` ⻅ 4.5 节
    // 其他interrupt / exception 可以直接忽略

    // 判断是否是interrupt
    if (scause & (1UL << 63) == (1UL << 63)) {
        // timer interrupt
        if (scause & (1UL << 4) == (1UL << 4)) {
            // printk("[S] Supervisor Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
        }
        // 忽略其他interrupt
        else {}
    }
    // 忽略exception
    else {}
}