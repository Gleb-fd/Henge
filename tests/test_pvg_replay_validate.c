/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../devices/pvg_replay_validate.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    assert(!pvg_range(3, 0, 4));
    assert(pvg_range(4, 0, 4));
    assert(!pvg_range(SIZE_MAX, SIZE_MAX - 1, 4));
    assert(!pvg_serialized(15, 0));
    assert(pvg_serialized(16, 0));
    assert(!pvg_serialized(16, UINT32_MAX));
    assert(pvg_serialized(20, 4));
    assert(!pvg_texture_table(35, 0));
    assert(!pvg_texture_table(47, 0));
    assert(pvg_texture_table(48, 0));
    assert(!pvg_texture_table(83, 1));
    assert(pvg_texture_table(84, 1));
    assert(!pvg_table(SIZE_MAX, 36, SIZE_MAX, 36, 12));
    assert(!pvg_texture_desc(23, 0, 0));
    assert(pvg_texture_desc(24, 0, 0));
    assert(!pvg_texture_desc(38, 0, 1));
    assert(pvg_texture_desc(39, 0, 1));
    assert(!pvg_pass_fixed(0x4b));
    assert(pvg_pass_fixed(0x4c));
    const size_t partial_passes[] = {0x4d, 0x50, 0x87, 0x22b, 0x22d, 0x230, 0x247};
    for (size_t i = 0; i < sizeof partial_passes / sizeof partial_passes[0]; i++)
        assert(!pvg_pass_fixed(partial_passes[i]));
    for (size_t i = 0; i <= 8; i++) assert(pvg_pass_fixed(0x4c + i * 0x3c));
    assert(pvg_pass_fixed(0x248));
    assert(pvg_pass_fixed(0x250));
    assert(pvg_linear_texture_min_size(7) == 18);
    assert(pvg_linear_texture_min_size(8) == 52);
    assert(pvg_linear_texture_min_size(0x1b) == 56);
    assert(pvg_linear_texture_min_size(0x34) == 51);
    assert(pvg_linear_texture_min_size(0x14) == 36);
    assert(pvg_linear_texture_min_size(1) == 36);
    assert(pvg_linear_texture_min_size(0x15) == 60);
    assert(pvg_linear_texture_min_size(0x38) == 68);
    assert(pvg_linear_texture_min_size(9) == 56);
    assert(pvg_linear_texture_min_size(0x37) == 71);
    assert(!pvg_pass_color(0x87, 0));
    assert(pvg_pass_color(0x88, 0));
    assert(!pvg_pass_color(0x22b, 7));
    assert(pvg_pass_color(0x22c, 7));
    assert(!pvg_pass_color(SIZE_MAX, 8));
    assert(!pvg_pass_metadata(0x247));
    assert(pvg_pass_metadata(0x248));
    puts("pvg replay validation: ok");
}
