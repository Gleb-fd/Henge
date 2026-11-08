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

## Device crash evidence and diagnostic build

The first A12Z runs reached sustained guest instruction execution and GPU
surface creation. This does not establish a completed macOS setup, a usable
desktop or complete CPU correctness. One captured boot used four CPUs,
2560 MB effective RAM (4096 MB requested), a 1024 MB JIT region and 120 Hz;
these are observations, not the conservative settings above.

Three system reports terminate in `AGXMetalA12` while creating a render encoder,
with `BackgroundObjectProgram` compilation on the stack and a LIBSYSTEM
“Application Triggered Fault” termination. These are not evidence of a jetsam
kill. The exact framebuffer configuration or underlying resource failure is
not yet known. Formats 80 (BGRA8Unorm) and 115 (RGBA16Float) are supported on
this GPU; their allocation failures do not prove a missing format capability.
A separately logged unlowered AIR intrinsic is a shader compatibility error,
but has not been tied to the fatal render-pass failure.

The diagnostic patch rejects truncated replay records and passes whose nonzero
attachment/resolve/visibility references cannot be resolved. It checks texture
subresources before calling Metal, rather than silently replacing missing
requested attachments or resolves with different load/store semantics. A
rejected pass is counted and logged; it can leave missing graphics. This is
not a replacement renderer or a guarantee against driver termination.

`HV_PVG_DIAGNOSTICS=1` logs bounded raw pass data, attachment properties and
pipeline/library starts before entering Metal. It is enabled by default only
on the experimental A12Z devices. To reduce diagnostic I/O, set
`HV_PVG_DIAGNOSTICS=0` in Documents/henge-env.txt before starting a session.
The logs still grow during a run; the bound is per diagnostic entry, not a
whole-session size limit. A final “pass start” is evidence of work submitted,
not proof that an asynchronous compiler fault belongs to that exact pass.
`NSException` handling cannot recover safely from native driver aborts or
SIGKILL.

A separate report faults while Darwin lazily instantiates a thread-local
variable inside the JIT signal guard. The patch removes that TLS access and
recognizes only the saved PCs of the three exact BRK request instructions.
It does not swallow unrelated traps or establish why the original GLib trap
occurred.

`make test-runtime-guards` covers portable wire-length and trap-PC policies.
Darwin arm64 also exercises the three real BRK routines; Linux reports that
part as skipped. iOS/macOS SDK compilation and another A12Z reproduction are
still required. Keep both application logs and system crash reports; when
using `idevicecrashreport`, pass `--keep` to copy without removing device reports.

Build and data-preserving update instructions are in
[a12z-build.md](a12z-build.md). No guest macOS reinstall is required by these
source changes.
