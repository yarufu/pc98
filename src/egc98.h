#ifndef EGC98_H
#define EGC98_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EGC98_DRAW_SKIPPED = 0,
    EGC98_DRAW_OK = 1
};

struct egc98_bios_flags {
    uint8_t prxdupd_054d;
    uint8_t crt_bios_0597;
};

/*
 * These BIOS work-area bits are a prerequisite, not a guarantee that every
 * EGC operation is compatible with the current machine.
 */
void egc98_read_bios_flags(struct egc98_bios_flags *flags);
int egc98_bios_flag_present(void);

/*
 * Draws an aligned solid rectangle and restores normal VRAM access before
 * returning. x and width must both be multiples of 16.
 *
 * The normal entry point explicitly reloads the shifter address and bit
 * length for every line. The automatic-restart entry point exists only for
 * comparing the documented EGC shifter restart behavior in EGCTEST.EXE.
 */
int egc98_boxfill_aligned16(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint8_t color);
int egc98_boxfill_aligned16_auto_restart_test(uint16_t x, uint16_t y,
                                             uint16_t width, uint16_t height,
                                             uint8_t color);

#ifdef __cplusplus
}
#endif

#endif
