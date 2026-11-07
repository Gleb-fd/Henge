#include "../core/a12_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static __uint128_t pair(uint64_t lo, uint64_t hi) { return (__uint128_t)hi << 64 | lo; }
static void run(HvCpu *c, uint32_t insn) { assert(hv_a12_compat_handles(insn)); assert(hv_a12_compat_exec(c, insn)); }
int main(void) {
    HvCpu *c = calloc(1, sizeof(*c)); assert(c);
    c->v[0]=pair(0,~0ULL); c->v[1]=pair(0x0101010101010101ULL,0); c->v[2]=c->v[1];
    run(c,0x0e829420); assert(c->v[0]==pair(0x0000000400000004ULL,0));
    c->v[0]=0; c->v[1]=pair(0xffffffffffffffffULL,~0ULL); c->v[2]=c->v[1];
    run(c,0x4e829420); assert(c->v[0]==pair(0x0000000400000004ULL,0x0000000400000004ULL));
    c->v[0]=0; run(c,0x2e829420); assert(c->v[0]==pair(0x0003f8040003f804ULL,0));
    c->v[0]=0; run(c,0x4fa2e820); assert(c->v[0]==pair(0x0000000400000004ULL,0x0000000400000004ULL));
    c->v[0]=0; run(c,0x2fa2e020); assert(c->v[0]==pair(0x0003f8040003f804ULL,0));
    c->v[1]=pair(1,2); c->v[2]=pair(4,8); c->v[3]=pair(16,32);
    run(c,0xce020c20); assert(c->v[0]==pair(21,42));
    run(c,0xce220c20); assert(c->v[0]==pair(5,10));
    run(c,0xce628c20); assert(c->v[0]==pair(9,18));
    run(c,0xce82fc20); assert(c->v[0]==pair(10,20));
    c->v[0]=0; c->v[1]=0; c->v[2]=0;
    run(c,0xce628020); assert(c->v[0]==0);
    run(c,0xce628420); assert(c->v[0]==0);
    run(c,0xcec08020); assert(c->v[0]==0);
    run(c,0xce628820); assert(c->v[0]==0);
    /* Nonzero SHA-512 results independently calculated from the scalar round equations. */
    const uint32_t sha[] = {0xce628020,0xce628420,0xcec08020,0xce628820};
    const uint64_t expected[][2] = {
        {0x0027c06064800003ULL,0x0019800003000004ULL},
        {0x5014009252000506ULL,0x000000514a000009ULL},
        {0x0200000000000002ULL,0x8300000000000003ULL},
        {0x000060000000001eULL,0x0000800000000028ULL}
    };
    for (unsigned i=0;i<4;i++) {
        c->v[0]=pair(1,2); c->v[1]=pair(3,4); c->v[2]=pair(5,6);
        run(c,sha[i]); assert(c->v[0]==pair(expected[i][0],expected[i][1]));
    }
    /* Every register alias is equivalent to using snapshots of the same values. */
    const uint32_t ops[] = {0x4e829420,0x4fa2e820,0xce628020,0xce628420,
        0xcec08020,0xce628820,0xce020c20,0xce220c20,0xce628c20,0xce82fc20};
    for (unsigned k=0;k<sizeof(ops)/sizeof(ops[0]);k++) {
        for (unsigned alias=0;alias<4;alias++) {
            unsigned dn=alias==1 ? 0 : 1, dm=alias==2 ? 0 : 2, da=alias==3 ? 0 : 3;
            __uint128_t seed=pair(0x123456789abcdef0ULL,0xfedcba9876543210ULL);
            c->v[0]=seed; c->v[1]=seed; c->v[2]=seed; c->v[3]=seed;
            run(c,ops[k]); __uint128_t result=c->v[0];
            c->v[0]=seed; c->v[1]=seed; c->v[2]=seed; c->v[3]=seed;
            uint32_t enc=(ops[k]&~(31u<<5)) | (dn<<5);
            if (k!=4) enc=(enc & ~(31u<<16)) | (dm<<16);
            if (k==6 || k==7) enc=(enc & ~(31u<<10)) | (da<<10);
            run(c,enc); assert(c->v[0]==result);
        }
    }
    c->nzcv=0; run(c,0xd500401f); assert(c->nzcv==(1u<<29));
    c->x[1]=0x8000000000000007ULL; run(c,0xba1f842f); assert((c->nzcv>>28)==15);
    c->x[1]=0x180; run(c,0x3a00082d); assert((c->nzcv>>28)==10);
    c->x[1]=0; run(c,0x3a00482d); assert((c->nzcv>>28)==6);
    run(c,0x3a000bed); assert((c->nzcv>>28)==6); /* WZR */
    uint64_t before=c->nzcv; assert(!hv_a12_compat_exec(c,0)); assert(c->nzcv==before);
    assert(!hv_a12_compat_handles(0x0e009420));
    assert(hv_a12_compat_handles(0x4f82e820));
    /* Indexed dot-product size=11 is reserved, not another vector width. */
    __uint128_t vector_before = c->v[0];
    assert(!hv_a12_compat_handles(0x4fc2e820));
    assert(!hv_a12_compat_exec(c, 0x4fc2e820));
    assert(c->v[0] == vector_before);
    free(c); puts("a12 compatibility tests passed");
}
