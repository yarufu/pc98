#include "fm86.h"

#include <stdint.h>

#define FM86_ADDR0 0x188
#define FM86_DATA0 0x18A
#define GDC_MASTER_STATUS 0x0060
#define GDC_STATUS_VSYNC 0x20

static int g_se86_initialized = 0;

static uint8_t io_in8(uint16_t port)
{
    uint8_t value;

    __asm__ __volatile__(
        "inb %%dx, %%al"
        : "=a"(value)
        : "d"(port)
        : "cc");

    return value;
}

static void io_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %%al, %%dx"
        :
        : "a"(value), "d"(port)
        : "cc");
}

static void fm86_wait_busy(void)
{
    uint16_t i;

    for (i = 0; i < 10000u; ++i) {
        if ((io_in8(FM86_ADDR0) & 0x80u) == 0) {
            break;
        }
    }
}

static void fm86_write(uint8_t reg, uint8_t value)
{
    fm86_wait_busy();
    io_out8(FM86_ADDR0, reg);
    fm86_wait_busy();
    io_out8(FM86_DATA0, value);
    fm86_wait_busy();
}

/* 怪しい
static void wait_vsync(void)
{
    while ((io_in8(GDC_MASTER_STATUS) & GDC_STATUS_VSYNC) != 0u) {
    }

    while ((io_in8(GDC_MASTER_STATUS) & GDC_STATUS_VSYNC) == 0u) {
    }
}
*/


static void wait_vsync(void)
{
    uint16_t guard;

    guard = 0;
    while ((io_in8(GDC_MASTER_STATUS) & GDC_STATUS_VSYNC) != 0u) {
        if (++guard == 0u) {
            return;
        }
    }

    guard = 0;
    while ((io_in8(GDC_MASTER_STATUS) & GDC_STATUS_VSYNC) == 0u) {
        if (++guard == 0u) {
            return;
        }
    }
}



static void wait_frames(uint8_t frames)
{
    uint8_t i;

    for (i = 0; i < frames; ++i) {
        wait_vsync();
    }
}

void se86_init(void)
{
    int op;

    if (g_se86_initialized) {
        return;
    }

    fm86_write(0x28, 0x00);

    for (op = 0; op < 4; ++op) {
        uint8_t r;

        r = (uint8_t)(op * 4);

        fm86_write((uint8_t)(0x30 + r), 0x71);
        fm86_write((uint8_t)(0x40 + r), 0x18);
        fm86_write((uint8_t)(0x50 + r), 0x1F);
        fm86_write((uint8_t)(0x60 + r), 0x05);
        fm86_write((uint8_t)(0x70 + r), 0x00);
        fm86_write((uint8_t)(0x80 + r), 0x0F);
        fm86_write((uint8_t)(0x90 + r), 0x00);
    }

    fm86_write(0xB0, 0x07);
    fm86_write(0xB4, 0xC0);
    fm86_write(0xA4, 0x22);
    fm86_write(0xA0, 0x69);

    g_se86_initialized = 1;
}

void se86_play_beep(void)
{
    if (!g_se86_initialized) {
        se86_init();
    }

    fm86_write(0x28, 0x00);
    fm86_write(0x28, 0xF0);
    wait_frames(4);
    fm86_write(0x28, 0x00);
}
