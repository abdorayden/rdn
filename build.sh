#!/usr/bin/env bash


# Copyright (c) 2023-2026 Ray Den
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# NOTE:
# congrats u did the first step, u have to check the script if it safe (u don't have to trust any script u executed from internet)
# the second step enjoy using this small language

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -Wextra -Werror -ggdb -std=c11}
LDFLAGS=${LDFLAGS:--ldl -rdynamic}
PREFIX=${PREFIX:-/usr/local/share/rdn}
BINDIR=${BINDIR:-/usr/local/bin}
APP_NAME=${APP_NAME:-rdn}
VERBOSE=${VERBOSE:-1}

LIBDIR=${LIBDIR:-/usr/lib}
INCLUDEDIR=${INCLUDEDIR:-/usr/include}

main_binary="$ROOT_DIR/main"
shared_ext="so"

rdn_lib_object="$ROOT_DIR/rdn.o"
static_lib="$ROOT_DIR/librdn.a"
shared_lib="$ROOT_DIR/librdn.so"

usage() {
    cat <<EOF
usage: $(basename "$0") [all|build|install|clean|help]

targets:
  all       build everything and install into \$PREFIX
  build     build the interpreter, native modules and libs
  libs      build the interpreter libraries (librdn.a / librdn.so)
  install   build and install into \$PREFIX and \$BINDIR
  clean     remove local build outputs
  help      show this help text

environment:
  PREFIX      install prefix for libs and nativelibs
  BINDIR      install directory for the rdn binary
  CC          C compiler to use
  CFLAGS      extra compiler flags
  LDFLAGS     extra linker flags
  APP_NAME    installed binary name
  VERBOSE     set to 0 to silence command logging
  LIBDIR      folder where script install .a/.so libs
  INCLUDEDIR  folder where script install API headers (rdn.h/...)
EOF
}

log() {
    printf '%s\n' "$*"
}

section() {
    log
    log "==> $*"
}

run() {
    if [ "$VERBOSE" != "0" ]; then
        printf '+ '
        printf '%q ' "$@"
        printf '\n'
    fi
    "$@"
}

case "$(uname -s)" in
    Darwin)
        shared_ext="dylib"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        shared_ext="dll"
        ;;
esac

main_cflags="$CFLAGS -O3 -DRDN_INSTALL_PREFIX=\"${PREFIX}\""
native_cflags="$CFLAGS -O3 -fPIC -shared"

build_main() {
    section "Building interpreter"
    run "$CC" $main_cflags -c "$ROOT_DIR/main.c" -o "$ROOT_DIR/main.o"
    run "$CC" "$ROOT_DIR/main.o" -O3 -o "$main_binary" $LDFLAGS
}

build_libs() {
    section "Building interpreter libraries (librdn.a / librdn.so)"
    run "$CC" $CFLAGS -O3 -fPIC -DRDN_INSTALL_PREFIX=\"${PREFIX}\" \
        -c "$ROOT_DIR/src/rdn.c" -o "$rdn_lib_object"
    run ar rcs "$static_lib" "$rdn_lib_object"
    run "$CC" -shared -rdynamic -o "$shared_lib" "$rdn_lib_object" -ldl
}

build_native() {
    local source_file="$1"
    local output_file="$2"
    local extra_flags="${3:-}"

    log "  native: $(basename "$source_file") -> $(basename "$output_file")"
    run "$CC" $native_cflags $extra_flags "$source_file" -I"$ROOT_DIR" -o "$output_file"
}

install_runtime_tree() {
    section "Installing runtime tree"
    run mkdir -p "$PREFIX/libs" "$PREFIX/nativelibs" "$BINDIR"
    run cp -R "$ROOT_DIR/libs/." "$PREFIX/libs/"
    run cp -R "$ROOT_DIR/nativelibs/." "$PREFIX/nativelibs/"
    run mkdir -p "$LIBDIR" "$INCLUDEDIR"
    run cp "$static_lib" "$LIBDIR/"
    run cp "$shared_lib" "$LIBDIR/"
    run cp "$ROOT_DIR/include/rdn.h" "$INCLUDEDIR/rdn.h"
    run cp "$ROOT_DIR/include/rdn_native.h" "$INCLUDEDIR/rdn_native.h"


    for source_file in "$PREFIX/libs"/*.rdn; do
        [ -e "$source_file" ] || continue
        alias_name="$(basename "$source_file")"
        alias_name="${alias_name^}"
        [ "$alias_name" = "$(basename "$source_file")" ] && continue
        run cp "$source_file" "$PREFIX/libs/$alias_name"
    done
    for source_file in "$PREFIX/nativelibs"/*."$shared_ext"; do
        [ -e "$source_file" ] || continue
        alias_name="$(basename "$source_file")"
        alias_name="${alias_name^}"
        [ "$alias_name" = "$(basename "$source_file")" ] && continue
        run cp "$source_file" "$PREFIX/nativelibs/$alias_name"
    done
    run cp "$main_binary" "$BINDIR/$APP_NAME"
    run chmod 755 "$BINDIR/$APP_NAME"
}

clean() {
    section "Cleaning build outputs"
    run rm -f "$ROOT_DIR/main" "$ROOT_DIR/main.o" "$rdn_lib_object" "$static_lib" "$shared_lib"
    run rm -f "$ROOT_DIR"/nativelibs/*.so "$ROOT_DIR"/nativelibs/*.dylib "$ROOT_DIR"/nativelibs/*.dll
}

build_all() {
    section "Preparing build"
    log "prefix: $PREFIX"
    log "bindir: $BINDIR"
    log "compiler: $CC"
    log "shared extension: $shared_ext"
    build_main
    build_libs
    build_native "$ROOT_DIR/nativelibs/math.c" "$ROOT_DIR/nativelibs/math.$shared_ext" "-lm"
    build_native "$ROOT_DIR/nativelibs/files.c" "$ROOT_DIR/nativelibs/files.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/unix.c" "$ROOT_DIR/nativelibs/unix.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/syscall.c" "$ROOT_DIR/nativelibs/syscall.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/path.c" "$ROOT_DIR/nativelibs/path.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/process.c" "$ROOT_DIR/nativelibs/process.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/net.c" "$ROOT_DIR/nativelibs/net.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/strings.c" "$ROOT_DIR/nativelibs/strings.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/strconv.c" "$ROOT_DIR/nativelibs/strconv.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/json.c" "$ROOT_DIR/nativelibs/json.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/io.c" "$ROOT_DIR/nativelibs/io.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/bint.c" "$ROOT_DIR/nativelibs/bint.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/coroutines.c" "$ROOT_DIR/nativelibs/coroutines.$shared_ext"
    build_native "$ROOT_DIR/nativelibs/map.c" "$ROOT_DIR/nativelibs/map.$shared_ext"
}

case "${1:-all}" in
    all)
        build_all
        install_runtime_tree
        ;;
    build)
        build_all
        ;;
    libs)
        build_libs
        ;;
    install)
        build_all
        install_runtime_tree
        ;;
    clean)
        clean
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
