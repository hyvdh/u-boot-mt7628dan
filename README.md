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

## 工程定制要点

- `configs/` 配置预设系统 + `loadcfg.sh` 一键编译（免 menuconfig、免手工配工具链路径）
- 默认配置：64MB (512Mb) DDR2、16bit、SPI Flash XIP（TEXT_BASE=0xBC000000）
- 交叉工具链 tarball 入库（`toolchains/`），克隆即用
- `.gitignore` 已整理：编译产物一律不入库，仓库仅含源码
