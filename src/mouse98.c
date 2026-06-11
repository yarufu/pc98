#include "mouse98.h"

#include <stdint.h>

int mouse98_init(void)
{
    uint16_t ax;

    __asm__ __volatile__(
        "xorw %%ax, %%ax\n\t"
        "int $0x33\n\t"
        "movw %%ax, %0"
        : "=m"(ax)
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");

    return ax != 0;
}

int mouse98_left_pressed(void)
{
    uint16_t bx;

    __asm__ __volatile__(
        "movw $0x0003, %%ax\n\t"
        "int $0x33\n\t"
        "movw %%bx, %0"
        : "=m"(bx)
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");

    return (bx & 0x0001) != 0;
}

void mouse98_wait_left_release(void)
{
    while (mouse98_left_pressed()) {
    }
}

int mouse98_right_pressed(void)
{
    uint16_t bx;

    __asm__ __volatile__(
        "movw $0x0003, %%ax\n\t"
        "int $0x33\n\t"
        "movw %%bx, %0"
        : "=m"(bx)
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");

    return (bx & 0x0002) != 0;
}

void mouse98_wait_right_release(void)
{
    while (mouse98_right_pressed()) {
    }
}

void mouse98_get_motion(int *dx, int *dy)
{
    uint16_t cx;
    uint16_t dx_reg;

    __asm__ __volatile__(
        "movw $0x000B, %%ax\n\t"
        "int $0x33\n\t"
        "movw %%cx, %0\n\t"
        "movw %%dx, %1"
        : "=m"(cx), "=m"(dx_reg)
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");

    if (dx != 0) {
        *dx = (int)(int16_t)cx;
    }
    if (dy != 0) {
        *dy = (int)(int16_t)dx_reg;
    }
}

void mouse98_hide_cursor(void)
{
    __asm__ __volatile__(
        "movw $0x0002, %%ax\n\t"
        "int $0x33"
        :
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");
}
