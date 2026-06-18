#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

void debug_log_init(void);
void debug_log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
