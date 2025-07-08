#include "ibm.h"
#include "x86.h"
#include "cpu.h"
#include <pcem/logging.h>

static int log_count = 0;
static int log_limit = 100;

void cpu_log_state(const char *prefix) {
#ifndef RELEASE_BUILD
    if (log_count >= log_limit)
        return;
    log_count++;
    pclog("%s PC=%08X CS=%04X EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X ESP=%08X EBP=%08X EFLAGS=%08X\n",
          prefix, cpu_state.pc, CS, EAX, EBX, ECX, EDX, ESI, EDI, ESP, EBP, cpu_state.eflags);
#endif
}
