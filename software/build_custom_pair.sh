#!/usr/bin/env bash
set -euo pipefail

FULL_BUILD_REQUEST='oc-build/1 target=T32_VOR apps=+PEWPEWPEW,+NOWAVE,+NOPONGLET,+NOTUNER,+NOENVSEQ,+NOFROGSEQ'
T32_EXTRA_BUILD_FLAGS='-DNO_DISPLAY_DMA'
FULL_VOR_EXTRA_BUILD_FLAGS='-DNO_APPLET_SCOPE'
LEAN_BUILD_REQUEST='oc-build/1 target=T32_VOR apps=+MIDI,+AUTOMATONNETZ,+NOENVSEQ,+NOPIQUET,+NOFROGSEQ'

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

FULL_CUSTOM_BUILD_FLAGS="$(GH_COMMENT="${FULL_BUILD_REQUEST}" python3 "${SCRIPT_DIR}/res/parse_build_request.py")"
LEAN_CUSTOM_BUILD_FLAGS="$(GH_COMMENT="${LEAN_BUILD_REQUEST}" python3 "${SCRIPT_DIR}/res/parse_build_request.py")"
OC_VERSION="$(tail -n 1 "${SCRIPT_DIR}/src/OC_version.h" | tr -d '"')"
SOURCE_SHORT_SHA="$(git -C "${REPO_ROOT}" rev-parse --short HEAD)"
ARTIFACT_DIR="${REPO_ROOT}/artifacts"

mkdir -p "${ARTIFACT_DIR}"

cd "${SCRIPT_DIR}"

build_firmware() {
  local label="$1"
  local flags="$2"
  local version_suffix="$3"
  local output="$4"

  echo "Building ${label}"
  export CUSTOM_BUILD_FLAGS="${flags} -DOC_VERSION_SUFFIX=${version_suffix}"
  "${PIO}" run -t clean -e custom
  "${PIO}" run -e custom
  cp .pio/build/custom/*.hex "${output}"
}

build_firmware \
  "full Hemisphere T32 custom firmware" \
  "${FULL_CUSTOM_BUILD_FLAGS} ${T32_EXTRA_BUILD_FLAGS}" \
  "T32_pewpewpew" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_pewpewpew-${SOURCE_SHORT_SHA}.hex"

build_firmware \
  "full Hemisphere VOR T32 custom firmware" \
  "${FULL_CUSTOM_BUILD_FLAGS} ${T32_EXTRA_BUILD_FLAGS} ${FULL_VOR_EXTRA_BUILD_FLAGS} -DVOR" \
  "T32_VOR_pewpewpew" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_VOR_pewpewpew-${SOURCE_SHORT_SHA}.hex"

build_firmware \
  "lean Hemisphere T32 custom firmware" \
  "${LEAN_CUSTOM_BUILD_FLAGS} ${T32_EXTRA_BUILD_FLAGS}" \
  "T32_lean" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_lean-${SOURCE_SHORT_SHA}.hex"

build_firmware \
  "lean Hemisphere VOR T32 custom firmware" \
  "${LEAN_CUSTOM_BUILD_FLAGS} ${T32_EXTRA_BUILD_FLAGS} -DVOR" \
  "T32_VOR_lean" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_VOR_lean-${SOURCE_SHORT_SHA}.hex"

echo "Built artifacts:"
ls -lh \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_pewpewpew-${SOURCE_SHORT_SHA}.hex" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_VOR_pewpewpew-${SOURCE_SHORT_SHA}.hex" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_lean-${SOURCE_SHORT_SHA}.hex" \
  "${ARTIFACT_DIR}/o_C-phazerville-${OC_VERSION}_T32_VOR_lean-${SOURCE_SHORT_SHA}.hex"
