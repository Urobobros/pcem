#ifndef PCEM_CPU_DEBUG_H
#define PCEM_CPU_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Logs the current CPU state for debugging. Besides the standard register set
 * (EAX, EBX, ECX, EDX, ESI, EDI, ESP, EBP, EFLAGS), the log also includes the
 * current CS segment base, limit and access attributes so that both WHPX and
 * the dynamic recompiler can produce identical dumps.
 */
void cpu_log_state(const char *prefix);

/*
 * Logs details of the CS segment when it changes. The provided reason string
 * is printed as a prefix so WHPX and the dynamic recompiler can match when the
 * segment is updated.
 */
void cpu_log_cs_segment(const char *reason);

/* Logs the guest physical address of a write operation so WHPX and the
 * dynamic recompiler can produce identical memory traces. */
void cpu_log_gpa_write(uint32_t gpa);

/* Logs the mnemonic of the instruction currently executing. */
void cpu_log_current_insn(void);

/* Logs when a control register changes. */
void cpu_log_cr_change(const char *reg, uint32_t old_val, uint32_t new_val);

/* Records CPU mode transitions (real/protected/v86). */
void cpu_log_mode_change(void);

/* Logs when the top of RAM is remapped (Shadow RAM). */
void cpu_log_shadow_remap(uint32_t start, uint32_t size);

/* Logs an access outside mapped memory regions. */
void cpu_log_oob_access(const char *op, uint32_t addr);

/* Dump a memory region to a binary file for comparison. */
void cpu_dump_memory(const char *label, const void *data, uint32_t addr, uint32_t size);

/* Report when BIOS contents change. */
void cpu_log_bios_change(uint32_t old_crc, uint32_t new_crc);

/* Enable or disable JSON formatted log output. */
void cpu_log_set_json(int enable);

#ifdef __cplusplus
}
#endif

#endif /* PCEM_CPU_DEBUG_H */
