#ifndef HENGEVM_A12_FP_H
#define HENGEVM_A12_FP_H

#include <stdint.h>
#include "cpu.h"

/* handles includes recognized instructions which exec currently rejects.
   exec returns 1 on success, 0 if unrecognized, and -1 if unsupported.
   On -1 the CPU state is unchanged; the caller must not execute natively. */
int hv_a12_fp_handles(uint32_t insn);
int hv_a12_fp_exec(HvCpu *cpu, uint32_t insn);

#endif
