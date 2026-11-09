# iOS CI build

## Codemagic

1. Open [Applications](https://codemagic.io/apps), add or select
   `Gleb-fd/Henge`, and use the YAML configuration from the repository.
2. Select branch `experimental/a12z` and check for the configuration file.
3. Start workflow `a12z-unsigned-ipa` (**Henge A12Z experimental IPA**).
4. The workflow requests an Apple Silicon Mac mini M2 with the latest Xcode.
   Its preflight accepts Xcode 26 or later, including Codemagic's Xcode 26.6.
   SDK 26 builds omit the SDK 27-only SwiftUI external-display accessory; the
   iOS 17–26 app-delegate display path is unchanged. SDK 27 builds retain the
   accessory. Check your plan's included minutes and machine availability;
   this configuration does not buy credits or enable paid billing.
5. Download `Henge-A12Z-experimental.ipa` from the build artifacts. Re-sign it
   with your sideloader before installing. No Apple signing secrets are needed
   for this build.

CPU tests, runtime-guard tests and a source-only Swift/renderer/JIT-guard
compile run before the dependency build. The runtime-guard target is
`make test-runtime-guards`: wire-format bounds and GPU-address-history tests
run on any host, while execution of the three BRK request routines is tested
only on Darwin arm64. These portable tests do not validate Metal execution.
`ci-logs/a12-tests.log`, `ci-logs/ios-sources.log`, `ci-logs/build.log`
and Xcode's `ios/build/*.buildlog` are collected as artifacts
when available. There are no push/webhook triggers in `codemagic.yaml`, so
builds start manually. Do not put Apple credentials or device pairing files in
this public repository.

## GitHub Actions

The `Build iOS IPA` workflow is manual (`workflow_dispatch`) and uses the
standard arm64 `xcode-27` GitHub-hosted runner. After the branch containing the
workflow is available in the authorized fork, enable Actions if necessary and
start it from the Actions tab, selecting the branch to build. No Apple account,
certificate, or signing secrets are used. The workflow must first be present on
the repository's default branch for GitHub to offer manual dispatch.

Download the `Henge-adhoc-ipa` artifact from the completed run. The IPA is
ad-hoc signed, has no provisioning profile, and requires re-signing by a
sideloader such as SideStore or AltStore. Installation and entitlements depend
on the sideloader and account; this build alone does not guarantee JIT or device
compatibility. Failed builds upload available build and dependency logs.

The shared `ci/build-ios.sh` prepares the pinned Husk dependencies and builds
pinned StikJIT from source before packaging. StikJIT uses binary Swift modules
and an internal-distribution XCFramework: its module/enum name collision breaks
textual Swift interfaces on the current toolchain. The script type-checks an
import from the packaged framework before proceeding. Build the framework and
Henge with the same Xcode; do not reuse this XCFramework across toolchains.
`ci/build-bitrise.sh` calls the same script and retains Bitrise's generic zip
upload. The CI job requires
`make test-a12-compat` before building dependencies and packaging; it does not
run the full Mac-only `make test`. A successful build is not a device test.

## GPU diagnostic runs

A12Z enables `HV_PVG_DIAGNOSTICS=1` by default. If Documents/henge-env.txt
contains `HV_PVG_DIAGNOSTICS=0`, remove that override or change it to `1` for
a diagnostic reproduction. The renderer records host capabilities at startup,
then identifies command buffers and encoders in bounded error reports. Preserve
both application logs and system `gpuEvent` reports: a last pipeline-start line
alone cannot identify work responsible for an asynchronous GPU fault.

`HV_PVG_SYNC=1` is an existing experimental switch that waits after each GPU
batch; it is not a fix for the reproduced A12Z account-creation fault. For a
controlled comparison, leave other settings unchanged and report whether this
switch was set. Its implementation tests presence, not the value: **setting
`HV_PVG_SYNC=0` does not disable it**. Remove the line to restore the default,
then fully restart Henge and re-enable JIT. Do not overwrite unrelated tuning
lines. A run that already initialized this switch keeps its cached value.

After reproducing a failure, collect `henge.log`, `henge-native.log` and their
`-prev` counterparts before reopening Henge. Use `idevicecrashreport --keep`
when copying device reports so the originals remain on the device. Stop a
stuck VM through Henge if it still responds; repeated attempts without new
diagnostics are not evidence of recovery.

## Updating an existing installation without reinstalling macOS

Stop the guest before exporting or updating. In the Mac's settings, choose
**Export**, wait for completion, then **Move Out of Henge…** to put the result
outside Henge's container, for example in iCloud Drive or on another computer.
An export left under Files → Henge → Exports is deleted if Henge is deleted.
The export contains the guest disk, auxiliary state, ROM and machine identity;
it does not include the downloaded IPSW installer.

For iloader 2.4.1, use the same Apple account/team and install the new IPA over
the existing application. The normal install path does not uninstall first;
it deterministically appends the signing team to the IPA's original bundle ID.
An update with the same application identity normally preserves the data
container, so the existing macOS disk and downloaded installer remain in place.
Do not delete Henge first or accept a delete-and-reinstall workaround without
an external backup. A certificate renewal alone does not imply data deletion.
A different signing team produces a different bundle ID and may install a
separate empty app rather than update the old one.

After updating, verify the existing machine and disk are present, not merely
that a container directory has the same UUID: iOS may move an app's container.
Re-enable external JIT after restarting Henge. Guest macOS does not need to be
reinstalled for a renderer or JIT-guard update.

The IPSW is an ordinary file under `Library/Application Support/Machines/Restore
images` in the private app container. It is not exposed by Henge's VM export or
Files sharing, but can potentially be copied over USB with authorized full
container access (`afcclient --container`), when iPadOS permits it. Do not assume
that a Files-only backup or ordinary device backup includes it.
