#!/usr/bin/env bash

set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$ROOT_DIR/test"
EXPECTED_DIR="$TEST_DIR/expected"
BIN="$ROOT_DIR/main"

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

for test_file in "$TEST_DIR"/*.rdn; do
    test_name="$(basename "$test_file" .rdn)"
    expected_out="$EXPECTED_DIR/$test_name.out"
    expected_status="$EXPECTED_DIR/$test_name.status"

    if [[ ! -f "$expected_out" ]]; then
        printf "%sSKIP%s %s (missing %s)\n" "$YELLOW" "$RESET" "$test_name" "$(basename "$expected_out")"
        continue
    fi

    want_status=0
    if [[ -f "$expected_status" ]]; then
        want_status="$(tr -d '[:space:]' < "$expected_status")"
    fi

    output="$("$BIN" "$test_file" 2>&1)"
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
