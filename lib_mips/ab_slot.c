/*
 * A/B boot slot selection for MT7628DAN
 *
 * 设计说明:
 *   高内聚: 槽位选择/自动回退/启动计数/状态查询的全部逻辑集中本文件;
 *   低耦合: 对外仅 3 个接口(见 include/ab_slot.h), board.c 默认启动路径
 *           一处调用, 关闭 AB_SLOT_BOOT 后与原版行为完全一致。
 *
 * env 协议(与 Linux 侧 fw_setenv 对齐, 变量名集中定义于此):
 *   boot_slot      当前活动槽位, "A" 或 "B" (缺省 A)
 *   slot_x_stable  槽位已被 Linux 确认可正常启动, "1"=是 (缺省 0, 未确认)
 *   slot_x_try     未确认状态下该槽位启动尝试次数 (缺省 0)
 *
 * 运行机制:
 *   1. 每次启动前, 未确认槽位的 try 计数 +1 并写回 env;
 *   2. 连续 AB_MAX_TRY_TIMES 次未获确认(固件起不来没人清计数) -> 自动
 *      切换到另一槽位并清零其计数(旧固件回退, 无需人工干预);
 *   3. 已确认槽位(stable=1)不计数不写 env, 避免每次启动擦写 flash;
 *   4. 全新出厂 env(变量不存在)时全部走缺省值, 不会出现空指针。
 *
 * 升级流程(Linux 侧, 以切到 B 槽为例):
 *   mtd write 新镜像到槽B分区
 *   fw_setenv boot_slot B
 *   fw_setenv slot_b_stable 0
 *   fw_setenv slot_b_try 0
 *   reboot
 * 新固件确认可正常运行后:
 *   fw_setenv slot_b_stable 1
 *   fw_setenv slot_b_try 0
 */

#include <common.h>
#include <ab_slot.h>

/* env 变量名(集中定义, 与 Linux 侧协议保持一致) */
#define AB_ENV_SLOT		"boot_slot"
#define AB_ENV_STABLE_FMT	"slot_%c_stable"
#define AB_ENV_TRY_FMT		"slot_%c_try"

/* 读取当前活动槽位(缺省/无效值回落到 A, 对 getenv 返回 NULL 安全) */
static char ab_active_slot(void)
{
	char *s = getenv(AB_ENV_SLOT);

	if (s != NULL && *s == AB_SLOT_ID_B)
		return AB_SLOT_ID_B;
	return AB_SLOT_ID_A;
}

/* 槽位是否已被 Linux 确认(缺省视为未确认) */
static int ab_slot_is_stable(char slot)
{
	char name[16];
	char *s;

	sprintf(name, AB_ENV_STABLE_FMT, slot);
	s = getenv(name);
	return (s != NULL && simple_strtoul(s, NULL, 10) == 1);
}

/* 读取槽位未确认启动尝试次数(缺省 0) */
static int ab_slot_try_get(char slot)
{
	char name[16];
	char *s;

	sprintf(name, AB_ENV_TRY_FMT, slot);
	s = getenv(name);
	if (s == NULL)
		return 0;
	return (int)simple_strtoul(s, NULL, 10);
}

/* 写入槽位未确认启动尝试次数 */
static void ab_slot_try_set(char slot, int val)
{
	char name[16];
	char val_str[8];

	sprintf(name, AB_ENV_TRY_FMT, slot);
	sprintf(val_str, "%d", val);
	setenv(name, val_str);
}

/* 切换活动槽位并清零目标槽位计数 */
static void ab_switch_to(char slot)
{
	char buf[2];

	buf[0] = slot;
	buf[1] = '\0';
	ab_slot_try_set(slot, 0);
	setenv(AB_ENV_SLOT, buf);
	saveenv();
}

/*
 * 主启动入口: 槽位选择 -> 超限自动回退 -> 未确认启动计数,
 * 返回当前槽位内核地址。board.c 默认启动路径唯一调用点。
 */
ulong ab_slot_kernel_addr(void)
{
	char slot, other;

	slot = ab_active_slot();
	other = (slot == AB_SLOT_ID_A) ? AB_SLOT_ID_B : AB_SLOT_ID_A;

	/* 未确认槽位连续启动失败超限 -> 自动切换到另一槽位 */
	if (!ab_slot_is_stable(slot) && ab_slot_try_get(slot) >= AB_MAX_TRY_TIMES) {
		printf("AB: slot %c boot failed %d times, switch to slot %c\n",
			slot, AB_MAX_TRY_TIMES, other);
		ab_switch_to(other);
		slot = other;
	}

	/* 未确认槽位记录一次启动尝试(Linux 确认后不再写 env, 减少擦写) */
	if (!ab_slot_is_stable(slot)) {
		ab_slot_try_set(slot, ab_slot_try_get(slot) + 1);
		saveenv();
	}

	return (slot == AB_SLOT_ID_A) ? AB_SLOT_A_ADDR : AB_SLOT_B_ADDR;
}

/* 打印两槽位状态(abslot 命令) */
void ab_slot_show(void)
{
	char slot = ab_active_slot();

	printf("boot_slot = %c (active)\n", slot);
	printf("slot A: addr=0x%08lX stable=%d try=%d\n",
		(ulong)AB_SLOT_A_ADDR,
		ab_slot_is_stable(AB_SLOT_ID_A), ab_slot_try_get(AB_SLOT_ID_A));
	printf("slot B: addr=0x%08lX stable=%d try=%d\n",
		(ulong)AB_SLOT_B_ADDR,
		ab_slot_is_stable(AB_SLOT_ID_B), ab_slot_try_get(AB_SLOT_ID_B));
}

/* 手动切换活动槽位(abslot A|B 命令), 成功返回 0 */
int ab_slot_switch(char slot)
{
	if (slot != AB_SLOT_ID_A && slot != AB_SLOT_ID_B)
		return 1;

	ab_switch_to(slot);
	printf("AB: boot slot switched to %c\n", slot);
	return 0;
}
