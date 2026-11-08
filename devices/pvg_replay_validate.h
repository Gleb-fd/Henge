/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PVG_REPLAY_VALIDATE_H
#define PVG_REPLAY_VALIDATE_H
#include <stddef.h>
#include <stdint.h>

static inline int pvg_range(size_t size, size_t off, size_t len) {
    return off <= size && len <= size - off;
}
static inline int pvg_table(size_t size, size_t off, size_t count, size_t stride, size_t tail) {
    return off <= size && (!stride || count <= (size - off) / stride) &&
        pvg_range(size, off + count * stride, tail);
}
static inline int pvg_serialized(size_t size, uint32_t length) {
    return pvg_range(size, 16, length);
}
static inline int pvg_texture_table(size_t size, size_t mips) {
    return pvg_table(size, 12 + 0x18, mips, 36, 12);
}
static inline int pvg_texture_desc(size_t size, size_t offset, int v2) {
    return pvg_range(size, offset, v2 ? 39 : 24);
}
static inline size_t pvg_linear_texture_min_size(uint32_t tag) {
    switch (tag) {
    case 0x34: return 12 + 39;
    case 0x14: case 1: return 12 + 24;
    case 0x15: return 16 + 0x24 + 8;
    case 0x38: return 16 + 0x2c + 8;
    case 7: return 16 + 2; /* parent and pixel format only */
    case 8: return 16 + 4 + 32; /* type, mip range and slice range */
    case 0x1b: return 16 + 40; /* also includes four swizzle bytes */
    case 9: return 32 + 24;
    case 0x37: return 32 + 39;
    default: return 4;
    }
}
static inline int pvg_pass_fixed(size_t size) {
    return size >= 0x248 || (size >= 0x4c && size <= 0x22c &&
                           (size - 0x4c) % 0x3c == 0);
}
static inline int pvg_pass_color(size_t size, unsigned i) {
    return i < 8 && pvg_range(size, 0x4c + (size_t)i * 0x3c, 0x3c);
}
static inline int pvg_pass_metadata(size_t size) { return size >= 0x248; }
#endif
