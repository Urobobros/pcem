#ifndef __PCEM_LOGGING_H__
#define __PCEM_LOGGING_H__

#define printf pclog
extern void pclog(const char *format, ...);
extern void error(const char *format, ...);
extern void fatal(const char *format, ...);
extern void warning(const char *format, ...);
// Opens the log file and prints its location to the console. Always safe to call.
extern uint8_t pclog_start(void);

#endif /* __PCEM_LOGGING_H__ */