# MT7628DAN U-Boot

MT7628DAN 平台 Bootloader（MIPS24Kc / 64MB DDR2 / 8MB SPI Flash），
交叉工具链随仓库附带，克隆即可一键编译。

## 一键编译

```bash
./loadcfg.sh
```

脚本自动完成（顺序执行）：
1. **首次运行**自动解压工具链 `toolchains/buildroot-gcc342.tar.bz2` → `toolchains/buildroot-gcc342/`（已解压则跳过）
2. 检测 32 位运行时依赖（GCC 3.4.2 为 32 位程序，缺失时提示安装命令）
3. 加载 64MB DDR2 配置预设（`configs/mt7628dan_64MB/`）
4. 全量编译，产出 `uboot.bin`（约 108KB）

## 系统依赖（仅一项）

64 位 Ubuntu/Debian 需安装 32 位运行时：

```bash
sudo apt install libc6-i386
```

> 不需要 Java：本工程内置网页均为 HTML，构建过程不调用 Java。

## 其他用法

```bash
./loadcfg.sh <预设名>              # 加载指定预设并编译
./loadcfg.sh <预设名> <命令...>    # 加载预设后执行任意命令（自动带工具链路径）
```

## 配置预设机制

本 SDK 的 `.config`（make 变量）与 `autoconf.h`（C 宏）必须成对一致，`loadcfg.sh`
从 `configs/<预设名>/` 成对加载并做一致性校验，因此**无需 menuconfig**。

新增预设（如换 DDR 颗粒）：复制 `configs/mt7628dan_64MB/` 下两个文件，修改
`ON_BOARD_512M_DRAM_COMPONENT` 对应行即可（可选值参考根目录 `config.in` 中 MT7628 段）。

## 编译产物与烧写

| 文件 | 用途 |
|---|---|
| `uboot.bin` | 烧写到 SPI flash 偏移 **0x0**（`u-boot` ELF 仅供调试，勿烧写） |

### 8MB Flash 分区布局

| 偏移 | 大小 | 分区 | 说明 |
|---|---|---|---|
| 0x000000 | 192KB | u-boot | 本工程产物 |
| 0x030000 | 64KB | u-boot-env | 环境变量 |
| 0x040000 | 64KB | factory | **WiFi 校准数据 + MAC，勿覆盖** |
| 0x050000 | 7872KB | firmware | 系统固件（OpenWrt 等） |

### Web 救砖（免拆机）

设备启动时按住 WPS/RESET 键，进入 U-Boot 内置 HTTP 升级模式（设备 IP 192.168.1.111）：

```bash
# 升级 U-Boot
curl -F uboot=@./uboot.bin -F press="Update U-Boot" 192.168.1.111 > /dev/null
# 升级固件
curl -F firmware=@./openwrt.bin -F press="Update firmware" 192.168.1.111 > /dev/null
```

## A/B 双槽启动

本工程内置 A/B 槽位启动选择（`AB_SLOT_BOOT`，默认已启用），8MB flash 布局：

| 槽位 | flash 偏移 | 说明 |
|---|---|---|
| A | 0x50000 | 与单镜像布局一致 |
| B | 0x450000 | 备用槽位 |

**机制**：每次启动未确认槽位的 `slot_x_try` 计数 +1；连续 3 次未获 Linux 确认
（固件起不来没人清计数）→ U-Boot 自动切回另一槽位（旧固件回退，免人工干预）。
已确认槽位不计数、不写 env（避免 flash 磨损）。

**env 协议**（Linux 侧用 `fw_setenv` 操作）：

| 变量 | 含义 |
|---|---|
| `boot_slot` | 当前活动槽位 A/B（缺省 A） |
| `slot_a_stable` / `slot_b_stable` | 槽位已确认可正常启动（1=是） |
| `slot_a_try` / `slot_b_try` | 未确认状态启动尝试次数 |

**升级流程**（以切到 B 槽为例，OpenWrt 侧）：

```bash
mtd write 新固件 <槽B的mtd分区>
fw_setenv boot_slot B
fw_setenv slot_b_stable 0
fw_setenv slot_b_try 0
reboot
# 新固件验证正常后执行确认:
fw_setenv slot_b_stable 1
fw_setenv slot_b_try 0
```

**U-Boot 控制台命令**：

```
abslot          # 显示两槽位状态
abslot A|B      # 手动切换活动槽位
```

### 开启 / 关闭 A/B 功能（调试用）

A/B 功能由宏 `AB_SLOT_BOOT` 整体控制（menuconfig 项 "A/B Slot Boot"，config.in:1511）。
**关闭后 `ab_slot.c` / `cmd_abslot.c` 完全不参与编译**（无 abslot 命令、无协议代码），
启动行为与未引入该功能前完全一致，产物体积逐字节还原。

**推荐方式：切换预设**（免改文件，两套固件随时对照烧写）

```bash
./loadcfg.sh mt7628dan_64MB_noab   # 关闭 A/B（调试对照版）→ uboot.bin 110,376 字节
./loadcfg.sh                        # 默认预设（A/B 开启）→ uboot.bin 112,272 字节
```

**手动方式：改预设两行**（`configs/mt7628dan_64MB/`，改完 `./loadcfg.sh` 生效）

| 文件 | 开启 | 关闭 |
|---|---|---|
| `config` | `AB_SLOT_BOOT=y` | `# AB_SLOT_BOOT is not set` |
| `autoconf.h` | `#define AB_SLOT_BOOT 1` | `#undef  AB_SLOT_BOOT` |

> 两种状态产物仅差 1.9KB（即 A/B 模块本体）；`AB_SLOT_BOOT` 与
> `DUAL_IMAGE_SUPPORT` 互斥（rt2880.h 中有 `#error` 保护，同时开启会编译报错）。

> 注意：Web 救砖上传的固件固定写入槽 A（0x50000）；救砖后如需从槽 A 启动，
> 用控制台 `setenv boot_slot A; saveenv` 或 `abslot A` 切换。

## 工程定制要点

- `configs/` 配置预设系统 + `loadcfg.sh` 一键编译（免 menuconfig、免手工配工具链路径）
- 默认配置：64MB (512Mb) DDR2、16bit、SPI Flash XIP（TEXT_BASE=0xBC000000）
- 交叉工具链 tarball 入库（`toolchains/`），克隆即用
- `.gitignore` 已整理：编译产物一律不入库，仓库仅含源码
