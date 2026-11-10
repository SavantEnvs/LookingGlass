#!/usr/bin/env bash
#
# mayhem/build.sh: builds the fuzz targets (+ standalone reproducers) and the oracle (selftest).
# Runs in the commit image as `mayhem` in /mayhem; idempotent and air-gapped (deps are baked into
# the image under /opt/toolchains/deps by the Dockerfile).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# Pinned dependency commits: keep in sync with the Dockerfile ARGs.
DEPS=/opt/toolchains/deps
LGPROTOCOL=$DEPS/LGProtocol-62b52724b63b6e179b8fb7e0c7c03e9a8b4e1636/src
CIMGUI=$DEPS/cimgui-4e8fcc13674481a8c4c04abbfea90915e3896fe7/src
[ -f "$CIMGUI/cimgui.h" ] || { echo "missing $CIMGUI" >&2; exit 1; }
[ -d "$LGPROTOCOL/include" ] || { echo "missing $LGPROTOCOL" >&2; exit 1; }

B="$SRC/mayhem/build"
OUT="${MAYHEM_OUT:-/mayhem}"
rm -rf "$B"
mkdir -p "$B/san" "$B/norm" "$OUT"

# x86-64-v2 is upstream's own CMake default (framebuffer.c needs SSE4.1).
COMMON="-march=x86-64-v2 -D_GNU_SOURCE -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS=1 \
  -I$SRC/common/include -I$LGPROTOCOL/include -I$SRC/mayhem/gen -I$SRC/mayhem/harnesses"
CLIENT="-I$SRC/client/include -I$SRC/client/src -I$CIMGUI"
SAN_CFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 $COMMON"
NORM_CFLAGS="-O2 $COVERAGE_FLAGS $COMMON"

# The platform-independent common library plus the Linux platform files the targets need
# (ivshmem.c needs kernel module headers and is not fuzzed).
LIB_SRCS=$(ls common/src/*.c common/src/platform/linux/{cpuinfo,debug,time,paths,thread,event,open,sysinfo,proctitle}.c)

compile_one() { # <outdir> <cflags> <src>
  local o="$1/$(echo "$3" | tr / _).o"
  $CC $2 -c "$3" -o "$o"
}
export -f compile_one

build_lib() { # <outdir> <cflags>
  printf '%s\n' $LIB_SRCS | xargs -P "$MAYHEM_JOBS" -I{} bash -c 'compile_one "$@"' _ "$1" "$2" {}
  ar rcs "$1/liblgcommon.a" "$1"/*.o
}
build_lib "$B/san"  "$SAN_CFLAGS"
build_lib "$B/norm" "$NORM_CFLAGS"

# The client's config.c (the real option table and parsers), kb.c (scancode names) and transport.c
# (transport name lookup) are compiled as one extra library next to common.
CLIENT_SRCS="client/src/config.c client/src/kb.c client/src/transport.c"
for s in $CLIENT_SRCS; do
  $CC $SAN_CFLAGS $CLIENT -c "$s" -o "$B/san/client_$(basename "$s").o"
done
ar rcs "$B/san/libclientcfg.a" "$B"/san/client_*.o

# lsan_off.cc is C++ and linked into every fuzz and standalone binary.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/san/lsan_off.o"
# The standalone driver is C; compile it as C so LLVMFuzzerTestOneInput keeps C linkage.
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/san/standalone_main.o"

LIBS="-lpthread -lrt -lm"
build_target() { # <name> [extra sources...]
  local t="$1"; shift
  $CC $SAN_CFLAGS $CLIENT $LIB_FUZZING_ENGINE "$SRC/mayhem/harnesses/$t.c" "$@" "$B/san/liblgcommon.a" "$B/san/lsan_off.o" $LIBS -o "$OUT/$t"
  $CC $SAN_CFLAGS $CLIENT "$SRC/mayhem/harnesses/$t.c" "$@" "$B/san/standalone_main.o" "$B/san/liblgcommon.a" "$B/san/lsan_off.o" $LIBS -o "$OUT/$t-standalone"
}

build_target fuzz_client_config "$SRC/mayhem/harnesses/config_env.c" "$B/san/libclientcfg.a"
build_target fuzz_option_ini
build_target fuzz_option_args
build_target fuzz_rects
build_target fuzz_framebuffer

# Oracle: the same library with normal flags (no sanitizers, no fuzzer instrumentation).
$CC $NORM_CFLAGS "$SRC/mayhem/selftest.c" "$B/norm/liblgcommon.a" $LIBS -o "$B/selftest"
