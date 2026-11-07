# Experimental A12Z port

This branch targets the 2020 iPad Pro models `iPad8,9`, `iPad8,10`, `iPad8,11`
and `iPad8,12`. It is a bring-up build, not a claim that macOS installs or boots
on these devices. GPU replay and system-wide memory pressure still require
on-device testing. The upstream minimum remains A14/M1 outside this experiment.

## CPU path

The app sets `HV_A12_COMPAT=1` before constructing any virtual machine on A12Z.
The machine snapshots that setting at initialization: changing it during a
session does not change already translated blocks. To exercise the path on an
Apple Silicon Mac, set the same environment variable before starting the engine.

The guest ID registers stay unchanged. Instructions that used to be copied
into host code are instead routed through the full saved-state helper:

- SDOT/UDOT, vector and indexed forms;
- SHA-512 H/H2/SU0/SU1 and SHA-3 EOR3/BCAX/RAX1/XAR;
- FlagM CFINV/RMIF/SETF8/SETF16;
- FRINT32Z/X and FRINT64Z/X, scalar and vector;
- FMLAL/FMLSL and their upper-source and indexed forms.

Integer/crypto operations use portable integer arithmetic. FRINT uses integer
bit manipulation rather than host floating-point casts. FHM on AArch64 expands
half operands by bits, then uses baseline single-precision fused multiply-add
under the guest FP environment; it must not execute the unavailable FHM opcode.
SB is lowered to `DSB SY; ISB`.

The existing assembly helper saves and restores GPRs, vector registers and NZCV.
The compatibility dispatch additionally captures and reinstalls FPCR/FPSR.
The existing CPACR FP-access trap check takes precedence over emulation.

Unsupported FP exception-trapping or alternate-handling modes stop with
`HV_EXIT_UNSUPPORTED` and log the guest PC and opcode. They must not fall back to
native execution. This is not a general interpreter for every extension newer
than A12; newly encountered instruction families may still need support.

## Tests

Run `make test-a12-compat`:

- On Linux/x86, run the standalone integer/crypto and FRINT semantic tests.
  AArch64-only FHM execution is not validated by that run.
- On Apple Silicon macOS, also run native-reference versus forced-compatibility
  JIT programs. Native instructions are gated by host capability queries;
  skipped groups are printed, not counted as tested.

The GitHub Actions workflow requires this target to pass before building the IPA.
Native CPU tests do not validate Metal replay, restore images, iPadOS JIT setup,
application signing or memory pressure. The full upstream `make test` is a
separate suite, not implied by this target.

## First device test

1. Install a re-signed IPA; do not use an ad-hoc-signed artifact directly.
2. On iPadOS 18, use external StikDebug with its pairing and loopback VPN setup.
   The built-in helper requires iOS 26 or later.
3. Verify the JIT self-test and the `experimental A12 instruction compatibility
   enabled` log entry before attempting an install.
4. Start with Sequoia 15.6.1, 3 GB guest RAM, two guest CPUs, and no Retina mode.
   These are conservative starting settings, not validated recommendations.
5. Keep the device connected to power and preserve `henge.log` and
   `henge-native.log` after failures. An unsupported-opcode stop is useful
   evidence; a successful build is not proof of a successful macOS boot.

Build instructions are in [a12z-build.md](a12z-build.md).
