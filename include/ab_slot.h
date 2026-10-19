/*
 * A/B boot slot selection interface
 *
 * 全部 A/B 槽位逻辑集中于 lib_mips/ab_slot.c, 对外仅暴露本头文件中的接口,
 * board.c 仅在默认启动路径调用一次 ab_slot_kernel_addr()。
 */
#ifndef __AB_SLOT_H
#define __AB_SLOT_H

/* 槽位标识 */
#define AB_SLOT_ID_A	'A'
#define AB_SLOT_ID_B	'B'

/*
 * 主启动入口: 完成槽位选择 -> 未确认超限自动回退 -> 启动尝试计数,
 * 返回当前应启动槽位的内核地址。
 */
ulong ab_slot_kernel_addr(void);

/* 打印两槽位状态(地址/确认标志/尝试次数) */
void ab_slot_show(void);

/* 手动切换活动槽位并清零目标槽位计数, 成功返回 0 */
int ab_slot_switch(char slot);

#endif /* __AB_SLOT_H */
