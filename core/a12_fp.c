#include "a12_fp.h"

/* Decode only allocated encodings. */
static int decode(uint32_t i) {
    if ((i & 0xffbe7c00u) == 0x1e284000u)
        return 1;
    if ((i & 0x9fbfec00u) == 0x0e21e800u) {
        /* A 64-bit vector cannot contain a single double element. */
        if ((i & (1u << 22)) && !(i & (1u << 30))) return 0;
        return 2;
    }
    if ((i & 0x9f60dc00u) == 0x0e20cc00u &&
        !!(i & (1u << 29)) != !!(i & (1u << 13)))
        return 3;
    if ((i & 0x9fc03400u) == 0x0f800000u &&
        !!(i & (1u << 29)) == !!(i & (1u << 15)))
        return 3;
    return 0;
}

int hv_a12_fp_handles(uint32_t insn) { return decode(insn) != 0; }

/* Integer-only implementation of the architectural round-to-integral and
   signed-range check. Invalid takes precedence over inexact. */
static uint64_t round_lane(uint64_t x, int dbl, int intsize, int mode,
                           uint64_t fpcr, uint64_t *flags) {
    int fracbits = dbl ? 52 : 23;
    int bias = dbl ? 1023 : 127;
    uint64_t signbit = UINT64_C(1) << (dbl ? 63 : 31);
    uint64_t fracmask = (UINT64_C(1) << fracbits) - 1;
    unsigned expmax = dbl ? 2047 : 255;
    uint64_t sign = x & signbit, mag = x & (signbit - 1);
    unsigned exp = (unsigned)(mag >> fracbits);
    uint64_t limit = (uint64_t)(bias + intsize - 1) << fracbits;
    int inexact = 0;

    if (exp == expmax) goto invalid;
    if (exp == 0 && (mag & fracmask) && (fpcr & (1u << 24))) {
        *flags |= 1u << 7; /* input denormal */
        return sign;
    }
    if (mag) {
        int e = exp ? (int)exp - bias : 1 - bias;
        if (e < 0) {
            int increment = 0;
            inexact = 1;
            if (mode == 0) {
                uint64_t half = (uint64_t)(bias - 1) << fracbits;
                increment = mag > half; /* tie rounds to even zero */
            } else if (mode == 1) increment = !sign;
            else if (mode == 2) increment = !!sign;
            mag = increment ? (uint64_t)bias << fracbits : 0;
        } else if (e < fracbits) {
            int shift = fracbits - e;
            uint64_t unit = UINT64_C(1) << shift;
            uint64_t rem = mag & (unit - 1);
            if (rem) {
                int increment = 0;
                inexact = 1;
                if (mode == 0)
                    increment = rem > unit / 2 ||
                                (rem == unit / 2 && (mag & unit));
                else if (mode == 1) increment = !sign;
                else if (mode == 2) increment = !!sign;
                mag = (mag & ~(unit - 1)) + (increment ? unit : 0);
            }
        }
    }
    if (mag > limit || (mag == limit && !sign)) goto invalid;
    if (inexact) *flags |= 1u << 4;
    return sign | mag;
invalid:
    *flags |= 1u;
    return signbit | limit;
}

#if defined(__aarch64__)
/* Adapted from float16_to_float32_by_bits / do_fmlal in QEMU
   target/arm/tcg/vec_helper.c, Copyright (c) 2018 Linaro,
   LGPL-2.1-or-later: https://github.com/qemu/qemu/blob/master/target/arm/tcg/vec_helper.c
   Widen by bits, without quieting signaling NaNs or raising exceptions. */
static uint32_t widen_half(uint16_t h, int flush) {
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exp = (h >> 10) & 31, frac = h & 1023;
    if (exp == 31) exp = 255;
    else if (exp) exp += 112;
    else if (frac && !flush) {
        int shift = __builtin_clz(frac) - 21;
        frac = (frac << shift) & 1023;
        exp = 113 - shift;
    } else frac = 0;
    return sign | (exp << 23) | (frac << 13);
}

/* No C floating point: install guest control for exactly the FMADD, capture
   its exception flags, then restore both host registers before returning. */
