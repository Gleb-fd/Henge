/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HENGE_JIT_TRAP_H
#define HENGE_JIT_TRAP_H

#include <stdbool.h>
#include <stdint.h>

static inline bool henge_jit_is_trap_pc(uintptr_t pc, uintptr_t mapping,
                                        uintptr_t detach, uintptr_t probe)
{
    return pc != 0 && (pc == mapping || pc == detach || pc == probe);
}

#endif
