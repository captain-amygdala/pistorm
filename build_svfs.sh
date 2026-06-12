#!/bin/bash
#
# Build the PiStorm CPLD bitstreams for both families. Single fixed
# variant: the RTL holds An/Dn into S0 for the 68000-style address/data
# hold after AS (see rtl/pistorm.v).

set -euo pipefail

BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC=pistorm            # source project basename (.qpf/.qsf)
REV=pistorm_build
FAMS=(EPM240 EPM570)
RESET=$'\033[0m'       # Quartus leaves the terminal recoloured; undo it

cd "$BASE_DIR/rtl"

# Temp project file = the real one with only the revision name swapped.
sed "s/^PROJECT_REVISION = .*/PROJECT_REVISION = \"$REV\"/" "$SRC.qpf" > "$REV.qpf"

for fam in "${FAMS[@]}"; do
  svf="${fam}_pistorm.svf"
  sed "s/^\(set_global_assignment -name DEVICE \).*/\1${fam}T100C5/" "$SRC.qsf" > "$REV.qsf"
  rm -rf db incremental_db output_files
  echo
  echo "${RESET}Building $svf.gz..."
  ../quartus_docker_run.sh quartus_sh --flow compile "$REV"
  ../quartus_docker_run.sh quartus_cpf -c -q 100KHz -g 3.3 -n p "output_files/$REV.pof" "$svf"
  gzip -f "$svf"
done
