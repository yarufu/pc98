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
