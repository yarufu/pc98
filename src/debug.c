#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

static void debug_write(FILE *fp, const char *text, unsigned int length)
{
    if (length != 0) {
        fwrite(text, 1, length, fp);
    }
}

static void debug_write_string(FILE *fp, const char *text)
{
    const char *end;

    if (text == 0) {
        text = "(null)";
    }

    end = text;
    while (*end != '\0') {
        ++end;
    }
    debug_write(fp, text, (unsigned int)(end - text));
}

static char __attribute__((noinline, optimize("Os")))
debug_decimal_digit(unsigned long *value, unsigned long place)
{
    char digit;

    digit = '0';
    while (*value >= place) {
        *value -= place;
        ++digit;
    }
    return digit;
}

static void debug_write_unsigned_decimal(FILE *fp, unsigned long value)
{
    char digits[10];
    int first;

    digits[0] = debug_decimal_digit(&value, 1000000000UL);
    digits[1] = debug_decimal_digit(&value, 100000000UL);
    digits[2] = debug_decimal_digit(&value, 10000000UL);
    digits[3] = debug_decimal_digit(&value, 1000000UL);
    digits[4] = debug_decimal_digit(&value, 100000UL);
    digits[5] = debug_decimal_digit(&value, 10000UL);
    digits[6] = debug_decimal_digit(&value, 1000UL);
    digits[7] = debug_decimal_digit(&value, 100UL);
    digits[8] = debug_decimal_digit(&value, 10UL);
    digits[9] = (char)('0' + (int)value);

    first = 0;
    while (first < 9 && digits[first] == '0') {
        ++first;
    }
    debug_write(fp, digits + first, (unsigned int)(10 - first));
}

static void debug_write_signed_decimal(FILE *fp, int value)
{
    unsigned int magnitude;

    if (value < 0) {
        debug_write(fp, "-", 1);
        magnitude = 0U - (unsigned int)value;
    } else {
        magnitude = (unsigned int)value;
    }
    debug_write_unsigned_decimal(fp, (unsigned long)magnitude);
}

static void debug_write_hex(FILE *fp, unsigned int value,
                            int width, int zero_pad)
{
    char digits[4];
    int first;
    int length;
    unsigned int digit;
    char padding;

    first = 4;
    do {
        digit = value & 0x0fU;
        digits[--first] = (char)(digit < 10U ?
                                 '0' + digit : 'A' + digit - 10U);
        value >>= 4;
    } while (value != 0 && first != 0);

    length = 4 - first;
    padding = zero_pad ? '0' : ' ';
    while (width > length) {
        debug_write(fp, &padding, 1);
        --width;
    }
    debug_write(fp, digits + first, (unsigned int)length);
}

/*
 * Supported debug formats:
 *   %s, %d, %u, %X, %lu, %02X, %04X, and %%
 * Current call sites use only %s, %d, %lu, and %04X.
 * Unsupported conversions are emitted literally with the remaining format.
 */
static void debug_write_format(FILE *fp, const char *format, va_list *args)
{
    const char *literal;

    while (*format != '\0') {
        int long_value;
        int width;
        int zero_pad;
        char conversion;

        literal = format;
        while (*format != '\0' && *format != '%') {
            ++format;
        }
        debug_write(fp, literal, (unsigned int)(format - literal));
        if (*format == '\0') {
            return;
        }

        literal = format++;
        zero_pad = 0;
        width = 0;
        if (*format == '0') {
            zero_pad = 1;
            ++format;
        }
        while (*format >= '0' && *format <= '9') {
            if (width <= 4) {
                width = width * 10 + (*format - '0');
            } else {
                width = 5;
            }
            ++format;
        }
        long_value = 0;
        if (*format == 'l') {
            long_value = 1;
            ++format;
        }
        conversion = *format;
        if (conversion != '\0') {
            ++format;
        }

        if (conversion == '%' && !long_value && width == 0) {
            debug_write(fp, "%", 1);
        } else if (conversion == 's' && !long_value && width == 0) {
            debug_write_string(fp, va_arg(*args, const char *));
        } else if (conversion == 'd' && !long_value && width == 0) {
            debug_write_signed_decimal(fp, va_arg(*args, int));
        } else if (conversion == 'u' && !long_value && width == 0) {
            debug_write_unsigned_decimal(
                fp, (unsigned long)va_arg(*args, unsigned int));
        } else if (conversion == 'u' && long_value && width == 0) {
            debug_write_unsigned_decimal(
                fp, va_arg(*args, unsigned long));
        } else if (conversion == 'X' && !long_value &&
                   (width == 0 ||
                    (zero_pad && (width == 2 || width == 4)))) {
            debug_write_hex(fp, va_arg(*args, unsigned int),
                            width, zero_pad);
        } else {
            debug_write_string(fp, literal);
            return;
        }
    }
}

void debug_log_init(void)
{
    FILE *fp;

    fp = fopen("debug.txt", "w");
    if (fp != 0) {
        fclose(fp);
    }
}

/*
 * Supported formats:
 * %s, %d, %u, %X, %lu, %02X, %04X, %%
 */
void debug_log(const char *fmt, ...)
{
    FILE *fp;
    va_list args;

    fp = fopen("debug.txt", "a");

    if (fp == 0) {
        return;
    }

    va_start(args, fmt);

    debug_write_format(fp, fmt, &args);
    debug_write(fp, "\n", 1);

    va_end(args);

    fclose(fp);
}
