# iOS CI build

## Codemagic

1. Open [Applications](https://codemagic.io/apps), add or select
   `Gleb-fd/Henge`, and use the YAML configuration from the repository.
2. Select branch `experimental/a12z` and check for the configuration file.
3. Start workflow `a12z-unsigned-ipa` (**Henge A12Z experimental IPA**).
4. The workflow requests an Apple Silicon Mac mini M2 with the latest Xcode.
   Its preflight requires Xcode 27 or later; if your account's latest image is
   older, choose an available Xcode 27+ image before retrying. Check your plan's
   included minutes and machine availability; this configuration does not buy
   credits or enable paid billing.
5. Download `Henge-A12Z-experimental.ipa` from the build artifacts. Re-sign it
   with your sideloader before installing. No Apple signing secrets are needed
   for this build.

Tests run before the dependency build. `ci-logs/a12-tests.log`,
`ci-logs/build.log` and Xcode's `ios/build/*.buildlog` are collected as artifacts
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
pinned StikJIT from source before packaging. `ci/build-bitrise.sh` calls the
same script and retains Bitrise's generic zip upload. The CI job requires
`make test-a12-compat` before building dependencies and packaging; it does not
run the full Mac-only `make test`. A successful build is not a device test.
