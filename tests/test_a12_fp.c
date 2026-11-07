#include "../core/a12_fp.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static HvCpu *cpu;
#if defined(__APPLE__) && defined(__aarch64__)
#include <sys/types.h>
#include <sys/sysctl.h>

/* Each constant instruction writes v0, including the two overlap cases. */
#define NATIVE_FHM(op) __asm__ volatile( \
    "mrs %0, fpcr\n\tmrs %1, fpsr\n\tmsr fpcr, %3\n\tmsr fpsr, xzr\n\t" \
    "ldp q0, q1, [%4]\n\tldr q2, [%4, #32]\n\t.inst " #op "\n\t" \
    "str q0, [%5]\n\tmrs %2, fpsr\n\tmsr fpcr, %0\n\tmsr fpsr, %1" \
    : "=&r"(oldcr), "=&r"(oldsr), "=&r"(status) \
    : "r"(cr), "r"(input), "r"(output) : "v0", "v1", "v2", "memory")

static void native_fhm_comparison(void) {
    int supported = 0;
    size_t size = sizeof(supported);
    if (sysctlbyname("hw.optional.arm.FEAT_FHM", &supported, &size, NULL, 0) || !supported) {
        puts("native FHM comparison skipped (feature not reported)");
        return;
    }
    const uint32_t ops[] = { 0x0e22ec20, 0x6e22cc20, 0x0ea2ec20,
        0x6ea2cc20, 0x0f820020, 0x6fb28820, 0x0f824020, 0x6fb2c820,
        0x0e22ec00, 0x0e20ec20 };
    const uint16_t half[] = { 0, 0x8000, 1, 0x8001, 0x3c00, 0x4000,
        0x7bff, 0x7c00, 0xfc00, 0x7c01, 0x7e55, 0xbc00 };
    uint32_t seed = 0x34a35c21;
    for (unsigned op = 0; op < sizeof(ops)/sizeof(ops[0]); ++op)
    for (unsigned k = 0; k < 512; ++k) {
        __uint128_t input[3] = { 0, 0, 0 }, output;
        for (int r = 0; r < 3; ++r)
            for (int h = 0; h < 8; ++h) {
                seed = seed * 1664525u + 1013904223u;
                input[r] |= (__uint128_t)half[(seed >> 16) % (sizeof(half)/sizeof(half[0]))] << (16 * h);
            }
        uint64_t cr = ((uint64_t)(k & 3) << 22) | ((uint64_t)((k >> 2) & 1) << 19) |
                      ((uint64_t)((k >> 3) & 1) << 24) | ((uint64_t)((k >> 4) & 1) << 25) |
                      ((uint64_t)((k >> 5) & 1) << 26);
        uint64_t oldcr, oldsr, status;
        switch (op) {
        case 0: NATIVE_FHM(0x0e22ec20); break;
        case 1: NATIVE_FHM(0x6e22cc20); break;
        case 2: NATIVE_FHM(0x0ea2ec20); break;
        case 3: NATIVE_FHM(0x6ea2cc20); break;
        case 4: NATIVE_FHM(0x0f820020); break;
        case 5: NATIVE_FHM(0x6fb28820); break;
        case 6: NATIVE_FHM(0x0f824020); break;
        case 7: NATIVE_FHM(0x6fb2c820); break;
        case 8: NATIVE_FHM(0x0e22ec00); break;
        default: NATIVE_FHM(0x0e20ec20); break;
        }
        cpu->v[0] = input[0]; cpu->v[1] = input[1]; cpu->v[2] = input[2];
        cpu->fpcr = cr; cpu->fpsr = 0;
        assert(hv_a12_fp_exec(cpu, ops[op]) == 1);
        assert(cpu->v[0] == output && cpu->fpsr == status);
    }
    puts("native FHM vector/by-element, FPCR/FPSR, overlap comparison passed");
}
#undef NATIVE_FHM

