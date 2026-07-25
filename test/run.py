#!/usr/bin/env python3

"""
Copyright (c) 2023-2026 Ray Den

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

python test script
run: ./test/run.py

"""

import os
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
TEST_DIR = ROOT_DIR / "test"
EXPECTED_DIR = TEST_DIR / "expected"
BIN = ROOT_DIR / "main"
SHARED_EXT = "dylib" if sys.platform == "darwin" else "so"
CC = os.environ.get("CC", "gcc")

RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
BOLD = "\033[1m"
RESET = "\033[0m"

NATIVE_MODULES = [
    ("native test module", [CC, "-Wall", "-Wextra", "-Werror", "-fPIC", "-shared",
        str(TEST_DIR / "native" / "native_test_module.c"),
        f"-I{ROOT_DIR}", "-o", str(TEST_DIR / "native" / f"native_test_module.{SHARED_EXT}")]),
    ("file native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "files.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"files.{SHARED_EXT}")]),
    ("math native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "math.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"math.{SHARED_EXT}"), "-lm"]),
    ("unix native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "unix.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"unix.{SHARED_EXT}")]),
    ("syscall native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "syscall.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"syscall.{SHARED_EXT}")]),
    ("path native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "path.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"path.{SHARED_EXT}")]),
    ("process native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "process.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"process.{SHARED_EXT}")]),
    ("net native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "net.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"net.{SHARED_EXT}")]),
    ("strings native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "strings.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"strings.{SHARED_EXT}")]),
    ("strconv native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "strconv.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"strconv.{SHARED_EXT}")]),
    ("json native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "json.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"json.{SHARED_EXT}")]),
    ("io native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "io.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"io.{SHARED_EXT}")]),
    ("bint native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "bint.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"bint.{SHARED_EXT}")]),
    ("coroutines native module", [CC, "-Wall", "-Wextra", "-Werror", "-ggdb", "-std=c11", "-fPIC", "-shared",
        str(ROOT_DIR / "nativelibs" / "coroutines.c"),
        f"-I{ROOT_DIR}", "-o", str(ROOT_DIR / "nativelibs" / f"coroutines.{SHARED_EXT}")]),
]


def note(s):
    print(f"{BLUE}{s}{RESET}")


def fail(s):
    print(f"{RED}{s}{RESET}")
    sys.exit(1)


def build_native(name, args):
    note(f"Building {name}...")
    if subprocess.run(args).returncode != 0:
        fail(f"{name.title()} build failed")


def run_test(test_file):
    name = test_file.stem
    expected_out = EXPECTED_DIR / f"{name}.out"
    expected_status_file = EXPECTED_DIR / f"{name}.status"
    stdin_file = TEST_DIR / f"{name}.stdin"

    if not expected_out.is_file():
        print(f"{YELLOW}SKIP{RESET} {name} (missing {expected_out.name})")
        return None

    want_status = int(expected_status_file.read_text().strip()) if expected_status_file.is_file() else 0

    stdin_data = stdin_file.read_bytes() if stdin_file.is_file() else None
    result = subprocess.run(
        [str(BIN), str(test_file)],
        input=stdin_data,
        capture_output=True,
    )
    got_status = result.returncode
    output = (result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace")).rstrip("\n")

    expected_output = expected_out.read_text().rstrip("\n")

    if output == expected_output and got_status == want_status:
        print(f"{GREEN}PASS{RESET} {name}")
        return True

    print(f"{RED}FAIL{RESET} {name}")
    print(f"  expected status: {BOLD}{want_status}{RESET}")
    print(f"  actual status:   {BOLD}{got_status}{RESET}")
    print("  expected output:")
    for line in (expected_output.splitlines() or ["    <empty>"]):
        print(f"    {line}" if line else "    <empty>")
    print("  actual output:")
    for line in (output.splitlines() or ["    <empty>"]):
        print(f"    {line}" if line else "    <empty>")
    return False


def main():
    note("Building test binary...")
    if subprocess.run(["make", "-C", str(ROOT_DIR)], stdout=subprocess.DEVNULL).returncode != 0:
        fail("Build failed")

    for name, args in NATIVE_MODULES:
        build_native(name, args)

    pass_count = 0
    fail_count = 0
    skip_count = 0
    for test_file in sorted(TEST_DIR.glob("*.rdn")):
        result = run_test(test_file)
        if result is True:
            pass_count += 1
        elif result is False:
            fail_count += 1
        else:
            skip_count += 1

    print(f"\n{BOLD}Summary{RESET} {GREEN}{pass_count} passed{RESET}, {RED}{fail_count} failed{RESET}, {YELLOW}{skip_count} skipped{RESET}")
    if fail_count:
        sys.exit(1)


if __name__ == "__main__":
    main()
