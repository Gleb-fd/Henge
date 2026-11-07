#!/usr/bin/env bash
# Bitrise uploads the ad-hoc-signed IPA as a generic zip, not an installable IPA.
set -euo pipefail
HENGE_ROOT="${BITRISE_SOURCE_DIR:?Bitrise must set BITRISE_SOURCE_DIR}"
bash "$HENGE_ROOT/ci/build-ios.sh"
mkdir -p "${BITRISE_DEPLOY_DIR:?Bitrise must set BITRISE_DEPLOY_DIR}"
( cd "$HOME/Desktop" && zip -0 -q "$BITRISE_DEPLOY_DIR/Henge-color-fix-unsigned.zip" Henge.ipa )
