wrap-shell
==========
A lightweight utility to run your prefered shell,
with a fallback to a secondary shell on failure.

In my case, the preferred shell is [xonsh](https://xon.sh/) or [fish](https://fishshell.com)
and my fallback shell is `zsh` (or /bin/sh if that's not found).

This allows you to do ctrl-d at your primary shell.
This will immediately fallback to a traditional POSIX shell.

The fallback shell is done in a subprocess (with fork), so the environment is clean
and has not been modified by the original shell.

This wrapper is usefull if your primary shell crashes.
It is also useful if you want to quickly switch to a POSIX shell (just with Ctrl-d).

Unfortunately, available shells are currently hardcoded into the app :(

PRs are welcome :D

## Build & Install
Uses meson for build.

Run `install.py` to install to (default `/opt/techcable`)
