#include "pmd.h"

#include <stdint.h>
#include <stdio.h>

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

void pmd_fadeout_music(void)
{
    __asm__ __volatile__(
        "movw $0x0208, %%ax\n\t"
        "int $0x60"
        :
        :
        : "ax", "cc", "memory");
}

int pmd_get_music_buffer(uint16_t *seg, uint16_t *ofs)
{
    uint16_t out_seg;
    uint16_t out_ofs;

    __asm__ __volatile__(
        "pushw %%ds\n\t"
        "pushw %%es\n\t"

        "movb $0x06, %%ah\n\t"
        "int $0x60\n\t"

        "movw %%ds, %%bx\n\t"
        "movw %%dx, %%cx\n\t"

        "popw %%es\n\t"
        "popw %%ds\n\t"

        "movw %%bx, %0\n\t"
        "movw %%cx, %1"
        : "=m"(out_seg), "=m"(out_ofs)
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");

    *seg = out_seg;
    *ofs = out_ofs;

    return 1;
}

unsigned long pmd_get_music_buffer_size(void)
{
    uint16_t ax_out;
    unsigned int size_kb;

    __asm__ __volatile__(
        "pushw %%ds\n\t"
        "pushw %%es\n\t"

        "movb $0x22, %%ah\n\t"
        "int $0x60\n\t"

        "movw %%ax, %%bx\n\t"

        "popw %%es\n\t"
        "popw %%ds\n\t"

        "movw %%bx, %0"
        : "=m"(ax_out)
        :
        : "ax", "bx", "cx", "dx", "cc", "memory");

    size_kb = ax_out & 0x00ff;

    return (unsigned long)size_kb * 1024UL;
}

static void pmd_write_far_byte(uint16_t base_seg,
                               uint16_t base_ofs,
                               unsigned long pos,
                               unsigned char value)
{
    unsigned long linear;
    uint16_t seg;
    uint16_t ofs;
    unsigned char __far *dst;

    linear = (unsigned long)base_ofs + pos;
    seg = (uint16_t)(base_seg + (uint16_t)(linear >> 4));
    ofs = (uint16_t)(linear & 0x000f);

    dst = (unsigned char __far *)(((uint32_t)seg << 16) | ofs);
    *dst = value;
}

int pmd_load_music_file(const char *filename)
{
    FILE *fp;
    uint16_t seg;
    uint16_t ofs;
    unsigned long max_size;
    unsigned long loaded;
    size_t read_size;
    size_t i;
    static unsigned char buffer[512];

    if (!pmd_get_music_buffer(&seg, &ofs)) {
        return 0;
    }

    max_size = pmd_get_music_buffer_size();
    if (max_size == 0) {
        return 0;
    }

    fp = fopen(filename, "rb");
    if (fp == 0) {
        return 0;
    }

    loaded = 0;

    while ((read_size = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (loaded + (unsigned long)read_size > max_size) {
            fclose(fp);
            return 0;
        }

        for (i = 0; i < read_size; ++i) {
            pmd_write_far_byte(seg, ofs, loaded, buffer[i]);
            ++loaded;
        }
    }

    fclose(fp);

    if (loaded == 0) {
        return 0;
    }

    return 1;
}
