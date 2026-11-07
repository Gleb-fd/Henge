#include "harness.h"

.text
.p2align 14
.globl _a12_programs_begin
.globl _a12_programs_finish
_a12_programs_begin:

// Encodings assembled with armv8.5-a extensions; no global -march required.
PROGRAM a12_dot
    fadd s31, s30, s29
    .inst 0x0e829420 // sdot v0.2s, v1.8b, v2.8b
    .inst 0x4e849463 // sdot v3.4s, v3.16b, v4.16b
    .inst 0x2e8594c5 // udot v5.2s, v6.8b, v5.8b
    .inst 0x6e899507 // udot v7.4s, v8.16b, v9.16b
    .inst 0x0f8ce16a // sdot v10.2s, v11.8b, v12.4b[0]
    .inst 0x4faee9ad // sdot v13.4s, v13.16b, v14.4b[3]
    .inst 0x2f91ea0f // udot v15.2s, v16.8b, v17.4b[2]
    .inst 0x6fb2e272 // udot v18.4s, v19.16b, v18.4b[1]
    fadd s31, s31, s30
END a12_dot

PROGRAM a12_sha512
    fadd s31, s30, s29
    .inst 0xce628020 // sha512h q0, q1, v2.2d
    .inst 0xce648463 // sha512h2 q3, q3, v4.2d
    .inst 0xcec080a5 // sha512su0 v5.2d, v5.2d
    .inst 0xce6688e6 // sha512su1 v6.2d, v7.2d, v6.2d
    fadd s31, s31, s30
END a12_sha512

PROGRAM a12_sha3
    .inst 0xce092908 // eor3 v8.16b, v8.16b, v9.16b, v10.16b
    .inst 0xce2b358b // bcax v11.16b, v12.16b, v11.16b, v13.16b
    .inst 0xce6e8dee // rax1 v14.2d, v15.2d, v14.2d
    .inst 0xce919610 // xar v16.2d, v16.2d, v17.2d, #37
END a12_sha3

PROGRAM a12_flagm
    adds x0, x0, x1
    .inst 0xd500401f // cfinv
    adc x2, x2, xzr
    .inst 0xba0384cf // rmif x6, #7, #15
    cset x3, eq
    .inst 0xba0f84e5 // rmif x7, #31, #5
    cset x4, cs
    .inst 0xba1f85ea // rmif x15, #63, #10
    cset x5, mi
    .inst 0xba00064f // rmif x18, #0, #15
    cset x8, vs
    .inst 0xba00878f // rmif x28, #1, #15
    cset x9, eq
    .inst 0x3a0008cd // setf8 w6
    cset x10, vs
    .inst 0x3a0048ed // setf16 w7
    cset x11, eq
    .inst 0x3a0009ed // setf8 w15
    cset x12, mi
    .inst 0x3a004a4d // setf16 w18
    cset x13, cs
    .inst 0x3a000b8d // setf8 w28
    cset x14, vs
END a12_flagm

PROGRAM a12_frint
    fadd s31, s30, s29
    .inst 0x1e28c020 // frint32x s0, s1
    .inst 0x1e684062 // frint32z d2, d3
    .inst 0x6e21f8a4 // frint64x v4.4s, v5.4s
    .inst 0x4e61f8e6 // frint64z v6.2d, v7.2d
    fadd s31, s31, s30
END a12_frint

PROGRAM a12_fhm
    fadd s31, s30, s29
    .inst 0x0e22ec20 // fmlal v0.2s, v1.2h, v2.2h
    .inst 0x6ea5cc83 // fmlsl2 v3.4s, v4.4h, v5.4h
    .inst 0x4fb800e6 // fmlal v6.4s, v7.4h, v8.h[3]
    .inst 0x6fbbc949 // fmlsl2 v9.4s, v10.4h, v11.h[7]
    fadd s31, s31, s30
END a12_fhm

PROGRAM a12_sb
    adds x0, x1, x2
    .inst 0xd50330ff // sb
    adc x3, x4, x5
END a12_sb

.p2align 14
_a12_programs_finish:
