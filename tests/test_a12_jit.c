#include <stdio.h>
#if defined(__APPLE__)
#include "harness.h"
#include <stdlib.h>
#include <sys/sysctl.h>

DECLARE_PROGRAM(a12_dot);
DECLARE_PROGRAM(a12_sha512);
DECLARE_PROGRAM(a12_sha3);
DECLARE_PROGRAM(a12_flagm);
DECLARE_PROGRAM(a12_frint);
DECLARE_PROGRAM(a12_fhm);
DECLARE_PROGRAM(a12_sb);
extern const char a12_programs_begin[], a12_programs_finish[];

typedef struct {
    TestProgram program;
    const char *feature;
    int fp_inputs;
} Case;

static const Case cases[] = {
    { TEST_PROGRAM(a12_dot), "hw.optional.arm.FEAT_DotProd", 0 },
    { TEST_PROGRAM(a12_sha512), "hw.optional.arm.FEAT_SHA512", 0 },
    { TEST_PROGRAM(a12_sha3), "hw.optional.arm.FEAT_SHA3", 0 },
    { TEST_PROGRAM(a12_flagm), "hw.optional.arm.FEAT_FlagM", 0 },
    { TEST_PROGRAM(a12_frint), "hw.optional.arm.FEAT_FRINTTS", 1 },
    { TEST_PROGRAM(a12_fhm), "hw.optional.arm.FEAT_FHM", 1 },
    { TEST_PROGRAM(a12_sb), "hw.optional.arm.FEAT_SB", 0 },
};

static int has_feature(const char *name) {
    int value = 0;
    size_t size = sizeof value;
    return sysctlbyname(name, &value, &size, NULL, 0) == 0 && value != 0;
}

int main(void) {
    static HvMachine machine;
    static HvCpu init, native, jit;
    if (setenv("HV_A12_COMPAT", "1", 1) || hv_machine_init(&machine, 4 << 20)) {
        puts("machine init failed");
        return 1;
    }
    if (!machine.a12_compat) {
        puts("A12 compatibility mode not enabled");
        return 1;
    }
    uint8_t *stack = aligned_alloc(16384, 65536);
    if (!stack) return 1;
    hv_machine_add_ram(&machine, (uint64_t)(uintptr_t)a12_programs_begin,
                       a12_programs_finish - a12_programs_begin, (void *)a12_programs_begin, 1);
    hv_machine_add_ram(&machine, (uint64_t)(uintptr_t)stack, 65536, stack, 0);
    uint64_t sp = (uint64_t)(uintptr_t)stack + 32768;
    int failed = 0, skipped = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const Case *c = &cases[i];
        if (!has_feature(c->feature)) {
            printf("%-14s SKIP (%s unavailable or not reported)\n", c->program.name, c->feature);
            skipped++;
            continue;
        }
        for (unsigned variant = 0; variant < 4; variant++) {
            memset(&init, 0, sizeof init);
            seed_cpu(&init, 17 + i * 4 + variant, sp);
            init.nzcv = variant << 28;
            init.fpcr = variant << 22; // all rounding modes
            init.fpsr = variant & 1 ? 0x10 : 0;
            if (c->fp_inputs) {
                // Finite singles and halves, with fractional values for rounding.
                static const uint64_t bits[] = {
                    0x3fc000003f000000ULL, 0xbf40000040200000ULL,
                    0x38003e00bc003c00ULL, 0x4000b80034003a00ULL
                };
                for (unsigned r = 0; r < 32; r++) {
                    uint64_t lo = bits[(r + variant) % 4];
                    uint64_t hi = bits[(r + variant + 1) % 4];
                    init.v[r] = (__uint128_t)hi << 64 | lo;
                }
            }
            run_native(&c->program, &init, &native, NULL);
            int bad = run_jit(&machine, &c->program, &init, &jit, NULL);
            if (!bad) bad = compare_cpus(c->program.name, &native, &jit);
            printf("%-14s seed %u: %s\n", c->program.name, variant, bad ? "FAIL" : "ok");
            failed += bad != 0;
        }
    }
    printf("A12 differential: %d failures, %d feature groups skipped\n", failed, skipped);
    free(stack);
    return failed != 0;
}
#else
int main(void) {
    puts("A12 differential tests require Apple Silicon; SKIP");
    return 0;
}
#endif
