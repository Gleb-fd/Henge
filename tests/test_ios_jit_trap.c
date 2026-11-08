/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../ios/Henge/Native/henge-jit-trap.h"

#include <assert.h>
#include <stdio.h>

/* To run the native part on Darwin arm64, compile with
 * -DHENGE_TEST_NATIVE_BRK and link ios/Henge/Native/henge-brk.S. */
#if defined(HENGE_TEST_NATIVE_BRK) && defined(__APPLE__) && defined(__aarch64__)
#include <signal.h>
#include <stdlib.h>
#include <sys/ucontext.h>

extern void *henge_brk_get_jit_mapping(void *, size_t);
extern void henge_brk_jit_detach(void);
extern uint64_t henge_brk_probe(void);
extern const char henge_brk_get_jit_mapping_trap;
extern const char henge_brk_jit_detach_trap;
extern const char henge_brk_probe_trap;
static volatile sig_atomic_t seen;
static volatile sig_atomic_t expected;

static void native_handler(int sig, siginfo_t *info, void *context)
{
    (void)sig;
    (void)info;
    ucontext_t *uc = context;
    if (!uc || !uc->uc_mcontext) _Exit(1);
    uintptr_t pc = (uintptr_t)uc->uc_mcontext->__ss.__pc;
    uintptr_t mapping = (uintptr_t)&henge_brk_get_jit_mapping_trap;
    uintptr_t detach = (uintptr_t)&henge_brk_jit_detach_trap;
    uintptr_t probe = (uintptr_t)&henge_brk_probe_trap;
    if (!henge_jit_is_trap_pc(pc, mapping, detach, probe) ||
        pc != (expected == 1 ? mapping : expected == 2 ? detach : probe)) {
        _Exit(1);
    }
    seen++;
    uc->uc_mcontext->__ss.__pc += 4;
    uc->uc_mcontext->__ss.__x[0] = 0;
}

static void test_native(void)
{
    struct sigaction sa = {0}, old_trap, old_bus;
    sa.sa_sigaction = native_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    assert(sigaction(SIGTRAP, &sa, &old_trap) == 0);
    assert(sigaction(SIGBUS, &sa, &old_bus) == 0);
    expected = 1;
    assert(henge_brk_get_jit_mapping(NULL, 4096) == NULL && seen == 1);
    expected = 2;
    henge_brk_jit_detach();
    assert(seen == 2);
    expected = 3;
    assert(henge_brk_probe() == 0 && seen == 3);
    assert(sigaction(SIGTRAP, &old_trap, NULL) == 0);
    assert(sigaction(SIGBUS, &old_bus, NULL) == 0);
}
#endif

int main(void)
{
    const uintptr_t mapping = 0x1000, detach = 0x2000, probe = 0x3000;
    assert(henge_jit_is_trap_pc(mapping, mapping, detach, probe));
    assert(henge_jit_is_trap_pc(detach, mapping, detach, probe));
    assert(henge_jit_is_trap_pc(probe, mapping, detach, probe));
    const uintptr_t wrong[] = {0, 1, 0x0ffc, 0x1001, 0x1004, 0x1ffc,
                               0x2004, 0x2ffc, 0x3004, 0x4000};
    for (size_t i = 0; i < sizeof(wrong) / sizeof(wrong[0]); ++i) {
        assert(!henge_jit_is_trap_pc(wrong[i], mapping, detach, probe));
    }
    assert(!henge_jit_is_trap_pc(0, 0, 0, 0));
#if defined(HENGE_TEST_NATIVE_BRK) && defined(__APPLE__) && defined(__aarch64__)
    test_native();
    puts("Darwin arm64 BRK requests: ok");
#else
    puts("SKIP native BRK requests (requires Darwin arm64)");
#endif
    puts("iOS JIT trap classifier: ok");
    return 0;
}
