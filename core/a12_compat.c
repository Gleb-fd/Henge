#include "a12_compat.h"

enum { NONE, DOT, DOT_ELEM, H, H2, SU0, SU1, EOR3, BCAX, RAX1, XAR, CFINV, RMIF, SETF8, SETF16 };

static int decode(uint32_t x) {
    if ((x & 0x9fe0fc00u) == 0x0e809400u) return DOT;
    if ((x & 0x9fc0f400u) == 0x0f80e000u) return DOT_ELEM;
    if ((x & 0xffe0fc00u) == 0xce608000u) return H;
    if ((x & 0xffe0fc00u) == 0xce608400u) return H2;
    if ((x & 0xfffffc00u) == 0xcec08000u) return SU0;
    if ((x & 0xffe0fc00u) == 0xce608800u) return SU1;
    if ((x & 0xffe08000u) == 0xce000000u) return EOR3;
    if ((x & 0xffe08000u) == 0xce200000u) return BCAX;
    if ((x & 0xffe0fc00u) == 0xce608c00u) return RAX1;
    if ((x & 0xffe00000u) == 0xce800000u) return XAR;
    if (x == 0xd500401fu) return CFINV;
    if ((x & 0xffe07c10u) == 0xba000400u) return RMIF;
    if ((x & 0xfffffc1fu) == 0x3a00080du) return SETF8;
    if ((x & 0xfffffc1fu) == 0x3a00480du) return SETF16;
    return NONE;
}
int hv_a12_compat_handles(uint32_t x) { return decode(x) != NONE; }
static uint64_t ror(uint64_t x, unsigned n) { return n ? (x >> n) | (x << (64-n)) : x; }
static uint64_t lo(__uint128_t x) { return (uint64_t)x; }
static uint64_t hi(__uint128_t x) { return (uint64_t)(x >> 64); }
static __uint128_t pair(uint64_t a, uint64_t b) { return (__uint128_t)b << 64 | a; }
static uint64_t big0(uint64_t x) { return ror(x,28)^ror(x,34)^ror(x,39); }
static uint64_t big1(uint64_t x) { return ror(x,14)^ror(x,18)^ror(x,41); }
static uint64_t small0(uint64_t x) { return ror(x,1)^ror(x,8)^(x>>7); }
static uint64_t small1(uint64_t x) { return ror(x,19)^ror(x,61)^(x>>6); }
static uint64_t choose(uint64_t x,uint64_t y,uint64_t z) { return (x & y) ^ (~x & z); }
static uint64_t majority(uint64_t x,uint64_t y,uint64_t z) { return (x & y) | ((x | y) & z); }

int hv_a12_compat_exec(HvCpu *cpu, uint32_t x) {
    int op = decode(x);
    unsigned d=x&31, n=(x>>5)&31, m=(x>>16)&31;
    if (!op) return 0;
    if (op == CFINV) { cpu->nzcv ^= 1u<<29; return 1; }
    if (op == RMIF) {
        uint64_t value = ror(n == 31 ? 0 : cpu->x[n], (x>>15)&63);
        uint64_t mask = (uint64_t)(x&15)<<28;
        cpu->nzcv = (cpu->nzcv & ~mask) | ((value << 28) & mask);
        return 1;
    }
    if (op == SETF8 || op == SETF16) {
        unsigned width = op == SETF8 ? 8 : 16;
        uint64_t value = n == 31 ? 0 : cpu->x[n];
        uint64_t sign = (value >> (width-1)) & 1;
        uint64_t overflow = sign ^ ((value >> width) & 1);
        cpu->nzcv = (cpu->nzcv & ~(0xdULL<<28)) | (sign<<31) |
                    ((uint64_t)((value & ((1u<<width)-1)) == 0)<<30) | (overflow<<28);
        return 1;
    }
    __uint128_t dv=cpu->v[d], nv=cpu->v[n], mv=cpu->v[m];
    uint64_t a=lo(dv), b=hi(dv), c=lo(nv), e=hi(nv), f=lo(mv), g=hi(mv);
    if (op == DOT || op == DOT_ELEM) {
        __uint128_t out=0;
        unsigned lanes=(x & (1u<<30)) ? 4 : 2;
        unsigned index=(((x>>11)&1)<<1) | ((x>>21)&1);
        int unsign=(x>>29)&1;
        for (unsigned i=0;i<lanes;i++) {
            uint32_t sum=(uint32_t)(dv>>(32*i));
            for (unsigned j=0;j<4;j++) {
                unsigned av=(unsigned)(nv>>(8*(i*4+j)))&255;
                unsigned bv=(unsigned)(mv>>(8*((op==DOT ? i : index)*4+j)))&255;
                int avs=unsign ? (int)av : (int)(av^128)-128;
                int bvs=unsign ? (int)bv : (int)(bv^128)-128;
                sum += (uint32_t)(avs*bvs);
            }
            out |= (__uint128_t)sum << (32*i);
        }
        cpu->v[d]=out; return 1;
    }
    /* SHA-512 transformations adapted from QEMU target/arm/tcg/crypto_helper.c,
       Copyright (C) 2013-2018 Linaro Ltd <ard.biesheuvel@linaro.org>,
       LGPL-2.1-or-later. Inputs are snapshots; see docs/licensing.md. */
    switch (op) {
    case H: b += big1(g)+choose(g,c,e); a += big1(b+f)+choose(b+f,g,c); break;
    case H2: b += big0(f)+majority(c,g,f); a += big0(b)+majority(b,f,g); break;
    case SU0: a += small0(b); b += small0(c); break;
    case SU1: a += small1(c)+f; b += small1(e)+g; break;
    case EOR3: { __uint128_t v=cpu->v[(x>>10)&31]; cpu->v[d]=nv^mv^v; return 1; }
    case BCAX: { __uint128_t v=cpu->v[(x>>10)&31]; cpu->v[d]=nv^(mv & ~v); return 1; }
    case RAX1: a=c^ror(f,63); b=e^ror(g,63); break;
    case XAR: a=ror(c^f,(x>>10)&63); b=ror(e^g,(x>>10)&63); break;
    default: return 0;
    }
    cpu->v[d]=pair(a,b); return 1;
}
