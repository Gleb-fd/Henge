/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PVG_GPU_DIAG_H
#define PVG_GPU_DIAG_H
#include <stdint.h>
#include <stddef.h>

enum { PVG_GPU_HISTORY_CAP = 128 };
typedef struct {
    uint32_t task, ref, generation, tag;
    uint64_t address, length, sequence;
    int retired;
} PvgGpuRange;
typedef struct {
    PvgGpuRange entries[PVG_GPU_HISTORY_CAP];
    size_t count, next;
    uint64_t sequence, evicted;
} PvgGpuHistory;
static inline int pvg_gpu_contains(uint64_t base, uint64_t len, uint64_t fault) {
    return fault >= base && fault - base < len;
}
static inline void pvg_gpu_retire(PvgGpuHistory *h, uint32_t task, uint32_t ref, int whole_task) {
    for (size_t i = 0; i < h->count; i++) {
        PvgGpuRange *r = &h->entries[i];
        if (!r->retired && r->task == task && (whole_task || r->ref == ref)) {
            r->retired = 1;
            r->sequence = ++h->sequence;
        }
    }
}
static inline void pvg_gpu_export(PvgGpuHistory *h, uint32_t task, uint32_t ref, uint32_t generation,
                                  uint32_t tag, uint64_t address, uint64_t length) {
    for (size_t i = 0; i < h->count; i++) {
        PvgGpuRange *r = &h->entries[i];
        if (!r->retired && r->task == task && r->ref == ref && r->generation == generation &&
            r->tag == tag && r->address == address && r->length == length) return;
    }
    if (h->count == PVG_GPU_HISTORY_CAP) h->evicted++;
    else h->count++;
    h->entries[h->next] = (PvgGpuRange){task, ref, generation, tag, address, length, ++h->sequence, 0};
    h->next = (h->next + 1) % PVG_GPU_HISTORY_CAP;
}
#endif
