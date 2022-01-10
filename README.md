run-shell
==========
A lightweight utility to run your prefered shell,
with a fallback to a secondary shell on failure.

In my case, the preferred shell is [xonsh](https://xon.sh/)
and my fallback shell is `zsh` (or /bin/sh if that's not found).

Unfortunately, these preferences are currently hardcoded into the app :(

PRs are welcome :D

## Build & Install
```shell
python3 build.py
# Installs to /opt/techcable by default (so it requires sudo)
#
# This can be overriden with `--target dir` flag
sudo python3 install.py
```
