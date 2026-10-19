/*
 * abslot command: show / switch A/B boot slot
 *
 * 薄接口层, 全部逻辑复用 lib_mips/ab_slot.c:
 *   abslot        显示两槽位状态
 *   abslot A|B    手动切换活动槽位
 */

#include <common.h>
#include <command.h>
#include <ab_slot.h>

int do_abslot(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	/* 无参数: 显示两槽位状态 */
	if (argc == 1) {
		ab_slot_show();
		return 0;
	}

	/* 单参数 A 或 B: 手动切换 */
	if (argc == 2 && argv[1][1] == '\0' &&
	    (argv[1][0] == AB_SLOT_ID_A || argv[1][0] == AB_SLOT_ID_B))
		return ab_slot_switch(argv[1][0]);

	printf("Usage:\n%s\n%s", cmdtp->usage, cmdtp->help);
	return 1;
}

#ifdef RALINK_CMDLINE
U_BOOT_CMD(
	abslot,	2,	1,	do_abslot,
	"abslot  - show or switch A/B boot slot\n",
	"      - show current slot status\n"
	"abslot A|B - switch active boot slot\n"
);
#endif /* RALINK_CMDLINE */
