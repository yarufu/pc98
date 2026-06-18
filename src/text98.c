#include "text98.h"

#include "graph98.h"

#define PORT_CG_MODE   0x0068
#define PORT_KANJI_LSB 0x00A1
#define PORT_KANJI_MSB 0x00A3
#define PORT_KANJI_ROW 0x00A5

#define CG_MODE_CODE_ACCESS 0x0A
#define CG_MODE_DOT_ACCESS  0x0B

/* 8 ビット値を I/O ポートへ書き込みます。 */
static void io_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * 16x16 ドットのビットマップフォントを描画します。
 *
 * font は 32 バイトです。
 * - 1 行につき 2 バイト使います
 * - 左 8 ビット + 右 8 ビットで 16 ドットになります
 * - 16 行あるので、合計 32 バイトです
 *
 * 例:
 *   font[0]  = 1 行目の左側 8 ドット
 *   font[1]  = 1 行目の右側 8 ドット
 *   font[2]  = 2 行目の左側 8 ドット
 *   font[3]  = 2 行目の右側 8 ドット
 *   ...
 */
void draw_jis_char(int x, int y, const unsigned char *font)
{
    int row;

    /*
     * 16 行ぶんを順番に見ます。
     * 1 行につき 2 バイトあるので、
     * row * 2 でその行の先頭位置を求められます。
     */
    for (row = 0; row < 16; ++row) {
        unsigned char left_byte;
        unsigned char right_byte;
        int bit;

        left_byte = font[row * 2];
        right_byte = font[row * 2 + 1];

        /*
         * 左側 8 ドットを描画します。
         * いちばん左のドットを bit 7、
         * いちばん右のドットを bit 0 として扱います。
         */
        for (bit = 0; bit < 8; ++bit) {
            if (left_byte & (0x80u >> bit)) {
                graph98_pset(x + bit, y + row, 1);
            }
        }

        /*
         * 右側 8 ドットを描画します。
         * x + 8 から x + 15 の位置に出ます。
         */
        for (bit = 0; bit < 8; ++bit) {
            if (right_byte & (0x80u >> bit)) {
                graph98_pset(x + 8 + bit, y + row, 1);
            }
        }
    }
}

void draw_jis_string(int x, int y, const unsigned char **fonts, int count)
{
    int i;

    if (fonts == 0 || count <= 0) {
        return;
    }

    for (i = 0; i < count; ++i) {
        if (fonts[i] != 0) {
            draw_jis_char(x + (i * 16), y, fonts[i]);
        }
    }
}

/*
 * PC-98 の漢字 ROM から、普通の全角文字 1 文字ぶんを読み出します。
 *
 * jis_code には JIS コードを渡します。
 * たとえば「あ」は 0x2422 です。
 *
 * buffer には 32 バイトを書き込みます。
 * 1 行につき 2 バイト、全部で 16 行なので 32 バイトです。
 *
 * 今回は最小構成にするため、
 * 「普通の全角文字」だけを対象にしています。
 * 外字や特殊記号は扱いません。
 *
 * 読み出し方法は BIOS ではなく、
 * PC-98 の CG ウィンドウを直接読む方法です。
 * 先に漢字コードを I/O ポートへ設定すると、
 * A400:0000 の CG ウィンドウに 16x16 のパターンが並びます。
 */
void get_kanji_font(uint16_t jis_code, unsigned char *buffer)
{
    uint8_t jis_hi;
    uint8_t jis_lo;
    jis_hi = (uint8_t)(jis_code >> 8);
    jis_lo = (uint8_t)(jis_code & 0x00FFu);

    /*
     * 漢字 ROM を読み出せるように、
     * いったんドットアクセスモードへ切り替えます。
     */
    io_out8(PORT_CG_MODE, CG_MODE_DOT_ACCESS);

    /*
     * 普通の全角文字は、右半分を指定すると
     * CG ウィンドウに 1 行 2 バイトずつ並びます。
     *
     * A3 には「上位バイト - 0x20」、
     * A1 には下位バイトを入れます。
     */
    io_out8(PORT_KANJI_ROW, 0x00);
    io_out8(PORT_KANJI_MSB, (uint8_t)(jis_hi - 0x20u));
    io_out8(PORT_KANJI_LSB, jis_lo);

    /*
     * CG ウィンドウ A400:0000 から 16 行ぶん読みます。
     * 1 行は 2 バイトなので、16 回読むと合計 32 バイトになります。
     *
     * ここは far ポインタを使わず、
     * ES=A400h を使って直接コピーしています。
     * そうすると、コンパイラ依存の far ポインタ構文を避けられます。
     */
    __asm__ __volatile__(
        "push %%es\n\t"
        "movw $0xA400, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "xor %%si, %%si\n\t"
        "movw %0, %%di\n\t"
        "movw $16, %%cx\n"
        "1:\n\t"
        "movw %%es:(%%si), %%ax\n\t"
        "movw %%ax, (%%di)\n\t"
        "addw $2, %%si\n\t"
        "addw $2, %%di\n\t"
        "loop 1b\n\t"
        "pop %%es"
        :
        : "r"(buffer)
        : "ax", "cx", "si", "di", "cc", "memory");

    /*
     * 読み終わったら元のコードアクセスモードへ戻します。
     * 戻さないままだと、画面表示に影響が出ることがあります。
     */
    io_out8(PORT_CG_MODE, CG_MODE_CODE_ACCESS);
}

