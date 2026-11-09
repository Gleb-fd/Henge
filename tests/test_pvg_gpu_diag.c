/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../devices/pvg_gpu_diag.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    PvgGpuHistory h = {0};
    assert(!pvg_gpu_contains(0, 0, 0));
    assert(!pvg_gpu_contains(100, 10, 99));
    assert(pvg_gpu_contains(100, 10, 100));
    assert(pvg_gpu_contains(100, 10, 109));
    assert(!pvg_gpu_contains(100, 10, 110));
    assert(pvg_gpu_contains(UINT64_MAX - 1, 4, UINT64_MAX));
    assert(!pvg_gpu_contains(UINT64_MAX - 1, 4, 0));
    pvg_gpu_export(&h, 1, 2, 0, 461, 100, 10);
    pvg_gpu_export(&h, 1, 2, 0, 461, 100, 10);
    assert(h.count == 1);
    pvg_gpu_export(&h, 2, 2, 0, 461, 100, 10);
    pvg_gpu_retire(&h, 1, 2, 0);
    assert(h.entries[0].retired && !h.entries[1].retired);
    pvg_gpu_export(&h, 1, 2, 1, 461, 200, 10);
    assert(h.count == 3 && !h.entries[2].retired);
    pvg_gpu_retire(&h, 2, 0, 1);
    assert(h.entries[1].retired && !h.entries[2].retired);
    uint64_t sequence = h.sequence;
    pvg_gpu_retire(&h, 9, 0, 1);
    assert(h.sequence == sequence);
    pvg_gpu_retire(&h, 1, 2, 0);
    pvg_gpu_export(&h, 1, 2, 1, 461, 200, 10);
    assert(h.count == 4 && h.entries[2].retired && !h.entries[3].retired);
    assert(h.entries[3].sequence > h.entries[2].sequence);
    pvg_gpu_export(&h, 1, 2, 1, 463, 200, 10);
    assert(h.count == 5); /* buffer and heap exports remain distinguishable */
    for (unsigned i = 0; i < 128; i++) pvg_gpu_export(&h, 3, i, 0, 461, i, 1);
    assert(h.count == 128 && h.evicted == 5);
    for (size_t i = 0; i < h.count; i++) assert(h.entries[i].task == 3);
    size_t next = h.next;
    sequence = h.sequence;
    pvg_gpu_export(&h, 3, 127, 0, 461, 127, 1);
    assert(h.evicted == 5 && h.next == next && h.sequence == sequence);
    pvg_gpu_retire(&h, 3, 0, 1);
    for (size_t i = 0; i < h.count; i++) assert(h.entries[i].retired);
    puts("pvg GPU diagnostic history: ok");
    return 0;
}
