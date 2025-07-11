#include "ibm.h"
#include "x86.h"
#include "cpu.h"
#include <pcem/logging.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_CAPSTONE
#include <capstone/capstone.h>
#endif

static int log_count = 0;
static int log_limit = 1000;
static int log_json = 0;

void cpu_log_set_json(int enable)
{
#ifndef RELEASE_BUILD
    log_json = enable;
#endif
}

void cpu_log_state(const char *prefix)
{
#ifndef RELEASE_BUILD
        if (log_count >= log_limit)
                return;
        log_count++;

        if (log_json) {
                pclog("{\"type\":\"state\",\"prefix\":\"%s\","
                      "\"pc\":%u,\"cs\":%u,\"eax\":%u,\"ebx\":%u,"
                      "\"ecx\":%u,\"edx\":%u,\"esi\":%u,\"edi\":%u,"
                      "\"esp\":%u,\"ebp\":%u,\"eflags\":%u,"
                      "\"cs_base\":%u,\"cs_limit\":%u,\"cs_attr\":%u}\n",
                      prefix,
                      cpu_state.pc,
                      CS,
                      EAX,
                      EBX,
                      ECX,
                      EDX,
                      ESI,
                      EDI,
                      ESP,
                      EBP,
                      cpu_state.eflags,
                      cpu_state.seg_cs.base,
                      cpu_state.seg_cs.limit,
                      cpu_state.seg_cs.access);
        } else {
                pclog("%s PC=%08X CS=%04X EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X ESP=%08X EBP=%08X EFLAGS=%08X CS_BASE=%08X CS_LIMIT=%08X CS_ATTR=%02X\n",
                      prefix,
                      cpu_state.pc,
                      CS,
                      EAX,
                      EBX,
                      ECX,
                      EDX,
                      ESI,
                      EDI,
                      ESP,
                      EBP,
                      cpu_state.eflags,
                      cpu_state.seg_cs.base,
                      cpu_state.seg_cs.limit,
                      cpu_state.seg_cs.access);
        }

#endif
}

void cpu_log_cs_segment(const char *reason)
{
#ifndef RELEASE_BUILD
    if (log_json) {
        pclog("{\"type\":\"cs_segment\",\"reason\":\"%s\",\"base\":%u,\"limit\":%u,\"attr\":%u}\n",
              reason,
              cpu_state.seg_cs.base,
              cpu_state.seg_cs.limit,
              cpu_state.seg_cs.access);
    } else {
        pclog("%s CS_BASE=%08X CS_LIMIT=%08X CS_ATTR=%02X\n",
              reason,
              cpu_state.seg_cs.base,
              cpu_state.seg_cs.limit,
              cpu_state.seg_cs.access);
    }
#endif
}

void cpu_log_gpa_write(uint32_t gpa)
{
#ifndef RELEASE_BUILD
    if (log_json)
        pclog("{\"type\":\"gpa_write\",\"gpa\":%u}\n", gpa);
    else
        pclog("GPA_WRITE=%08X\n", gpa);
#endif
}

static int last_mode = -1;

void cpu_log_cr_change(const char *reg, uint32_t old_val, uint32_t new_val)
{
#ifndef RELEASE_BUILD
    if (old_val != new_val) {
        if (log_json)
            pclog("{\"type\":\"cr_change\",\"reg\":\"%s\",\"old\":%u,\"new\":%u}\n", reg, old_val, new_val);
        else
            pclog("CR_CHANGE %s %08X->%08X\n", reg, old_val, new_val);
    }
#endif
}

void cpu_log_mode_change(void)
{
#ifndef RELEASE_BUILD
    int mode;
    const char *name;

    if (cpu_cur_status & CPU_STATUS_PMODE) {
        if (cpu_cur_status & CPU_STATUS_V86) {
            mode = 2;
            name = "v86";
        } else {
            mode = 1;
            name = "protected";
        }
    } else {
        mode = 0;
        name = "real";
    }

    if (mode != last_mode) {
        if (log_json)
            pclog("{\"type\":\"mode_change\",\"mode\":\"%s\"}\n", name);
        else
            pclog("MODE_CHANGE=%s\n", name);
        last_mode = mode;
    }
#endif
}

void cpu_log_shadow_remap(uint32_t start, uint32_t size)
{
#ifndef RELEASE_BUILD
    if (log_json)
        pclog("{\"type\":\"shadow_remap\",\"start\":%u,\"size\":%u}\n", start, size);
    else
        pclog("SHADOW_REMAP start=%08X size=%08X\n", start, size);
#endif
}

void cpu_log_current_insn(void)
{
#ifndef RELEASE_BUILD
#ifdef HAVE_CAPSTONE
    csh handle;
    cs_mode mode = (cr0 & 1) ? CS_MODE_32 : CS_MODE_16;
    if (cs_open(CS_ARCH_X86, mode, &handle) != CS_ERR_OK)
        return;
    cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);

    uint8_t code[16];
    for (int i = 0; i < 16; i++)
        code[i] = fastreadb(cs + cpu_state.pc + i);

    cs_insn *insn;
    size_t cnt = cs_disasm(handle, code, sizeof(code), cs + cpu_state.pc, 1, &insn);
    if (cnt > 0) {
        if (log_json)
            pclog("{\"type\":\"insn\",\"mnemonic\":\"%s\",\"operands\":\"%s\"}\n", insn[0].mnemonic, insn[0].op_str);
        else
            pclog("INSN %s %s\n", insn[0].mnemonic, insn[0].op_str);
        cs_free(insn, cnt);
    } else {
        if (log_json)
            pclog("{\"type\":\"insn\",\"mnemonic\":\"???\"}\n");
        else
            pclog("INSN ???\n");
    }
    cs_close(&handle);
#endif
#endif
}

void cpu_log_oob_access(const char *op, uint32_t addr)
{
#ifndef RELEASE_BUILD
    if (log_json) {
        pclog("{\"type\":\"oob\",\"op\":\"%s\",\"addr\":%u}\n", op, addr);
        cpu_log_state("oob");
    } else {
        pclog("OOB_%s %08X\n", op, addr);
        cpu_log_state("OOB access");
    }
#endif
}

void cpu_dump_memory(const char *label, const void *data, uint32_t addr, uint32_t size)
{
#ifndef RELEASE_BUILD
    char fname[64];
    snprintf(fname, sizeof(fname), "%s_%08X.bin", label, addr);
    FILE *f = fopen(fname, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
        if (log_json)
            pclog("{\"type\":\"mem_dump\",\"label\":\"%s\",\"addr\":%u,\"size\":%u,\"file\":\"%s\"}\n",
                  label, addr, size, fname);
        else
            pclog("MEM_DUMP %s %08X %u\n", label, addr, size);
    }
#endif
}

void cpu_log_bios_change(uint32_t old_crc, uint32_t new_crc)
{
#ifndef RELEASE_BUILD
    if (old_crc != new_crc) {
        if (log_json)
            pclog("{\"type\":\"bios_diff\",\"old\":%u,\"new\":%u}\n", old_crc, new_crc);
        else
            pclog("BIOS_DIFF %08X->%08X\n", old_crc, new_crc);
    }
#endif
}
