#!/usr/bin/env python3
import sys
import shutil

from pathlib import Path

SUPPORTED_FLAGS = {"--target",}

def main(flags):
    binary = Path("build/wrap-shell")
    if not binary.exists():
        print("ERROR: Binary does not exist", file=sys.stderr)
        sys.exit(1)
    idx = 0
    target_path = "/opt/techcable"
    while idx < len(flags):
        flag_name = flags[idx]

        if flag_name not in SUPPORTED_FLAGS:
            print(f"Unknown flag: {flag_name}", file=sys.stderr)
            sys.exit(1)
        if flag_name == "--target":
            idx += 1
            try:
                target_path = flags[idx]
            except IndexError:
                print("Must specify target", file=sys.stderr)
                sys.exit(1)
            else:
                idx += 1
        else:
            raise AssertionError
    assert binary.exists()
    print(f"Installing {binary} to {target_path}/bin/{binary.name}")
    bin_path = Path(target_path, "bin")
    bin_path.mkdir(exist_ok=True, parents=True)
    shutil.copy2(binary, Path(bin_path, "wrap-shell"))

if __name__ == "__main__":
    main(sys.argv[1:])
