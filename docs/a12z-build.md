# iOS CI build

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
