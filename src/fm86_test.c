/*
 * PC-9801-86 FM sound test 2
 * ia16-elf-gcc 用
 */

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;

#define FM_ADDR0 0x188
#define FM_DATA0 0x18A

/* 鳴らなければ上をコメントアウトしてこっちを試す */
/*
#define FM_ADDR0 0x288
#define FM_DATA0 0x28A
*/

static void outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );
    return value;
}

static void fm_wait_busy(void)
{
    int i;

    for (i = 0; i < 10000; i++) {
        if ((inb(FM_ADDR0) & 0x80) == 0) {
            break;
        }
    }
}

static void fm_write(uint8_t reg, uint8_t value)
{
    fm_wait_busy();
    outb(FM_ADDR0, reg);
    fm_wait_busy();
    outb(FM_DATA0, value);
    fm_wait_busy();
}

static void delay_long(void)
{
    volatile unsigned long i;
    for (i = 0; i < 600000UL; i++) {
    }
}

int main(void)
{
    int op;

    /* key off */
    fm_write(0x28, 0x00);

    /* ch1 音色設定 */
    for (op = 0; op < 4; op++) {
        uint8_t r = (uint8_t)(op * 4);

        fm_write((uint8_t)(0x30 + r), 0x71); /* DT/MUL */
        fm_write((uint8_t)(0x40 + r), 0x10); /* TL 小さいほど音が大きい */
        fm_write((uint8_t)(0x50 + r), 0x1F); /* AR */
        fm_write((uint8_t)(0x60 + r), 0x05); /* DR */
        fm_write((uint8_t)(0x70 + r), 0x00); /* SR */
        fm_write((uint8_t)(0x80 + r), 0x0F); /* RR */
        fm_write((uint8_t)(0x90 + r), 0x00); /* SSG-EG off */
    }

    /* Algorithm 7 */
    fm_write(0xB0, 0x07);

    /* 重要：ch1を左右出力ON */
    fm_write(0xB4, 0xC0);

    /* 周波数 A4付近 */
    fm_write(0xA4, 0x22);
    fm_write(0xA0, 0x69);

    /* ch1 key on / 全slot */
    fm_write(0x28, 0xF0);

    delay_long();

    /* key off */
    fm_write(0x28, 0x00);

    return 0;
}