static void native_comparison(void) {
    int supported = 0;
    size_t size = sizeof(supported);
    if (sysctlbyname("hw.optional.arm.FEAT_FRINTTS", &supported, &size, NULL, 0) || !supported) {
        puts("native FRINTTS comparison skipped (feature not reported)");
        return;
    }
    const uint32_t values[] = { 0, 1, 0x80000001, 0x3f000000, 0xbf000000,
        0x3fc00000, 0xbfc00000, 0x4f000000, 0xcf000000, 0x7fc00000,
        0x7f800000, 0x7f800001, 0x4effffff };
    for (unsigned mode = 0; mode < 4; ++mode) {
        for (unsigned flush = 0; flush < 2; ++flush) {
            for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
                uint64_t input[2] = { values[i], 0 }, output[2], status;
                uint64_t oldcr, oldsr, cr = ((uint64_t)mode << 22) | ((uint64_t)flush << 24);
                __asm__ volatile(
                    "mrs %0, fpcr\n\tmrs %1, fpsr\n\t"
                    "msr fpcr, %4\n\tmsr fpsr, xzr\n\t"
                    "ldr q1, [%5]\n\t.inst 0x1e28c020\n\t"
                    "str q0, [%6]\n\tmrs %2, fpsr\n\t"
                    "msr fpcr, %0\n\tmsr fpsr, %1"
                    : "=&r"(oldcr), "=&r"(oldsr), "=&r"(status), "=m"(output)
                    : "r"(cr), "r"(input), "r"(output)
                    : "v0", "v1", "memory");
                cpu->fpcr = cr;
                cpu->fpsr = 0;
                cpu->v[1] = values[i];
                assert(hv_a12_fp_exec(cpu, 0x1e28c020) == 1);
                assert(cpu->v[0] == ((__uint128_t)output[1] << 64 | output[0]));
                assert(cpu->fpsr == status);
            }
        }
    }
    puts("native FRINT32X scalar S comparison passed");
}
#endif