// Shift_JIS 1文字 → JIS 1文字
uint16_t sjis_to_jis(uint8_t sjis_hi, uint8_t sjis_lo)
{
    uint8_t row;
    uint8_t cell;

    if (sjis_lo < 0x9F) {
        if (sjis_hi <= 0x9F) {
            row = (uint8_t)((sjis_hi - 0x81) * 2 + 0x21);
        } else {
            row = (uint8_t)((sjis_hi - 0xE0) * 2 + 0x5F);
        }

        if (sjis_lo >= 0x7F) {
            sjis_lo--;
        }
        cell = (uint8_t)(sjis_lo - 0x1F);
    } else {
        if (sjis_hi <= 0x9F) {
            row = (uint8_t)((sjis_hi - 0x81) * 2 + 0x22);
        } else {
            row = (uint8_t)((sjis_hi - 0xE0) * 2 + 0x60);
        }

        cell = (uint8_t)(sjis_lo - 0x7E);
    }

    return (uint16_t)(((uint16_t)row << 8) | cell);
}

static uint16_t ascii_to_fullwidth_jis(uint8_t c)
{
    static const uint16_t ascii_fullwidth_jis[0x5F] = {
        0x2121, 0x212A, 0x7C7E, 0x2174, 0x2170, 0x2173, 0x2175, 0x7C7D,
        0x214A, 0x214B, 0x2176, 0x215C, 0x2124, 0x215D, 0x2125, 0x213F,
        0x2330, 0x2331, 0x2332, 0x2333, 0x2334, 0x2335, 0x2336, 0x2337,
        0x2338, 0x2339, 0x2127, 0x2128, 0x2163, 0x2161, 0x2164, 0x2129,
        0x2177, 0x2341, 0x2342, 0x2343, 0x2344, 0x2345, 0x2346, 0x2347,
        0x2348, 0x2349, 0x234A, 0x234B, 0x234C, 0x234D, 0x234E, 0x234F,
        0x2350, 0x2351, 0x2352, 0x2353, 0x2354, 0x2355, 0x2356, 0x2357,
        0x2358, 0x2359, 0x235A, 0x214E, 0x2140, 0x214F, 0x2130, 0x2132,
        0x212E, 0x2361, 0x2362, 0x2363, 0x2364, 0x2365, 0x2366, 0x2367,
        0x2368, 0x2369, 0x236A, 0x236B, 0x236C, 0x236D, 0x236E, 0x236F,
        0x2370, 0x2371, 0x2372, 0x2373, 0x2374, 0x2375, 0x2376, 0x2377,
        0x2378, 0x2379, 0x237A, 0x2150, 0x2143, 0x2151, 0x2141
    };

    if (c < 0x20 || c > 0x7E) {
        return 0;
    }

    return ascii_fullwidth_jis[c - 0x20];
}

static int convert_sjis_string_to_jis_array_core(const unsigned char *src,
                                                 uint16_t *dst,
                                                 int max_chars,
                                                 int convert_ascii)
{
    int count;

    count = 0;

    while (*src != '\0' && count < max_chars) {
        uint8_t c;

        c = *src;

        /* 半角スペース */
        if (c == 0x20) {
            dst[count++] = 0x2121;
            src++;
            continue;
        }

        if (convert_ascii && c >= 0x21 && c <= 0x7E) {
            dst[count++] = ascii_to_fullwidth_jis(c);
            src++;
            continue;
        }

        /* 全角1文字（Shift_JIS 2バイト） */
        if (((c >= 0x81) && (c <= 0x9F)) ||
            ((c >= 0xE0) && (c <= 0xEF))) {
            uint8_t c2;

            src++;
            if (*src == '\0') {
                break;
            }

            c2 = *src;
            dst[count] = sjis_to_jis(c, c2);
            count++;
            src++;
            continue;
        }

        /*
         * ここに来る1バイト文字は無視する。
         * 本文・名前欄では convert_ascii=1 により ASCII は全角変換済み。
         */
        src++;
    }

    return count;
}

// 文字列全体を Shift_JIS → JIS配列へ変換
int convert_sjis_string_to_jis_array(const unsigned char *src,
                                     uint16_t *dst,
                                     int max_chars)
{
    return convert_sjis_string_to_jis_array_core(src, dst, max_chars, 0);
}

// 本文メッセージ用。半角ASCIIを全角JISへ変換して表示欠けを防ぐ。
int convert_message_text_sjis_to_jis_array(const unsigned char *src,
                                           uint16_t *dst,
                                           int max_chars)
{
    return convert_sjis_string_to_jis_array_core(src, dst, max_chars, 1);
}
