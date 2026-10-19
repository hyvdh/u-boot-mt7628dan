#!/bin/bash
# 加载预设配置（configs/<名字>/ 下的 config + autoconf.h 成对拷贝到工程根目录）
# 说明: 本 SDK 的 menuconfig 会同时生成 .config(make变量) 与 autoconf.h(C宏)，
#       两者必须一致，本脚本成对加载以保证同步，加载后无需再跑 menuconfig。
#       工具链: 自动使用 toolchains/ 下已解压的工具链; 未解压时自动解压
#       toolchains/buildroot-gcc342.tar.bz2 (仅需一次); 并检测32位运行时依赖。
#
# 用法:
#   ./loadcfg.sh                        加载默认预设并编译（日常只需这一条）
#   ./loadcfg.sh <预设名>               加载指定预设并编译
#   ./loadcfg.sh <预设名> <命令...>     加载预设后执行指定命令(自动带上工具链路径)
#
# 默认预设: mt7628dan_64MB (MT7628DAN, 512Mb DDR2 16bit = 64MB, SPI flash XIP)
set -e

DEFAULT_PRESET=mt7628dan_64MB
NAME=${1:-$DEFAULT_PRESET}

[ -f "configs/$NAME/config" ]     || { echo "错误: 未找到 configs/$NAME/config"; exit 1; }
[ -f "configs/$NAME/autoconf.h" ] || { echo "错误: 未找到 configs/$NAME/autoconf.h"; exit 1; }

cp "configs/$NAME/config"     .config
cp "configs/$NAME/autoconf.h" autoconf.h

# 一致性校验: .config 中启用的 DDR 颗粒选项必须与 autoconf.h 的 #define 互相匹配
for opt in $(sed -n 's/^\(ON_BOARD_[0-9]*M_DRAM_COMPONENT\)=y$/\1/p' .config); do
    grep -q "^#define $opt 1$" autoconf.h || { echo "错误: .config 启用 $opt 但 autoconf.h 未定义，已中止"; exit 1; }
done
for opt in $(sed -n 's/^#define \(ON_BOARD_[0-9]*M_DRAM_COMPONENT\) 1$/\1/p' autoconf.h); do
    grep -q "^$opt=y$" .config || { echo "错误: autoconf.h 定义 $opt 但 .config 未启用，已中止"; exit 1; }
done

DRAM=$(grep '_DRAM_COMPONENT=y' .config | head -1)
BASE=$(grep '^TEXT_BASE=' .config)
echo "已加载: $NAME"
echo "  $DRAM / $BASE"

# 自动探测/解压工程内工具链(已解压则跳过, 未解压且有tarball则自动解压一次)
TOOLCHAIN_BIN="$PWD/toolchains/buildroot-gcc342/bin"
if [ ! -d "$TOOLCHAIN_BIN" ] && [ -f "$PWD/toolchains/buildroot-gcc342.tar.bz2" ]; then
    echo "首次使用: 正在解压工具链(约几秒)..."
    tar -xjf "$PWD/toolchains/buildroot-gcc342.tar.bz2" -C "$PWD/toolchains/" || {
        echo "错误: 工具链解压失败, 请先安装 bzip2: sudo apt install bzip2"
        exit 1
    }
fi
if [ -d "$TOOLCHAIN_BIN" ]; then
    export CONFIG_CROSS_COMPILER_PATH="$TOOLCHAIN_BIN"
    # 32位运行时检测(GCC3.4.2是32位程序, 64位系统缺libc6-i386会误报"No such file or directory")
    if ! "$TOOLCHAIN_BIN/mipsel-linux-gcc" --version >/dev/null 2>&1; then
        echo "错误: 工具链无法执行, 64位系统请先安装32位运行时: sudo apt install libc6-i386"
        exit 1
    fi
    echo "  工具链: $CONFIG_CROSS_COMPILER_PATH"
else
    echo "  提示: 未找到 $TOOLCHAIN_BIN，将使用 Makefile 默认路径(/opt/buildroot-gcc342)"
fi

# 第1参数为预设名(已消费)，剩余参数作为命令执行；无剩余参数时默认执行 make
if [ $# -gt 0 ]; then shift; fi
if [ $# -eq 0 ]; then set -- make; fi
echo "执行: $*"
if [ -n "$CONFIG_CROSS_COMPILER_PATH" ]; then
    exec env CONFIG_CROSS_COMPILER_PATH="$CONFIG_CROSS_COMPILER_PATH" "$@"
else
    exec "$@"
fi
