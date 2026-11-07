#!/usr/bin/env bash
# Rebuild StikJIT 1.9.0 with full-width debugserver addresses. Its shipped
# nine-digit packet format truncates JIT mappings above 0xfffffffff on iPads.
set -euo pipefail

HENGE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STIKJIT_ROOT="${1:?StikJIT checkout path required}"
STIKJIT_REV=32287268fa5824f9edce4cb359f5833ce0cf7b00

git clone --depth 1 --branch 1.9.0 https://github.com/StikDebug/StikJIT.git "$STIKJIT_ROOT"
test "$(git -C "$STIKJIT_ROOT" rev-parse HEAD)" = "$STIKJIT_REV"
git -C "$STIKJIT_ROOT" apply "$HENGE_ROOT/ci/stikjit-high-address.patch"

cd "$STIKJIT_ROOT"
xcodegen generate
# The module and its public enum are both named StikJIT. Distribution-mode
# textual interfaces qualify types as StikJIT.DDIPaths etc., which Swift can
# resolve against the enum instead of the module (swiftlang/swift#56573).
# Henge and this framework use the same Xcode, so keep binary Swift modules.
xcodebuild archive -project StikJIT.xcodeproj -scheme StikJIT -configuration Release \
    -sdk iphoneos -destination 'generic/platform=iOS' -archivePath build/StikJIT \
    BUILD_LIBRARY_FOR_DISTRIBUTION=NO CODE_SIGNING_ALLOWED=NO -quiet
xcodebuild -create-xcframework -allow-internal-distribution \
    -framework build/StikJIT.xcarchive/Products/Library/Frameworks/StikJIT.framework \
    -output StikJIT.xcframework

# Check the packaged module before copying it into Henge's dependencies.
MODULES=StikJIT.xcframework/ios-arm64/StikJIT.framework/Modules/StikJIT.swiftmodule
BINARY_MODULE=$(find "$MODULES" -type f -name '*.swiftmodule' -print -quit)
if [ -z "$BINARY_MODULE" ] || [ ! -s "$BINARY_MODULE" ]; then
    echo 'StikJIT XCFramework is missing its binary Swift module' >&2
    exit 1
fi
printf 'import StikJIT\n' > build/check-stikjit-import.swift
xcrun swiftc -typecheck -target arm64-apple-ios26.0 \
    -sdk "$(xcrun --sdk iphoneos --show-sdk-path)" \
    -F "$PWD/StikJIT.xcframework/ios-arm64" \
    -module-cache-path "$PWD/build/import-module-cache" \
    build/check-stikjit-import.swift
