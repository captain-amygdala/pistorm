#!/bin/bash
#
# One-time: install Quartus 20.1.1 Lite into ~/intelFPGA_lite, MAX II/V
# device family only (PiStorm CPLDs are EPM240/EPM570 = MAX II), using the
# stock raetro/quartus:base image (auto-pulled; no custom image needed).
# Re-runnable. After this, run.sh / build_svfs.sh just consume the install.
#
# Env overrides:
#   INSTALLER_DIR  unpacked Quartus tarball (default ~/Quartus-lite-20.1.1.720-linux)
#   INSTALL_DIR    install target           (default ~/intelFPGA_lite)
set -euo pipefail

INSTALLER_DIR="${INSTALLER_DIR:-$HOME/Quartus-lite-20.1.1.720-linux}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/intelFPGA_lite}"

mkdir -p "$INSTALL_DIR"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$INSTALLER_DIR":/installer:ro \
  -v "$INSTALL_DIR":/opt/intelFPGA_lite \
  -e HOME=/tmp \
  -e LD_PRELOAD=/usr/lib/libtcmalloc_minimal.so.4:/usr/lib/x86_64-linux-gnu/libstdc++.so.6 \
  raetro/quartus:base \
  /installer/setup.sh --mode unattended --unattendedmodeui none \
    --installdir /opt/intelFPGA_lite/20.1 --accept_eula 1 \
    --disable-components quartus_help,arria_lite,cyclone,cyclone10lp,cyclonev,max10,quartus_update,modelsim_ase,modelsim_ae

echo "Quartus installed in $INSTALL_DIR - you can now run ./build_svfs.sh"