static uint32_t host_fmadd(uint32_t n, uint32_t m, uint32_t a,
                           uint64_t fpcr, uint64_t *flags) {
    uint64_t oldcr, oldsr, status;
    uint32_t result;
    __asm__ volatile(
        "mrs %0, fpcr\n\t"
        "mrs %1, fpsr\n\t"
        "msr fpcr, %4\n\t"
        "msr fpsr, xzr\n\t"
        "fmov s0, %w5\n\t"
        "fmov s1, %w6\n\t"
        "fmov s2, %w7\n\t"
        "fmadd s0, s0, s1, s2\n\t"
        "fmov %w3, s0\n\t"
        "mrs %2, fpsr\n\t"
        "msr fpcr, %0\n\t"
        "msr fpsr, %1"
        : "=&r"(oldcr), "=&r"(oldsr), "=&r"(status), "=&r"(result)
        : "r"(fpcr), "r"(n), "r"(m), "r"(a)
        : "v0", "v1", "v2", "memory");
    *flags |= status;
    return result;
}

static int exec_fhm(HvCpu *cpu, uint32_t insn) {
    unsigned d = insn & 31, n = (insn >> 5) & 31;
    int element = !!(insn & (1u << 24));
    unsigned m = (insn >> 16) & (element ? 15 : 31);
    unsigned lanes = (insn & (1u << 30)) ? 4 : 2;
    unsigned first = (insn & (1u << 29)) ? lanes : 0;
    unsigned index = ((insn >> 11) & 1) << 2 |
                     ((insn >> 21) & 1) << 1 | ((insn >> 20) & 1);
    int sub = !!(insn & (1u << (element ? 14 : 23)));
    int flush = !!(cpu->fpcr & (1u << 19));
    __uint128_t acc = cpu->v[d], lhs = cpu->v[n], rhs = cpu->v[m], result = 0;
    uint64_t flags = 0;
    for (unsigned j = 0; j < lanes; ++j) {
        uint16_t nh = (uint16_t)(lhs >> (16 * (first + j)));
        uint16_t mh = (uint16_t)(rhs >> (16 * (element ? index : first + j)));
        nh ^= sub ? 0x8000 : 0;
        uint32_t v = host_fmadd(widen_half(nh, flush), widen_half(mh, flush),
                                (uint32_t)(acc >> (32 * j)), cpu->fpcr, &flags);
        result |= (__uint128_t)v << (32 * j);
    }
    cpu->v[d] = result;
    cpu->fpsr |= flags;
    return 1;
}
#endif

int hv_a12_fp_exec(HvCpu *cpu, uint32_t insn) {
    int kind = decode(insn);
    if (!kind) return 0;
    /* Exception trapping and FEAT_AFP alternate handling need exception
       delivery / alternate input rules, neither provided by this helper. */
    if (cpu->fpcr & 0x9f03u) return -1;
    if (kind == 3) {
#if defined(__aarch64__)
        return exec_fhm(cpu, insn);
#else
        return -1; /* Baseline AArch64 FMADD backend is unavailable. */
#endif
    }
    int dbl = !!(insn & (1u << 22));
    int intsize = kind == 1 ? ((insn & (1u << 16)) ? 64 : 32)
                            : ((insn & (1u << 12)) ? 64 : 32);
    int current = kind == 1 ? !!(insn & (1u << 15))
                            : !!(insn & (1u << 29));
    int mode = current ? (int)((cpu->fpcr >> 22) & 3) : 3;
    int bits = dbl ? 64 : 32;
    int lanes = kind == 1 ? 1 : ((insn & (1u << 30)) ? 128 : 64) / bits;
    unsigned rd = insn & 31, rn = (insn >> 5) & 31;
    __uint128_t src = cpu->v[rn], result = 0;
    uint64_t flags = 0;
    for (int j = 0; j < lanes; ++j) {
        uint64_t x = (uint64_t)(src >> (j * bits));
        if (!dbl) x &= UINT32_MAX;
        result |= (__uint128_t)round_lane(x, dbl, intsize, mode, cpu->fpcr,
                                         &flags) << (j * bits);
    }
    cpu->v[rd] = result;
    cpu->fpsr |= flags;
    return 1;
}
