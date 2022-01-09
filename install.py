#!/usr/bin/env python3
import sys
import shutil

from pathlib import Path

SUPPORTED_FLAGS = {"--target",}

def main(flags):
    binary = Path("build/run-shell")
    if not binary.exists():
        print("ERROR: Binary does not exist", file=sys.stderr)
        sys.exit(1)
    idx = 0
    target_path = "/opt/techcable"
    while idx < len(flags):
        flag = flags[idx]
        if flag == "--target":
            idx += 1
            try:
                target_file = flag[idx]
            except IndexError:
                print("Must specify target", file=sys.stderr)
                sys.exit(1)
        else:
            raise AssertionError(f"Unimplemetned flag: {flag!r}")
    assert binary.exists()
    print(f"Installing {binary} to {target_path}/bin/{binary.name}")
    bin_path = Path(target_path, "bin")
    bin_path.mkdir(exist_ok=True, parents=True)
    shutil.copy2(binary, Path(bin_path, "run-shell"))

if __name__ == "__main__":
    main(sys.argv[1:])
