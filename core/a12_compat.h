#ifndef HENGEVM_A12_COMPAT_H
#define HENGEVM_A12_COMPAT_H
#include "cpu.h"
int hv_a12_compat_handles(uint32_t insn);
int hv_a12_compat_exec(HvCpu *cpu, uint32_t insn);
#endif
