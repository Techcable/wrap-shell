#!/usr/bin/env python3
import sys
import os

from pathlib import Path
from subprocess import run

SUPPORTED_FLAGS = {'--opto', '-O'}

def main(flags):
    cmd = [os.getenv("CC", default="gcc"), "-Wall", "-Wextra"]
    optimize = False
    for flag in flags:
        if flag in ("--opto", "-O"):
            optimize = True
        elif flag not in SUPPORTED_FLAGS:
            print(f"Unsupported flag: {flag!r}", file=sys.stderr)
            sys.exit(1)
    if optimize in flags:
        cmd.append("-O2")
        # TODO: -define NDEBUG?
    # NOTE: We are a single file operation, so we just
    # compile directly to a binary
    Path("build").mkdir(exist_ok=True)
    cmd.append("src/run-shell.c")
    cmd.extend(["-o", "build/run-shell"])
    print("Compiling src/run-shell.c -> run-shell (executable)")
    run(cmd, check=True)

if __name__ == "__main__":
    main(sys.argv[1:])
