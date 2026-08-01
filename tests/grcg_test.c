#include "graph98.h"

#include <stdint.h>
#include <stdio.h>

#define TEST_BYTES_PER_LINE 80u
#define TEST_HUNDREDTHS_PER_DAY 8640000UL
#define TEST_BIOS_CRT_STATUS \
    (*(volatile const uint8_t __far *)0x0000054CUL)

#define TEST_VRAM_BLUE   ((volatile uint8_t __far *)0xA8000000UL)
#define TEST_VRAM_RED    ((volatile uint8_t __far *)0xB0000000UL)
#define TEST_VRAM_GREEN  ((volatile uint8_t __far *)0xB8000000UL)
#define TEST_VRAM_INTENS ((volatile uint8_t __far *)0xE0000000UL)

#define TEST_PORT_DISPLAY_PAGE 0x00A4u
#define TEST_PORT_ACCESS_PAGE  0x00A6u
#define TEST_PAGE_FRONT 0x00u
#define TEST_PAGE_BACK  0x01u

#define TEST_CPU_X0 64
#define TEST_GRCG_X0 320
#define TEST_RECT_WIDTH 128
#define TEST_RECT_HEIGHT 8
#define TEST_RECT_Y0 16
#define TEST_RECT_Y_STEP 16

#define BENCH_PASSES 10u
#define BENCH_COLOR 0u

struct bench_case {
    uint16_t width;
    uint16_t height;
    uint16_t iterations;
};

/* Each case writes 128,000,000 pixels per CPU/GRCG measurement. */
static const struct bench_case bench_cases[] = {
    {  16u,  16u, 50000u },
    {  64u,  64u,  3125u },
    { 320u, 200u,   200u },
    { 640u, 400u,    50u }
};

static void test_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %%al, %%dx"
        :
        : "a"(value), "d"(port)
        : "cc", "memory");
}

static volatile uint8_t __far *test_plane(unsigned int plane)
{
    if (plane == 0u) {
        return TEST_VRAM_BLUE;
    }
    if (plane == 1u) {
        return TEST_VRAM_RED;
    }
    if (plane == 2u) {
        return TEST_VRAM_GREEN;
    }
    return TEST_VRAM_INTENS;
}

