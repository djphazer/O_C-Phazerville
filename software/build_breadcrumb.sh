#!/usr/bin/env bash
set -euo pipefail

BUILD_REQUEST='oc-build/1 target=T32_VOR apps=+MIDI,+AUTOMATONNETZ,+NOENVSEQ,+NOPIQUET,+NOFROGSEQ'
EXTRA_BUILD_FLAGS='-DNO_DISPLAY_DMA -DPHZ_BOOT_BREADCRUMBS -DPHZ_APP_ISR_DIVIDER=8'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PIO="${REPO_ROOT}/.venv/bin/pio"

if [[ ! -x "${PIO}" ]]; then
  if command -v pio >/dev/null 2>&1; then
    PIO="$(command -v pio)"
  else
    echo "error: pio not found. Activate your venv or install PlatformIO first." >&2
    exit 1
  fi
fi

CUSTOM_BUILD_FLAGS="$(GH_COMMENT="${BUILD_REQUEST}" python3 "${SCRIPT_DIR}/res/parse_build_request.py")"
OC_VERSION="$(tail -n 1 "${SCRIPT_DIR}/src/OC_version.h" | tr -d '"')"
SOURCE_SHORT_SHA="$(git -C "${REPO_ROOT}" rev-parse --short HEAD)"
ARTIFACT_DIR="${REPO_ROOT}/artifacts"

mkdir -p "${ARTIFACT_DIR}"

cd "${SCRIPT_DIR}"

echo "Building breadcrumb lean Hemisphere T32 custom firmware"
export CUSTOM_BUILD_FLAGS="${CUSTOM_BUILD_FLAGS} ${EXTRA_BUILD_FLAGS} -DOC_VERSION_SUFFIX=T32_breadcrumb_lean"
"${PIO}" run -t clean -e custom
"${PIO}" run -e custom
cp .pio/build/custom/*.hex \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_breadcrumb_lean-${SOURCE_SHORT_SHA}.hex"

echo "Built artifact:"
ls -lh "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_breadcrumb_lean-${SOURCE_SHORT_SHA}.hex"
