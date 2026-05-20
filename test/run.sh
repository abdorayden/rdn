#!/usr/bin/env bash

set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$ROOT_DIR/test"
EXPECTED_DIR="$TEST_DIR/expected"
BIN="$ROOT_DIR/main"

case "$(uname -s)" in
    Darwin)
        SHARED_EXT="dylib"
        ;;
    *)
        SHARED_EXT="so"
        ;;
esac

RED="$(printf '\033[31m')"
GREEN="$(printf '\033[32m')"
YELLOW="$(printf '\033[33m')"
BLUE="$(printf '\033[34m')"
BOLD="$(printf '\033[1m')"
RESET="$(printf '\033[0m')"

pass_count=0
fail_count=0

printf "%sBuilding test binary...%s\n" "$BLUE" "$RESET"
if ! make -C "$ROOT_DIR" >/dev/null; then
    printf "%sBuild failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding native test module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -fPIC -shared \
    "$TEST_DIR/native/native_test_module.c" \
    -I"$ROOT_DIR" \
    -o "$TEST_DIR/native/native_test_module.$SHARED_EXT"; then
    printf "%sNative module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding file native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/files.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/files.$SHARED_EXT"; then
    printf "%sFile native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding math native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/math.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/math.$SHARED_EXT" \
    -lm; then
    printf "%sMath native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding unix native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/unix.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/unix.$SHARED_EXT"; then
    printf "%sUnix native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding syscall native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/syscall.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/syscall.$SHARED_EXT"; then
    printf "%sSyscall native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding path native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/path.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/path.$SHARED_EXT"; then
    printf "%sPath native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding process native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/process.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/process.$SHARED_EXT"; then
    printf "%sProcess native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding net native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/net.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/net.$SHARED_EXT"; then
    printf "%sNet native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding json native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/json.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/json.$SHARED_EXT"; then
    printf "%sJson native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

printf "%sBuilding io native module...%s\n" "$BLUE" "$RESET"
if ! "${CC:-gcc}" -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared \
    "$ROOT_DIR/nativelibs/io.c" \
    -I"$ROOT_DIR" \
    -o "$ROOT_DIR/nativelibs/io.$SHARED_EXT"; then
    printf "%sIo native module build failed%s\n" "$RED" "$RESET"
    exit 1
fi

for test_file in "$TEST_DIR"/*.rdn; do
    test_name="$(basename "$test_file" .rdn)"
    expected_out="$EXPECTED_DIR/$test_name.out"
    expected_status="$EXPECTED_DIR/$test_name.status"
    stdin_file="$TEST_DIR/$test_name.stdin"

    if [[ ! -f "$expected_out" ]]; then
        printf "%sSKIP%s %s (missing %s)\n" "$YELLOW" "$RESET" "$test_name" "$(basename "$expected_out")"
        continue
    fi

    want_status=0
    if [[ -f "$expected_status" ]]; then
        want_status="$(tr -d '[:space:]' < "$expected_status")"
    fi

    if [[ -f "$stdin_file" ]]; then
        output="$("$BIN" "$test_file" < "$stdin_file" 2>&1)"
    else
        output="$("$BIN" "$test_file" 2>&1)"
    fi
    got_status=$?
    expected_output="$(cat "$expected_out")"

    if [[ "$output" == "$expected_output" && "$got_status" == "$want_status" ]]; then
        printf "%sPASS%s %s\n" "$GREEN" "$RESET" "$test_name"
        pass_count=$((pass_count + 1))
        continue
    fi

    printf "%sFAIL%s %s\n" "$RED" "$RESET" "$test_name"
    printf "  expected status: %s%s%s\n" "$BOLD" "$want_status" "$RESET"
    printf "  actual status:   %s%s%s\n" "$BOLD" "$got_status" "$RESET"
    printf "  expected output:\n"
    if [[ -n "$expected_output" ]]; then
        printf "%s\n" "$expected_output" | sed 's/^/    /'
    else
        printf "    <empty>\n"
    fi
    printf "  actual output:\n"
    if [[ -n "$output" ]]; then
        printf "%s\n" "$output" | sed 's/^/    /'
    else
        printf "    <empty>\n"
    fi
    fail_count=$((fail_count + 1))
done

printf "\n%sSummary%s %s%d passed%s, %s%d failed%s\n" \
    "$BOLD" "$RESET" "$GREEN" "$pass_count" "$RESET" "$RED" "$fail_count" "$RESET"

if [[ "$fail_count" -ne 0 ]]; then
    exit 1
fi
