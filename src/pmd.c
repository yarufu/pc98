#include "pmd.h"

#include <stdint.h>

#define PMD_VECTOR_NUMBER 0x60u
#define PMD_SIGNATURE_OFFSET 2u

int pmd_is_resident(void)
{
    const uint16_t __far * const ivt =
        (const uint16_t __far *)0x00000000UL;
    const uint16_t vector_index = (uint16_t)(PMD_VECTOR_NUMBER * 2u);
    const uint16_t offset = ivt[vector_index];
    const uint16_t segment = ivt[(uint16_t)(vector_index + 1u)];
    const uint32_t signature_addr =
        ((uint32_t)segment << 16) | (uint32_t)(uint16_t)(offset + PMD_SIGNATURE_OFFSET);
    const char __far * const signature =
        (const char __far *)signature_addr;

    if (signature[0] != 'P') {
        return 0;
    }
    if (signature[1] != 'M') {
        return 0;
    }
    if (signature[2] != 'D') {
        return 0;
    }

    return 1;
}

void pmd_start_music(void)
{
    __asm__ __volatile__(
        "movb $0x00, %%ah\n\t"
        "int $0x60"
        :
        :
        : "ax", "cc", "memory");
}

void pmd_stop_music(void)
{
    __asm__ __volatile__(
        "movb $0x01, %%ah\n\t"
        "int $0x60"
        :
        :
        : "ax", "cc", "memory");
}
