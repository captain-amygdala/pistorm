#!/bin/bash
# Run a command in the Quartus container: stock raetro/quartus:base with the
# host's Quartus bind-mounted in. The current directory is mounted as /work
# and is the cwd, so cd to your project dir first. Usage:
#   ./run.sh                  # interactive shell
#   ./run.sh <command...>     # one-shot command
# Env: INSTALL_DIR (Quartus install, default ~/intelFPGA_lite).
set -e

INSTALL_DIR="${INSTALL_DIR:-$HOME/intelFPGA_lite}"

TTY_FLAG=""
if [ -t 0 ]; then TTY_FLAG="-it"; fi

# --user makes bind-mount writes host-owned; HOME=/tmp is writable for any
# uid (Quartus drops license/config there); LD_PRELOAD is the tcmalloc fix
# for Quartus 20.1's libsys_cpt udev double-free.
exec docker run --rm $TTY_FLAG \
    --user "$(id -u):$(id -g)" \
    -v "$INSTALL_DIR":/opt/intelFPGA_lite \
    -v "$PWD":/work \
    -w /work \
    -e HOME=/tmp \
    -e PATH=/opt/intelFPGA_lite/20.1/quartus/bin:/usr/local/bin:/usr/bin:/bin \
    -e LD_PRELOAD=/usr/lib/libtcmalloc_minimal.so.4:/usr/lib/x86_64-linux-gnu/libstdc++.so.6 \
    raetro/quartus:base "$@"