static int test_compare_rects(int x0_a, int x0_b, int y0,
                              int width, int height)
{
    unsigned int plane_index;
    int row;
    int byte_index;
    int bytes;

    bytes = width >> 3;
    for (plane_index = 0u; plane_index < 4u; ++plane_index) {
        volatile uint8_t __far *plane;

        plane = test_plane(plane_index);
        for (row = 0; row < height; ++row) {
            uint16_t offset_a;
            uint16_t offset_b;

            offset_a = (uint16_t)((y0 + row) * TEST_BYTES_PER_LINE +
                                  (x0_a >> 3));
            offset_b = (uint16_t)((y0 + row) * TEST_BYTES_PER_LINE +
                                  (x0_b >> 3));
            for (byte_index = 0; byte_index < bytes; ++byte_index) {
                if (plane[(uint16_t)(offset_a + byte_index)] !=
                    plane[(uint16_t)(offset_b + byte_index)]) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

static int test_rect_guard_is_clear(int x0, int y0, int width, int height)
{
    unsigned int plane_index;
    int row;
    int byte_index;
    int byte_x0;
    int byte_x1;

    byte_x0 = x0 >> 3;
    byte_x1 = (x0 + width - 1) >> 3;
    for (plane_index = 0u; plane_index < 4u; ++plane_index) {
        volatile uint8_t __far *plane;

        plane = test_plane(plane_index);
        for (byte_index = byte_x0 - 1; byte_index <= byte_x1 + 1;
             ++byte_index) {
            uint16_t top;
            uint16_t bottom;

            top = (uint16_t)((y0 - 1) * TEST_BYTES_PER_LINE + byte_index);
            bottom = (uint16_t)((y0 + height) * TEST_BYTES_PER_LINE +
                                byte_index);
            if (plane[top] != 0u || plane[bottom] != 0u) {
                return 0;
            }
        }

        for (row = 0; row < height; ++row) {
            uint16_t left;
            uint16_t right;

            left = (uint16_t)((y0 + row) * TEST_BYTES_PER_LINE +
                              byte_x0 - 1);
            right = (uint16_t)((y0 + row) * TEST_BYTES_PER_LINE +
                               byte_x1 + 1);
            if (plane[left] != 0u || plane[right] != 0u) {
                return 0;
            }
        }
    }

    return 1;
}

static int test_compare_all_colors(int use_grcg)
{
    unsigned int color;

    graph98_boxfill(0, 0, GRAPH98_WIDTH - 1, GRAPH98_HEIGHT - 1, 0u);

    for (color = 0u; color < 16u; ++color) {
        int y0;

        y0 = TEST_RECT_Y0 + (int)color * TEST_RECT_Y_STEP;
        graph98_boxfill(TEST_CPU_X0, y0,
                        TEST_CPU_X0 + TEST_RECT_WIDTH - 1,
                        y0 + TEST_RECT_HEIGHT - 1,
                        (unsigned char)color);

        if (use_grcg) {
            if (!graph98_boxfill_grcg_aligned8(
                    TEST_GRCG_X0, y0,
                    TEST_GRCG_X0 + TEST_RECT_WIDTH - 1,
                    y0 + TEST_RECT_HEIGHT - 1,
                    (unsigned char)color)) {
                return 0;
            }
        } else {
            graph98_boxfill_grcg_or_cpu(
                TEST_GRCG_X0, y0,
                TEST_GRCG_X0 + TEST_RECT_WIDTH - 1,
                y0 + TEST_RECT_HEIGHT - 1,
                (unsigned char)color);
        }

        if (!test_compare_rects(TEST_CPU_X0, TEST_GRCG_X0, y0,
                                TEST_RECT_WIDTH, TEST_RECT_HEIGHT)) {
            printf("COLOR %u: MISMATCH\n", color);
            return 0;
        }
        if (!test_rect_guard_is_clear(TEST_GRCG_X0, y0,
                                      TEST_RECT_WIDTH, TEST_RECT_HEIGHT)) {
            printf("COLOR %u: OUTSIDE WRITE\n", color);
            return 0;
        }
    }

    return 1;
}

static int test_unaligned_fallback(void)
{
    int y0;

    y0 = 300;
    if (graph98_boxfill_grcg_aligned8(65, y0, 190, y0 + 7, 6u)) {
        return 0;
    }
    graph98_boxfill(65, y0, 190, y0 + 7, 6u);
    graph98_boxfill_grcg_or_cpu(321, y0, 446, y0 + 7, 6u);
    return test_compare_rects(64, 320, y0, 128, 8);
}

static uint32_t test_dos_hundredths(void)
{
    uint16_t time_cx;
    uint16_t time_dx;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t hundredth;

    __asm__ __volatile__(
        "movb $0x2c, %%ah\n\t"
        "int $0x21"
        : "=c"(time_cx), "=d"(time_dx)
        :
        : "ax", "cc", "memory");

    hour = (uint8_t)(time_cx >> 8);
    minute = (uint8_t)time_cx;
    second = (uint8_t)(time_dx >> 8);
    hundredth = (uint8_t)time_dx;

    return (((uint32_t)hour * 60u + minute) * 60u + second) * 100u +
           hundredth;
}

static uint32_t test_elapsed(uint32_t start, uint32_t end)
{
    if (end >= start) {
        return end - start;
    }
    return TEST_HUNDREDTHS_PER_DAY - start + end;
}

static uint32_t bench_measure_cpu(const struct bench_case *item)
{
    uint16_t pass;
    uint16_t iteration;
    uint32_t start;
    uint32_t end;

    start = test_dos_hundredths();
    for (pass = 0u; pass < BENCH_PASSES; ++pass) {
        for (iteration = 0u; iteration < item->iterations; ++iteration) {
            graph98_boxfill(0, 0, item->width - 1u, item->height - 1u,
                            BENCH_COLOR);
        }
    }
    end = test_dos_hundredths();
    return test_elapsed(start, end);
}

static uint32_t bench_measure_grcg(const struct bench_case *item)
{
    uint16_t pass;
    uint16_t iteration;
    uint32_t start;
    uint32_t end;

    start = test_dos_hundredths();
    for (pass = 0u; pass < BENCH_PASSES; ++pass) {
        for (iteration = 0u; iteration < item->iterations; ++iteration) {
            (void)graph98_boxfill_grcg_aligned8(
                0, 0, item->width - 1u, item->height - 1u, BENCH_COLOR);
        }
    }
    end = test_dos_hundredths();
    return test_elapsed(start, end);
}

static void bench_print_result(const struct bench_case *item,
                               uint32_t cpu_time, uint32_t grcg_time)
{
    uint32_t speedup_x100;
    uint32_t total_loops;

    total_loops = (uint32_t)item->iterations * BENCH_PASSES;
    printf("%ux%u (%lu loops): CPU %lu ms, GRCG %lu ms",
           (unsigned)item->width, (unsigned)item->height,
           (unsigned long)total_loops,
           (unsigned long)(cpu_time * 10u),
           (unsigned long)(grcg_time * 10u));

    if (grcg_time == 0u) {
        printf(", ratio unavailable\n");
        return;
    }

    speedup_x100 = (uint32_t)((cpu_time * 100u) / grcg_time);
    printf(", CPU/GRCG %lu.%02lux\n",
           (unsigned long)(speedup_x100 / 100u),
           (unsigned long)(speedup_x100 % 100u));
}

static void run_benchmark(void)
{
    unsigned int i;

    printf("BENCH: 128000000 pixels per measurement\n");
    fflush(stdout);
    for (i = 0u; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i) {
        uint32_t cpu_time;
        uint32_t grcg_time;

        cpu_time = bench_measure_cpu(&bench_cases[i]);
        grcg_time = bench_measure_grcg(&bench_cases[i]);
        bench_print_result(&bench_cases[i], cpu_time, grcg_time);
        fflush(stdout);
    }
}

static int benchmark_option(const char *arg)
{
    return (arg[0] == 'B' && arg[1] == '\0') ||
           (arg[0] == '/' && arg[1] == 'B' && arg[2] == '\0');
}

static void print_usage(void)
{
    printf("Usage: GRCTEST [B|/B]\n");
    printf("  no argument: compare only\n");
    printf("  B or /B: compare and benchmark\n");
}

int main(int argc, char **argv)
{
    uint8_t bios_status;
    int grcg_available;
    int compare_ok;
    int fallback_ok;
    int benchmark_requested;

    if (argc == 1) {
        benchmark_requested = 0;
    } else if (argc == 2 && benchmark_option(argv[1])) {
        benchmark_requested = 1;
    } else {
        print_usage();
        fflush(stdout);
        return 2;
    }

    bios_status = TEST_BIOS_CRT_STATUS;
    grcg_available = graph98_grcg_available();
    printf("GRCTEST.EXE - GRCG TDW aligned rectangle test\n");
    printf("BIOS 0000:054C = %02Xh, bit 1 = %s\n",
           (unsigned)bios_status, grcg_available ? "ON" : "OFF");

    graph98_init();
    test_out8(TEST_PORT_DISPLAY_PAGE, TEST_PAGE_FRONT);
    test_out8(TEST_PORT_ACCESS_PAGE, TEST_PAGE_BACK);

    compare_ok = test_compare_all_colors(grcg_available);
    fallback_ok = test_unaligned_fallback();

    printf("COLORS 0-15 CPU/%s: %s\n",
           grcg_available ? "GRCG" : "CPU FALLBACK",
           compare_ok ? "MATCH" : "FAILED");
    printf("UNALIGNED CPU FALLBACK: %s\n",
           fallback_ok ? "MATCH" : "FAILED");
    fflush(stdout);

    if (!grcg_available) {
        printf("GRCG PORT ACCESS: SKIPPED\n");
        fflush(stdout);
    }

    if (!benchmark_requested) {
        printf("BENCH: NOT REQUESTED (use B or /B)\n");
        fflush(stdout);
    } else if (grcg_available && compare_ok) {
        run_benchmark();
    } else if (!grcg_available) {
        printf("BENCH: SKIPPED (GRCG unavailable)\n");
        fflush(stdout);
    } else {
        printf("BENCH: SKIPPED (comparison failed)\n");
        fflush(stdout);
    }

    graph98_restore_default_pages();
    return (compare_ok && fallback_ok) ? 0 : 1;
}
