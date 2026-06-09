# Shared LeapOS / RTEMS build environment (source from other scripts).
# Usage: source "$(dirname "$0")/env.sh"

LEAPOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$LEAPOS_ROOT/../../../.." && pwd)"

export RTEMS_ROOT="${RTEMS_ROOT:-$HOME/rtems}"
export RTEMS_PREFIX="${RTEMS_PREFIX:-$RTEMS_ROOT/6}"
export RTEMS_TOOLS="${RTEMS_TOOLS:-$RTEMS_PREFIX/bin}"
export RTEMS_BSPS="${RTEMS_BSPS:-$RTEMS_PREFIX/i386-rtems6}"
export RTEMS_BSP="${RTEMS_BSP:-i386/pc386}"
export RTEMS_VERSION="${RTEMS_VERSION:-6}"

export RTEMS_SRC="${RTEMS_SRC:-$RTEMS_ROOT/src}"
export RTEMS_RSB="${RTEMS_RSB:-$RTEMS_SRC/rsb/rtems}"
export RTEMS_EXAMPLES="${RTEMS_EXAMPLES:-$RTEMS_SRC/rtems-examples}"

export LEAPOS_IMAGE_DIR="${LEAPOS_IMAGE_DIR:-$LEAPOS_ROOT/rtems-image}"
export LEAPOS_STAGING="${LEAPOS_STAGING:-$LEAPOS_IMAGE_DIR/staging}"

export NET_PROBE_DIR="${NET_PROBE_DIR:-$LEAPOS_ROOT/net-probe}"
export NET_PROBE_EXE="${NET_PROBE_EXE:-$NET_PROBE_DIR/build/i386-rtems6-pc386/net-probe.exe}"

export LEAP_PORT_DIR="${LEAP_PORT_DIR:-$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapPort}"
export LEAP_PORT_EXE="${LEAP_PORT_EXE:-$LEAPOS_ROOT/rtems-image/leap-port.exe}"
export LIBBSD_A="${LIBBSD_A:-$RTEMS_BSPS/pc386/lib/libbsd.a}"

export PATH="$RTEMS_TOOLS:$PATH"