static void check(uint32_t insn, uint64_t input, uint64_t fpcr,
                  uint64_t expected, uint64_t flags) {
    cpu->v[1] = input;
    cpu->v[0] = ~(__uint128_t)0;
    cpu->fpcr = fpcr;
    cpu->fpsr = 0;
    assert(hv_a12_fp_handles(insn));
    assert(hv_a12_fp_exec(cpu, insn) == 1);
    assert(cpu->v[0] == expected);
    assert(cpu->fpsr == flags);
}
int main(void) {
    cpu = calloc(1, sizeof(*cpu));
    assert(cpu);
    const uint32_t z32s = 0x1e284020, x32s = 0x1e28c020;
    const uint32_t z64d = 0x1e694020, x32d = 0x1e68c020;
    check(z32s, 0x3fc00000, 0, 0x3f800000, 0x10);
    check(x32s, 0x3fc00000, 0, 0x40000000, 0x10);
    check(x32s, 0x3f000000, 0, 0, 0x10);
    check(x32s, 0xbf000000, 0, 0x80000000, 0x10);
    check(x32s, 0xbf000000, 2u << 22, 0xbf800000, 0x10);
    check(x32s, 0x3f000000, 1u << 22, 0x3f800000, 0x10);
    check(x32s, 1, 1u << 24, 0, 0x80);
    check(x32s, 0x80000001, 1u << 24, 0x80000000, 0x80);
    check(z32s, 0x7fc00000, 0, 0xcf000000, 1);
    check(z32s, 0x7f800000, 0, 0xcf000000, 1);
    check(z32s, 0xcf000000, 0, 0xcf000000, 0);
    check(z32s, 0x4f000000, 0, 0xcf000000, 1);
    check(z64d, UINT64_C(0xc3e0000000000000), 0,
          UINT64_C(0xc3e0000000000000), 0);
    check(z64d, UINT64_C(0x43e0000000000000), 0,
          UINT64_C(0xc3e0000000000000), 1);
    check(x32d, UINT64_C(0x41dfffffffe00000), 1u << 22,
          UINT64_C(0xc1e0000000000000), 1);
    /* Vector aliases source and destination; Q=0 clears the upper half. */
    cpu->v[0] = ((__uint128_t)0x1234 << 64) | UINT64_C(0xbfc000003fc00000);
    cpu->fpcr = cpu->fpsr = 0;
    assert(hv_a12_fp_exec(cpu, 0x0e21e800) == 1);
    /* Rn is 0 in this encoding. */
    assert(cpu->v[0] == UINT64_C(0xbf8000003f800000));
    assert(cpu->fpsr == 0x10);
    const uint32_t fhm[] = { 0x0e22ec20, 0x6e22cc20, 0x0ea2ec20,
        0x6ea2cc20, 0x0f820020, 0x6fb28820, 0x0f824020, 0x6fb2c820 };
    for (unsigned i = 0; i < sizeof(fhm)/sizeof(fhm[0]); ++i) {
        assert(hv_a12_fp_handles(fhm[i]));
#if defined(__aarch64__)
        cpu->v[0] = 0; cpu->v[1] = 0; cpu->v[2] = 0;
        cpu->fpcr = cpu->fpsr = 0;
        assert(hv_a12_fp_exec(cpu, fhm[i]) == 1);
        assert(cpu->v[0] == 0 && cpu->fpsr == 0);
#else
        __uint128_t before = cpu->v[0];
        uint64_t fpsr = cpu->fpsr;
        assert(hv_a12_fp_exec(cpu, fhm[i]) == -1);
        assert(cpu->v[0] == before && cpu->fpsr == fpsr);
#endif
    }
#if defined(__aarch64__)
    /* Two half products added to single-precision accumulators. */
    cpu->v[0] = ((__uint128_t)~(uint64_t)0 << 64) | UINT64_C(0x400000003f800000);
    cpu->v[1] = UINT64_C(0x40003c00); /* 1, 2 */
    cpu->v[2] = UINT64_C(0x42003800); /* .5, 3 */
    cpu->fpcr = cpu->fpsr = 0;
    assert(hv_a12_fp_exec(cpu, 0x0e22ec20) == 1);
    assert(cpu->v[0] == UINT64_C(0x410000003fc00000));
    assert(cpu->fpsr == 0);
    /* A helper call must not leak guest control or flags into host state. */
    uint64_t oldcr, oldsr, aftercr, aftersr;
    __asm__ volatile("mrs %0, fpcr\n\tmrs %1, fpsr" : "=r"(oldcr), "=r"(oldsr));
    uint64_t hostcr = 2u << 22, hostsr = 0x10;
    __asm__ volatile("msr fpcr, %0\n\tmsr fpsr, %1" :: "r"(hostcr), "r"(hostsr) : "memory");
    cpu->fpcr = 0;
    cpu->fpsr = 0x80;
    assert(hv_a12_fp_exec(cpu, 0x0e22ec20) == 1);
    __asm__ volatile("mrs %0, fpcr\n\tmrs %1, fpsr\n\tmsr fpcr, %2\n\tmsr fpsr, %3"
                     : "=&r"(aftercr), "=&r"(aftersr) : "r"(oldcr), "r"(oldsr) : "memory");
    assert(aftercr == hostcr && aftersr == hostsr);
    assert(cpu->fpsr & 0x80);
    puts("FHM baseline AArch64 FMADD backend tested");
#else
    puts("FHM recognized but unsupported: AArch64 FMADD backend unavailable");
#endif
    assert(!hv_a12_fp_handles(0x0e61e820)); /* reserved 1D */
    assert(!hv_a12_fp_handles(0xd503201f));
    cpu->fpcr = 1u << 8;
    assert(hv_a12_fp_exec(cpu, z32s) == -1);
#if defined(__APPLE__) && defined(__aarch64__)
    native_comparison();
    native_fhm_comparison();
#endif
    free(cpu);
    puts("A12 FP tests passed");
    return 0;
}
