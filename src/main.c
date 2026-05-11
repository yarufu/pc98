#include "graph98.h"
#include "fm86.h"
#include "pmd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PORT_CG_MODE   0x0068
#define PORT_KANJI_LSB 0x00A1
#define PORT_KANJI_MSB 0x00A3
#define PORT_KANJI_ROW 0x00A5

#define CG_MODE_CODE_ACCESS 0x0A
#define CG_MODE_DOT_ACCESS  0x0B

// キャラの位置調整
#define STAND_LEFT_X   60
#define STAND_RIGHT_X  320
#define STAND_Y        9

#define STAND_W        256
#define STAND_H        290


struct Message;

enum BackgroundId {
    BG_NONE = 0,
    BG_01,
    BG_02,
    BG_03
};

enum StandId {
    STAND_NONE = 0,
    STAND_CHARACTER01,
    STAND_CHARACTER02,
    STAND_CHARACTER03
};

enum FaceId {
    FACE_NORMAL = 0,
    FACE_HAPPY,
    FACE_ANGRY,
    FACE_SURPRISED
};

typedef struct {
    char name[32];
    int value;
} GameFlag;

#define MAX_FLAGS 16

static GameFlag g_flags[MAX_FLAGS];



/* 関数宣言部 */
static void input_wait_key(void);
static void ui_draw_wait_mark(int x, int y, unsigned char color);
static void text98_hide_cursor(void);
static void text98_clear_screen(void);
static int input_wait_choice_cursor(const struct Message *msg,
                                    enum BackgroundId current_bg);
static void io_out8(uint16_t port, uint8_t value);
static void get_kanji_font(uint16_t jis_code, unsigned char *buffer);
static void ui_draw_background(enum BackgroundId bg_id);
static void ui_draw_message_window(void);
static void ui_draw_background_test(void);
static const char *ui_get_stand_sprite_path(enum StandId stand_id,
                                            enum FaceId face_id,
                                            int facing_left);
static void ui_draw_stand_placeholder(int x, int y);
static void ui_draw_stand(enum StandId stand_id, enum FaceId face_id,
                          int x, int y, int facing_left);
static void ui_draw_stands_for_message(const struct Message *msg);
static void draw_string_kanji(int x, int y, const unsigned char **fonts, int count);
static void ui_draw_cursor_triangle(int x, int y, unsigned char color);
static int ui_draw_message_page_jis(const uint16_t *name, int name_len,
                                    const uint16_t *jis_codes, int count,
                                    int start_index);
static void ui_draw_message_jis(const uint16_t *name, int name_len,
                                const uint16_t *jis_codes, int count);
static enum BackgroundId parse_bg_id(const char *name);
static const char *get_bg_path(enum BackgroundId bg_id);
static enum StandId parse_stand_id(const char *name);
static enum FaceId parse_face_id(const char *name);
static void process_command_line(const char *line,
                                 enum BackgroundId *bg_id,
                                 enum StandId *left_stand,
                                 enum FaceId *left_face,
                                 enum StandId *right_stand,
                                 enum FaceId *right_face);
static void ui_draw_current_stands(enum StandId left_stand,
                                   enum FaceId left_face,
                                   enum StandId right_stand,
                                   enum FaceId right_face);
static int input_wait_choice_jis(const uint16_t *choice1, int choice1_len,
                                 const uint16_t *choice2, int choice2_len);
static void handle_choice_block(FILE *fp, const char *label1, const char *label2);
static int find_label_and_jump(FILE *fp, const char *label_name);
static void trim_leading_spaces(char *str);
static int find_flag_index(const char *name);
static void set_flag_on(const char *name);
static void set_flag_off(const char *name);
static int is_flag_on(const char *name);
static void ui_restore_stand_background_rect(int x0, int y0, int x1, int y1, int bg_id);
static void ui_refresh_left_stand_only(int bg_id,
                                       enum StandId left_stand,
                                       enum FaceId left_face);
static void ui_refresh_right_stand_only(int bg_id,
                                        enum StandId right_stand,
                                        enum FaceId right_face);


static void ui_draw_wait_mark(int x, int y, unsigned char color)
{
    graph98_hline(x + 0, x + 8, y + 0, color);
    graph98_hline(x + 1, x + 7, y + 1, color);
    graph98_hline(x + 2, x + 6, y + 2, color);
    graph98_hline(x + 3, x + 5, y + 3, color);
    graph98_hline(x + 4, x + 4, y + 4, color);
}

static void ui_draw_choice_jis(const uint16_t *choice1, int choice1_len,
                               const uint16_t *choice2, int choice2_len,
                               int selected);

struct Message {
    const uint16_t *name;
    int name_len;

    const uint16_t *text;
    int text_len;

    enum StandId left_stand;
    enum StandId right_stand;
    enum FaceId left_face;
    enum FaceId right_face;

    int has_choice;

    const uint16_t *choice1;
    int choice1_len;

    const uint16_t *choice2;
    int choice2_len;

    int next;
    int next1;
    int next2;
};

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
void draw_kanji_16(int x, int y, const unsigned char *font)
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

static void draw_string_kanji(int x, int y, const unsigned char **fonts, int count)
{
    int i;

    if (fonts == 0 || count <= 0) {
        return;
    }

    for (i = 0; i < count; ++i) {
        if (fonts[i] != 0) {
            draw_kanji_16(x + (i * 16), y, fonts[i]);
        }
    }
}

/*
 * 小さい右向き三角カーソルを描きます。
 *
 * 文字ではなく、横線を少しずつ長くして
 * 右向きの三角にしています。
 */
static void ui_draw_cursor_triangle(int x, int y, unsigned char color)
{
    graph98_hline(x,     x,     y + 0, color);
    graph98_hline(x,     x + 1, y + 1, color);
    graph98_hline(x,     x + 2, y + 2, color);
    graph98_hline(x,     x + 3, y + 3, color);
    graph98_hline(x,     x + 4, y + 4, color);
    graph98_hline(x,     x + 3, y + 5, color);
    graph98_hline(x,     x + 2, y + 6, color);
    graph98_hline(x,     x + 1, y + 7, color);
    graph98_hline(x,     x,     y + 8, color);
}

static void ui_draw_message_window(void)
{
    /* 外枠 */
    // graph98_boxfill(88, 300, 551, 389, 1);
    // graph98_rect(88, 300, 551, 389, 15);

    /* 内側（26文字幅 + 左右空白1文字ぶんを想定） */
    // graph98_boxfill(96, 313, 543, 386, 0);
    // graph98_boxfill(92, 313, 544, 392, 0); 26文字メッセージ枠
    // graph98_boxfill(92, 313, 516, 392, 0);
    graph98_boxfill(109, 313, 531, 392, 0);
    // graph98_rect(96, 308, 543, 381, 8);
}

static void ui_draw_background(enum BackgroundId bg_id)
{
    const char *path;

    path = get_bg_path(bg_id);

    if (path != 0) {
        if (graph98_load_g98(path)) {
            return;
        }
    }

    ui_draw_background_test();
}

/*
 * 画像なしで「体育館っぽい背景」を描くテスト版です。
 *
 * 方針:
 * - 上部 0～299 をメイン背景
 * - その中を「壁」と「床」に分割
 * - 壁に柱っぽい縦ライン
 * - 床に境界線と板ライン
 * - 下部 300～399 は既存UI用の紫エリアを維持
 * - 左右の装飾柱も維持
 *
 * 既存のメッセージウィンドウ/選択肢UIと重ならないよう、
 * UI領域そのものの構造は崩していません。
 */
static void ui_draw_background_test(void)
{
    int x;
    int y;

    /* =========================================================
     * まず全体の土台
     * ========================================================= */

    /* 上部メイン表示領域の下地 */
    graph98_boxfill(0,   0, 639, 299, 8);

    /* 下部UIエリア（今まで通り） */
    graph98_boxfill(0, 300, 639, 399, 5);

    /* 左右の装飾柱（今まで通り） */
    graph98_boxfill(0,   0,  63, 399, 5);
    graph98_boxfill(576, 0, 639, 399, 5);

    /* =========================================================
     * 上部メイン表示領域：体育館っぽい壁
     * ========================================================= */

    /*
     * 壁を上・中・下で少しだけ色分けして、
     * 単色ベタ塗り感を減らします。
     */
    graph98_boxfill(64,  0, 575,  55, 7);   /* 上の明るい帯 */
    graph98_boxfill(64, 56, 575, 165, 8);   /* 壁の本体 */
    graph98_boxfill(64,166, 575, 199, 7);   /* 床との間の帯 */

    /* 壁の最上部ライン */
    graph98_hline(64, 575,  12, 15);
    graph98_hline(64, 575,  13, 7);

    /*
     * 柱っぽい縦帯。
     * 太い縦帯 + 中央の明るい線で、
     * のっぺり感を減らします。
     */
    graph98_boxfill(110,  24, 126, 199, 1);
    graph98_boxfill(236,  24, 252, 199, 1);
    graph98_boxfill(388,  24, 404, 199, 1);
    graph98_boxfill(514,  24, 530, 199, 1);

    graph98_vline(118, 24, 199, 9);
    graph98_vline(244, 24, 199, 9);
    graph98_vline(396, 24, 199, 9);
    graph98_vline(522, 24, 199, 9);

    /* 柱の左右の影ライン */
    graph98_vline(110, 24, 199, 0);
    graph98_vline(126, 24, 199, 15);
    graph98_vline(236, 24, 199, 0);
    graph98_vline(252, 24, 199, 15);
    graph98_vline(388, 24, 199, 0);
    graph98_vline(404, 24, 199, 15);
    graph98_vline(514, 24, 199, 0);
    graph98_vline(530, 24, 199, 15);

    /*
     * 上の壁に横ラインを少し入れて、
     * 体育館の内壁パネルっぽさを出します。
     */
    graph98_hline(64, 575,  55, 15);
    graph98_hline(64, 575,  56, 7);
    graph98_hline(64, 575, 110, 7);
    graph98_hline(64, 575, 111, 0);
    graph98_hline(64, 575, 165, 15);
    graph98_hline(64, 575, 166, 7);

    /* =========================================================
     * 床
     * ========================================================= */

    /*
     * 床領域。
     * 200～299 を床にして、壁と分離します。
     */
    graph98_boxfill(64, 200, 575, 299, 6);

    /*
     * 壁と床の境界線。
     * ここがあるだけでかなり「部屋」感が出ます。
     */
    graph98_hline(64, 575, 199, 15);
    graph98_hline(64, 575, 200, 0);

    /*
     * 床の板ライン。
     * 等間隔の横線で、木床っぽい雰囲気を出します。
     */
    for (y = 214; y <= 294; y += 14) {
        graph98_hline(64, 575, y, 14);
    }

    /*
     * 床の縦の継ぎ目。
     * まっすぐ入れるだけでも、単色感がだいぶ減ります。
     */
    for (x = 96; x <= 544; x += 48) {
        graph98_vline(x, 200, 299, 14);
    }

    /*
     * センター付近だけ明るめの長方形を入れて、
     * 「床の反射」「照明が当たっている感じ」を簡易表現。
     */
    graph98_boxfill(220, 214, 420, 284, 14);
    graph98_rect(220, 214, 420, 284, 6);

    /*
     * 反射帯を数本だけ入れる。
     * ベタっとしすぎないように間引いています。
     */
    graph98_hline(236, 404, 228, 15);
    graph98_hline(232, 408, 246, 15);
    graph98_hline(228, 412, 264, 15);

    /*
     * 体育館のコートっぽい線を簡易で追加。
     * あくまで雰囲気なので最小構成です。
     */
    graph98_rect(176, 224, 464, 288, 15);
    graph98_vline(320, 224, 288, 15);
    graph98_hline(272, 368, 256, 15);

    /* =========================================================
     * 既存UIとの境界
     * ========================================================= */

    /*
     * メイン背景と下部UIの区切り。
     * 今までの区切り線を少し強めにしています。
     */
    graph98_hline(0, 639, 298, 8);
    graph98_hline(0, 639, 299, 15);
    graph98_hline(0, 639, 300, 1);

    /* 左右の紫柱にも縁線を入れて少し締める */
    graph98_vline(63,  0, 399, 15);
    graph98_vline(64,  0, 399, 1);
    graph98_vline(575, 0, 399, 1);
    graph98_vline(576, 0, 399, 15);
}

static void ui_restore_stand_background_rect(int x0, int y0, int x1, int y1, int bg_id)
{
    /*
     * 最小版:
     * いまは gym/test ともに「背景全体を描く関数」しかないので、
     * 背景画像の部分切り出しはまだしません。
     *
     * まずはテスト背景と同じ見た目になるよう、
     * 立ち絵の後ろに来る範囲だけを描き直します。
     *
     * BG_GYM_IMAGE でも、ひとまずテスト背景の該当部分を描く最小版で進めます。
     * もし bg001.g98 を使った本物背景で違和感が出たら、
     * 次の段階で「背景バッファ保持」に進みます。
     */

    (void)bg_id;

    /* 上部の背景領域だけ描き直す */
    if (x0 < 64)  x0 = 64;
    if (x1 > 575) x1 = 575;
    if (y0 < 0)   y0 = 0;
    if (y1 > 299) y1 = 299;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    /*
     * 背景テスト版のうち、立ち絵の背後に効く部分だけ再描画
     */
    /* 壁ベース */
    if (y0 <= 55) {
        int yy1 = y1 < 55 ? y1 : 55;
        if (yy1 >= y0) {
            graph98_boxfill(x0, y0, x1, yy1, 7);
        }
    }
    if (y1 >= 56 && y0 <= 165) {
        int yy0 = y0 > 56 ? y0 : 56;
        int yy1 = y1 < 165 ? y1 : 165;
        if (yy1 >= yy0) {
            graph98_boxfill(x0, yy0, x1, yy1, 8);
        }
    }
    if (y1 >= 166 && y0 <= 199) {
        int yy0 = y0 > 166 ? y0 : 166;
        int yy1 = y1 < 199 ? y1 : 199;
        if (yy1 >= yy0) {
            graph98_boxfill(x0, yy0, x1, yy1, 7);
        }
    }
    if (y1 >= 200) {
        int yy0 = y0 > 200 ? y0 : 200;
        if (y1 >= yy0) {
            graph98_boxfill(x0, yy0, x1, y1, 6);
        }
    }

    /* 体育館の横ライン類 */
    if (12 >= y0 && 12 <= y1) graph98_hline(x0, x1, 12, 15);
    if (13 >= y0 && 13 <= y1) graph98_hline(x0, x1, 13, 7);
    if (55 >= y0 && 55 <= y1) graph98_hline(x0, x1, 55, 15);
    if (56 >= y0 && 56 <= y1) graph98_hline(x0, x1, 56, 7);
    if (110 >= y0 && 110 <= y1) graph98_hline(x0, x1, 110, 7);
    if (111 >= y0 && 111 <= y1) graph98_hline(x0, x1, 111, 0);
    if (165 >= y0 && 165 <= y1) graph98_hline(x0, x1, 165, 15);
    if (166 >= y0 && 166 <= y1) graph98_hline(x0, x1, 166, 7);
    if (199 >= y0 && 199 <= y1) graph98_hline(x0, x1, 199, 15);
    if (200 >= y0 && 200 <= y1) graph98_hline(x0, x1, 200, 0);

    /* 柱の復元 */
    if (!(x1 < 110 || x0 > 126)) {
        int xx0 = x0 > 110 ? x0 : 110;
        int xx1 = x1 < 126 ? x1 : 126;
        int yy0 = y0 > 24 ? y0 : 24;
        int yy1 = y1 < 199 ? y1 : 199;
        if (xx1 >= xx0 && yy1 >= yy0) {
            graph98_boxfill(xx0, yy0, xx1, yy1, 1);
            if (118 >= xx0 && 118 <= xx1) graph98_vline(118, yy0, yy1, 9);
            if (110 >= xx0 && 110 <= xx1) graph98_vline(110, yy0, yy1, 0);
            if (126 >= xx0 && 126 <= xx1) graph98_vline(126, yy0, yy1, 15);
        }
    }

    if (!(x1 < 236 || x0 > 252)) {
        int xx0 = x0 > 236 ? x0 : 236;
        int xx1 = x1 < 252 ? x1 : 252;
        int yy0 = y0 > 24 ? y0 : 24;
        int yy1 = y1 < 199 ? y1 : 199;
        if (xx1 >= xx0 && yy1 >= yy0) {
            graph98_boxfill(xx0, yy0, xx1, yy1, 1);
            if (244 >= xx0 && 244 <= xx1) graph98_vline(244, yy0, yy1, 9);
            if (236 >= xx0 && 236 <= xx1) graph98_vline(236, yy0, yy1, 0);
            if (252 >= xx0 && 252 <= xx1) graph98_vline(252, yy0, yy1, 15);
        }
    }

    if (!(x1 < 388 || x0 > 404)) {
        int xx0 = x0 > 388 ? x0 : 388;
        int xx1 = x1 < 404 ? x1 : 404;
        int yy0 = y0 > 24 ? y0 : 24;
        int yy1 = y1 < 199 ? y1 : 199;
        if (xx1 >= xx0 && yy1 >= yy0) {
            graph98_boxfill(xx0, yy0, xx1, yy1, 1);
            if (396 >= xx0 && 396 <= xx1) graph98_vline(396, yy0, yy1, 9);
            if (388 >= xx0 && 388 <= xx1) graph98_vline(388, yy0, yy1, 0);
            if (404 >= xx0 && 404 <= xx1) graph98_vline(404, yy0, yy1, 15);
        }
    }

    if (!(x1 < 514 || x0 > 530)) {
        int xx0 = x0 > 514 ? x0 : 514;
        int xx1 = x1 < 530 ? x1 : 530;
        int yy0 = y0 > 24 ? y0 : 24;
        int yy1 = y1 < 199 ? y1 : 199;
        if (xx1 >= xx0 && yy1 >= yy0) {
            graph98_boxfill(xx0, yy0, xx1, yy1, 1);
            if (522 >= xx0 && 522 <= xx1) graph98_vline(522, yy0, yy1, 9);
            if (514 >= xx0 && 514 <= xx1) graph98_vline(514, yy0, yy1, 0);
            if (530 >= xx0 && 530 <= xx1) graph98_vline(530, yy0, yy1, 15);
        }
    }

    /* 床ラインの復元 */
    {
        int yy;
        for (yy = 214; yy <= 294; yy += 14) {
            if (yy >= y0 && yy <= y1) {
                graph98_hline(x0, x1, yy, 14);
            }
        }
    }
    {
        int xx;
        int yy0 = y0 > 200 ? y0 : 200;
        int yy1 = y1 < 299 ? y1 : 299;
        if (yy1 >= yy0) {
            for (xx = 96; xx <= 544; xx += 48) {
                if (xx >= x0 && xx <= x1) {
                    graph98_vline(xx, yy0, yy1, 14);
                }
            }
        }
    }
}


static void ui_refresh_left_stand_only(int bg_id,
                                       enum StandId left_stand,
                                       enum FaceId left_face)
{
    ui_restore_stand_background_rect(STAND_LEFT_X,
                                     STAND_Y,
                                     STAND_LEFT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_id);
    ui_draw_stand(left_stand, left_face, STAND_LEFT_X, STAND_Y, 0);
}

static void ui_refresh_right_stand_only(int bg_id,
                                        enum StandId right_stand,
                                        enum FaceId right_face)
{
    ui_restore_stand_background_rect(STAND_RIGHT_X,
                                     STAND_Y,
                                     STAND_RIGHT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_id);
    ui_draw_stand(right_stand, right_face, STAND_RIGHT_X, STAND_Y, 1);
}

static const char *ui_get_stand_sprite_path(enum StandId stand_id,
                                            enum FaceId face_id,
                                            int facing_left)
{
    (void)facing_left;

    if (stand_id == STAND_CHARACTER01) {
        if (face_id == FACE_HAPPY) {
            return "c01_h.spr";
        }
        if (face_id == FACE_ANGRY) {
            return "c01_a.spr";
        }
        if (face_id == FACE_SURPRISED) {
            return "c01_s.spr";
        }
        return "c01_n.spr";
    }

    if (stand_id == STAND_CHARACTER02) {
        if (face_id == FACE_HAPPY) {
            return "c02_h.spr";
        }
        if (face_id == FACE_ANGRY) {
            return "c02_a.spr";
        }
        if (face_id == FACE_SURPRISED) {
            return "c02_s.spr";
        }
        return "c02_n.spr";
    }

    if (stand_id == STAND_CHARACTER03) {
        if (face_id == FACE_HAPPY) {
            return "c03_h.spr";
        }
        if (face_id == FACE_ANGRY) {
            return "c03_a.spr";
        }
        if (face_id == FACE_SURPRISED) {
            return "c03_s.spr";
        }
        return "c03_n.spr";
    }

    return 0;
}

static void ui_draw_stand_placeholder(int x, int y)
{
    /*
     * スプライトファイルがまだ無い場合の最小表示です。
     * 以前の仮立ち絵のような大きな矩形ではなく、
     * 画像ロード失敗が分かるための小さい目印だけ描きます。
     */
    graph98_boxfill(x + 28, y + 40, x + 100, y + 200, 0);
    graph98_rect(   x + 28, y + 40, x + 100, y + 200, 15);
    graph98_draw_string(x + 38, y + 112, "SPRITE", 15);
    graph98_draw_string(x + 44, y + 124, "MISSING", 12);
}

static void ui_draw_stand(enum StandId stand_id, enum FaceId face_id,
                          int x, int y, int facing_left)
{
    const char *sprite_path;

    if (stand_id == STAND_NONE) {
        return;
    }

    /*
     * 立ち絵ファイルのパスを決めてから透過付きで描画します。
     * 透明色 0 を飛ばすので、背景画像の上へ自然に重なります。
     */
    sprite_path = ui_get_stand_sprite_path(stand_id, face_id, facing_left);
    if (sprite_path == 0) {
        return;
    }

    if (!graph98_draw_sprite_file_trans(sprite_path, x, y, 0)) {
        // ui_draw_stand_placeholder(x, y);
        graph98_boxfill(x + 20, y + 20, x + 180, y + 80, 4);
        graph98_draw_string(x + 30, y + 45, "SPRITE LOAD NG", 15);

    }
}

static void ui_draw_stands_for_message(const struct Message *msg)
{
    if (msg == 0) {
        return;
    }

    ui_draw_stand(msg->left_stand,  msg->left_face,  STAND_LEFT_X,  STAND_Y, 0);
    ui_draw_stand(msg->right_stand, msg->right_face, STAND_RIGHT_X, STAND_Y, 1);
}

static int ui_draw_message_page_jis(const uint16_t *name, int name_len,
                                    const uint16_t *jis_codes, int count,
                                    int start_index)
{
    static const uint16_t jis_left_bracket = 0x215A;   /* 【 */
    static const uint16_t jis_right_bracket = 0x215B;  /* 】 */
    static const int message_line_chars = 25;
    static const int message_max_lines = 3;
    //static const int message_line1_y = 323;
    //static const int message_line2_y = 347;
    //static const int message_line3_y = 371;
    static const int message_line1_y = 321;
    static const int message_line2_y = 345;
    static const int message_line3_y = 369;



    static unsigned char bracket_font0[32];
    static unsigned char bracket_font1[32];
    static unsigned char name_font[26][32];
    static const unsigned char *name_text[28];
    static unsigned char font[78][32];
    static const unsigned char *line1[26];
    static const unsigned char *line2[26];
    static const unsigned char *line3[26];
    int i;
    int name_draw_count;
    int line1_text_limit;
    int line1_count;
    int line2_count;
    int line3_count;
    int draw_count;
    int remaining_count;
    int text_x;

    ui_draw_message_window();

    name_draw_count = name_len;
    if (name_draw_count < 0) {
        name_draw_count = 0;
    }
    if (name_draw_count > 26) {
        name_draw_count = 26;
    }

    remaining_count = count - start_index;
    if (remaining_count < 0) {
        remaining_count = 0;
    }

    draw_count = remaining_count;
    if (draw_count > message_line_chars * message_max_lines) {
        draw_count = message_line_chars * message_max_lines;
    }

    line1_text_limit = message_line_chars;
    if (name != 0 && name_draw_count > 0) {
        line1_text_limit = message_line_chars - (name_draw_count + 2);
        if (line1_text_limit < 0) {
            line1_text_limit = 0;
        }
    }

    for (i = 0; i < draw_count; ++i) {
        get_kanji_font(jis_codes[start_index + i], font[i]);
    }

    line1_count = draw_count;
    if (line1_count > line1_text_limit) {
        line1_count = line1_text_limit;
    }

    line2_count = draw_count - line1_count;
    if (line2_count > message_line_chars) {
        line2_count = message_line_chars;
    }

    line3_count = draw_count - line1_count - line2_count;
    if (line3_count > message_line_chars) {
        line3_count = message_line_chars;
    }

    text_x = 124;

    if (name != 0 && name_draw_count > 0) {
        get_kanji_font(jis_left_bracket, bracket_font0);
        name_text[0] = bracket_font0;

        for (i = 0; i < name_draw_count; ++i) {
            get_kanji_font(name[i], name_font[i]);
            name_text[i + 1] = name_font[i];
        }

        get_kanji_font(jis_right_bracket, bracket_font1);
        name_text[name_draw_count + 1] = bracket_font1;

        draw_string_kanji(116, message_line1_y, name_text, name_draw_count + 2);

        text_x = 116 + (name_draw_count + 2) * 16 + 8;
    }

    for (i = 0; i < line1_count; ++i) {
        line1[i] = font[i];
    }
    for (i = 0; i < line2_count; ++i) {
        line2[i] = font[line1_count + i];
    }
    for (i = 0; i < line3_count; ++i) {
        line3[i] = font[line1_count + line2_count + i];
    }

    if (line1_count > 0) {
        draw_string_kanji(text_x, message_line1_y, line1, line1_count);
    }
    if (line2_count > 0) {
        draw_string_kanji(116, message_line2_y, line2, line2_count);
    }
    if (line3_count > 0) {
        draw_string_kanji(116, message_line3_y, line3, line3_count);
    }


    /* 続きアイコンは非表示 */
     // ui_draw_wait_mark(528, 368, 15);

    return line1_count + line2_count + line3_count;
}

static void ui_draw_message_jis(const uint16_t *name, int name_len,
                                const uint16_t *jis_codes, int count)
{
    int start_index;
    int page_count;

    start_index = 0;

    for (;;) {
        page_count = ui_draw_message_page_jis(name, name_len,
                                              jis_codes, count,
                                              start_index);
        input_wait_key();

        if (page_count <= 0) {
            break;
        }

        start_index += page_count;
        if (start_index >= count) {
            break;
        }
    }
}

/*
 * 選択肢を表示する専用ウィンドウです。
 *
 * 今回は最小版なので、
 * - 1 行目に「1. はい」
 * - 2 行目に「2. いいえ」
 * のように 2 行固定で描きます。
 *
 * 番号部分は ASCII のまま既存関数で描き、
 * 日本語部分だけを JIS 配列から漢字 ROM 経由で描きます。
 */
static void ui_draw_choice_jis(const uint16_t *choice1, int choice1_len,
                               const uint16_t *choice2, int choice2_len,
                               int selected)
{
    static const int choice_line1_y = 317;
    static const int choice_line2_y = 339;
    static const int choice_text_x = 160;
    static const int choice_cursor_x = 128;

    static unsigned char choice1_font0[32];
    static unsigned char choice1_font1[32];
    static unsigned char choice1_font2[32];
    static unsigned char choice1_font3[32];
    static unsigned char choice1_font4[32];
    static unsigned char choice1_font5[32];
    static unsigned char choice1_font6[32];
    static unsigned char choice1_font7[32];

    static unsigned char choice2_font0[32];
    static unsigned char choice2_font1[32];
    static unsigned char choice2_font2[32];
    static unsigned char choice2_font3[32];
    static unsigned char choice2_font4[32];
    static unsigned char choice2_font5[32];
    static unsigned char choice2_font6[32];
    static unsigned char choice2_font7[32];

    static const unsigned char *line1[8];
    static const unsigned char *line2[8];
    int draw_count1;
    int draw_count2;

    /*
     * 選択肢中は右上の別ウィンドウを使わず、
     * 下部メッセージウィンドウをそのまま使います。
     */
    ui_draw_message_window();


    /* 選択中の行に帯を描く */
    if (selected == 1) {
        graph98_boxfill(140, 313, 500, 332, 15);
    }
    if (selected == 2) {
        graph98_boxfill(140, 337, 500, 354, 15);
    }

    /*
     * 選択中の行にだけ三角カーソルを表示します。
     * 今回は 2 択だけなので、
     * selected が 1 か 2 かだけ見れば十分です。
     * 
     * 現在カーソルは表示しない
     */
     /*
    if (selected == 1) {
        ui_draw_cursor_triangle(choice_cursor_x, choice_line1_y + 4, 15);
    }
    if (selected == 2) {
        ui_draw_cursor_triangle(choice_cursor_x, choice_line2_y + 4, 15);
    }
    */

    draw_count1 = choice1_len;
    if (draw_count1 > 8) {
        draw_count1 = 8;
    }

    draw_count2 = choice2_len;
    if (draw_count2 > 8) {
        draw_count2 = 8;
    }

    if (choice1 != 0 && draw_count1 > 0) {
        if (draw_count1 >= 1) {
            get_kanji_font(choice1[0], choice1_font0);
            line1[0] = choice1_font0;
        }
        if (draw_count1 >= 2) {
            get_kanji_font(choice1[1], choice1_font1);
            line1[1] = choice1_font1;
        }
        if (draw_count1 >= 3) {
            get_kanji_font(choice1[2], choice1_font2);
            line1[2] = choice1_font2;
        }
        if (draw_count1 >= 4) {
            get_kanji_font(choice1[3], choice1_font3);
            line1[3] = choice1_font3;
        }
        if (draw_count1 >= 5) {
            get_kanji_font(choice1[4], choice1_font4);
            line1[4] = choice1_font4;
        }
        if (draw_count1 >= 6) {
            get_kanji_font(choice1[5], choice1_font5);
            line1[5] = choice1_font5;
        }
        if (draw_count1 >= 7) {
            get_kanji_font(choice1[6], choice1_font6);
            line1[6] = choice1_font6;
        }
        if (draw_count1 >= 8) {
            get_kanji_font(choice1[7], choice1_font7);
            line1[7] = choice1_font7;
        }

        draw_string_kanji(choice_text_x, choice_line1_y, line1, draw_count1);
    }

    if (choice2 != 0 && draw_count2 > 0) {
        if (draw_count2 >= 1) {
            get_kanji_font(choice2[0], choice2_font0);
            line2[0] = choice2_font0;
        }
        if (draw_count2 >= 2) {
            get_kanji_font(choice2[1], choice2_font1);
            line2[1] = choice2_font1;
        }
        if (draw_count2 >= 3) {
            get_kanji_font(choice2[2], choice2_font2);
            line2[2] = choice2_font2;
        }
        if (draw_count2 >= 4) {
            get_kanji_font(choice2[3], choice2_font3);
            line2[3] = choice2_font3;
        }
        if (draw_count2 >= 5) {
            get_kanji_font(choice2[4], choice2_font4);
            line2[4] = choice2_font4;
        }
        if (draw_count2 >= 6) {
            get_kanji_font(choice2[5], choice2_font5);
            line2[5] = choice2_font5;
        }
        if (draw_count2 >= 7) {
            get_kanji_font(choice2[6], choice2_font6);
            line2[6] = choice2_font6;
        }
        if (draw_count2 >= 8) {
            get_kanji_font(choice2[7], choice2_font7);
            line2[7] = choice2_font7;
        }

        draw_string_kanji(choice_text_x, choice_line2_y, line2, draw_count2);
    }
}

/* 8 ビット値を I/O ポートへ書き込みます。 */
static void io_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
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
static void get_kanji_font(uint16_t jis_code, unsigned char *buffer)
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

/* Enterキーが押されるまで待ちます。 */
static void input_wait_key(void)
{
    uint8_t ch;

    for (;;) {
        __asm__ __volatile__(
            "movb $0x08, %%ah\n\t"
            "int $0x21\n\t"
            "movb %%al, %0"
            : "=rm"(ch)
            :
            : "ax", "cc", "memory");

        if (ch == 0x0D) {
            break;  /* Enter */
        }
    }
}

/*
 * '1' または '2' が押されるまで待ちます。
 *
 * 今回の選択肢は 2 個だけなので、
 * それ以外のキーは無視して待ち続けます。
 * 
 * 注意！　これは古い関数で使われていません！
 * 　input_wait_choice_jis
 * を使用しています。
 */
static int input_wait_choice_cursor(const struct Message *msg,
                                    enum BackgroundId current_bg)
{
    uint8_t ch;
    int selected;

    selected = 1;

    for (;;) {
        graph98_clear(0);
        ui_draw_background(current_bg);
        ui_draw_stands_for_message(msg);
        ui_draw_choice_jis(msg->choice1, msg->choice1_len,
                           msg->choice2, msg->choice2_len,
                           selected);

        __asm__ __volatile__(
            "movb $0x08, %%ah\n\t"
            "int $0x21\n\t"
            "movb %%al, %0"
            : "=rm"(ch)
            :
            : "ax", "cc", "memory");

        /* Enter で決定 */
        if (ch == 0x0D) {
            return selected;
        }

        /* PC-98 実測値 */
        if (ch == 0x0B) {
            selected = 1;   /* ↑ */
        }

        if (ch == 0x0A) {
            selected = 2;   /* ↓ */
        }

        /* 保険として代替キーも残す */
        if (ch == 'W' || ch == 'w' || ch == '8') {
            selected = 1;
        }

        if (ch == 'S' || ch == 's' || ch == '2') {
            selected = 2;
        }
    }
}


static void text98_clear_screen(void)
{
    __asm__ __volatile__(
        "push %%es\n\t"

        /* 文字エリア A000:0000 を空白(0x20, 0x00)で埋める */
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "xor %%di, %%di\n\t"
        "movw $2000, %%cx\n\t"
        "movw $0x0020, %%ax\n"
        "1:\n\t"
        "stosw\n\t"
        "loop 1b\n\t"

        /* 属性エリア A200:0000 もゼロで埋める */
        "movw $0xA200, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "xor %%di, %%di\n\t"
        "movw $2000, %%cx\n\t"
        "xor %%ax, %%ax\n"
        "2:\n\t"
        "stosw\n\t"
        "loop 2b\n\t"

        "pop %%es"
        :
        :
        : "ax", "cx", "di", "memory", "cc");
}

static void text98_hide_cursor(void)
{
    __asm__ __volatile__(
        "movb $0x12, %%ah\n\t"
        "xorw %%cx, %%cx\n\t"
        "int $0x18"
        :
        :
        : "ax", "cx", "memory", "cc");
}

// ASCII用の簡易表示関数
static void ui_draw_message_ascii(const char *name, const char *text)
{
    ui_draw_message_window();

    if (name != 0) {
        graph98_draw_string(105, 321, "[", 1);
        graph98_draw_string(113, 321, name, 1);
        graph98_draw_string(113 + strlen(name) * 6, 321, "]", 1);
    }

    graph98_draw_string(105, 345, text, 1);

    /* 続きアイコンは非表示 */
    // ui_draw_wait_mark(528, 368, 15);
}

// 改行削除関数
static void remove_newline(char *str)
{
    int len = (int)strlen(str);

    while (len > 0 &&
           (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        --len;
    }
}

// 先頭空白を飛ばす小関数
static void trim_leading_spaces(char *str)
{
    int i;
    int j;

    i = 0;
    while (str[i] == ' ' || str[i] == '\t') {
        i++;
    }

    if (i == 0) {
        return;
    }

    j = 0;
    while (str[i] != '\0') {
        str[j++] = str[i++];
    }
    str[j] = '\0';
}

// ラベルを探してそこへ飛ぶ関数
static int find_label_and_jump(FILE *fp, const char *label_name)
{
    char line[256];
    char cmd[32];
    char arg1[64];
    int count;

    if (fp == 0 || label_name == 0 || label_name[0] == '\0') {
        return 0;
    }

    /* いったん先頭へ戻ってラベルを探す */
    fseek(fp, 0L, SEEK_SET);

    while (fgets(line, sizeof(line), fp) != 0) {
        remove_newline(line);
        trim_leading_spaces(line);

        if (line[0] != '#') {
            continue;
        }

        cmd[0] = '\0';
        arg1[0] = '\0';

        count = sscanf(line, "%31s %63s", cmd, arg1);
        if (count >= 2) {
            if (strcmp(cmd, "#label") == 0 && strcmp(arg1, label_name) == 0) {
                /*
                 * ここで見つかった時点で、
                 * fp は #label 行の次の行を指している。
                 * そのまま戻れば、次の fgets() から本編再開できる。
                 */
                return 1;
            }
        }
    }

    return 0;
}

// フラグ関数
static int find_flag_index(const char *name)
{
    int i;

    if (name == 0 || name[0] == '\0') {
        return -1;
    }

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (g_flags[i].name[0] == '\0') {
            continue;
        }

        if (strcmp(g_flags[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

static void set_flag_on(const char *name)
{
    int i;
    int idx;

    if (name == 0 || name[0] == '\0') {
        return;
    }

    idx = find_flag_index(name);
    if (idx >= 0) {
        g_flags[idx].value = 1;
        return;
    }

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (g_flags[i].name[0] == '\0') {
            strcpy(g_flags[i].name, name);
            g_flags[i].value = 1;
            return;
        }
    }
}

static void set_flag_off(const char *name)
{
    int idx;

    if (name == 0 || name[0] == '\0') {
        return;
    }

    idx = find_flag_index(name);
    if (idx >= 0) {
        g_flags[idx].value = 0;
    }
}

static int is_flag_on(const char *name)
{
    int idx;

    idx = find_flag_index(name);
    if (idx >= 0) {
        return g_flags[idx].value;
    }

    return 0;
}

// Shift_JIS 1文字 → JIS 1文字
static uint16_t sjis_to_jis(uint8_t sjis_hi, uint8_t sjis_lo)
{
    uint8_t row;
    uint8_t cell;

    if (sjis_lo < 0x9F) {
        if (sjis_hi <= 0x9F) {
            row = (uint8_t)((sjis_hi - 0x81) * 2 + 0x21);
        } else {
            row = (uint8_t)((sjis_hi - 0xC1) * 2 + 0x5F);
        }

        if (sjis_lo >= 0x7F) {
            sjis_lo--;
        }
        cell = (uint8_t)(sjis_lo - 0x1F);
    } else {
        if (sjis_hi <= 0x9F) {
            row = (uint8_t)((sjis_hi - 0x81) * 2 + 0x22);
        } else {
            row = (uint8_t)((sjis_hi - 0xC1) * 2 + 0x60);
        }

        cell = (uint8_t)(sjis_lo - 0x7E);
    }

    return (uint16_t)(((uint16_t)row << 8) | cell);
}

// 文字列全体を Shift_JIS → JIS配列へ変換
static int convert_sjis_string_to_jis_array(const unsigned char *src,
                                            uint16_t *dst,
                                            int max_chars)
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

        /* 全角1文字（Shift_JIS 2バイト） */
        if (((c >= 0x81) && (c <= 0x9F)) ||
            ((c >= 0xE0) && (c <= 0xEF))) {
            uint8_t c2;

            src++;
            if (*src == '\0') {
                break;
            }

            c2 = *src;
            dst[count++] = sjis_to_jis(c, c2);
            src++;
            continue;
        }

        /*
         * 半角英数・記号は今回は無視。
         * 必要なら後で対応追加。
         */
        src++;
    }

    return count;
}

// [名前] から名前部分だけ取り出す
static void extract_name_from_brackets(const char *line, char *out_name, int out_size)
{
    int i;
    int j;

    j = 0;

    for (i = 1; line[i] != '\0' && line[i] != ']'; ++i) {
        if (j < out_size - 1) {
            out_name[j++] = line[i];
        }
    }

    out_name[j] = '\0';
}

// 日本語 script.txt を再生する関数
static void run_script_sjis(void)
{
    FILE *fp;
    char line[256];
    char current_name[128];

    uint16_t name_jis[64];
    uint16_t text_jis[128];

    int name_len;
    int text_len;

    enum BackgroundId current_bg;
    enum StandId current_left_stand;
    enum StandId current_right_stand;
    enum FaceId current_left_face;
    enum FaceId current_right_face;

    enum BackgroundId last_bg;
    enum StandId last_left_stand;
    enum StandId last_right_stand;
    enum FaceId last_left_face;
    enum FaceId last_right_face;
    int scene_dirty;

    int stand_dirty;
    
    
    stand_dirty = 0;

    current_name[0] = '\0';

    current_bg = BG_01;
    current_left_stand = STAND_NONE;
    current_right_stand = STAND_NONE;
    current_left_face = FACE_NORMAL;
    current_right_face = FACE_NORMAL;

    last_bg = BG_NONE;
    last_left_stand = STAND_NONE;
    last_right_stand = STAND_NONE;
    last_left_face = FACE_NORMAL;
    last_right_face = FACE_NORMAL;
    scene_dirty = 1;

    fp = fopen("script.txt", "rb");
    if (fp == 0) {
        return;
    }

    while (fgets(line, sizeof(line), fp) != 0) {
        remove_newline(line);

        if (line[0] == '\0') {
            continue;
        }

        if (line[0] == '#') {
            char cmd[32];
            char arg1[64];
            char arg2[64];
            int count;

            cmd[0] = '\0';
            arg1[0] = '\0';
            arg2[0] = '\0';

            count = sscanf(line, "%31s %63s %63s", cmd, arg1, arg2);

            if (strcmp(cmd, "#choice") == 0) {
                if (count >= 3) {
                    handle_choice_block(fp, arg1, arg2);
                }
                continue;
            }

            if (strcmp(cmd, "#label") == 0) {
                continue;
            }

            if (strcmp(cmd, "#jump") == 0) {
                if (count >= 2) {
                    find_label_and_jump(fp, arg1);
                }
                continue;
            }

            if (strcmp(cmd, "#set") == 0) {
                if (count >= 2) {
                    set_flag_on(arg1);
                }
                continue;
            }

            if (strcmp(cmd, "#reset") == 0) {
                if (count >= 2) {
                    set_flag_off(arg1);
                }
                continue;
            }

            if (strcmp(cmd, "#if") == 0) {
                if (count >= 3) {
                    if (is_flag_on(arg1)) {
                        find_label_and_jump(fp, arg2);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#ifnot") == 0) {
                if (count >= 3) {
                    if (!is_flag_on(arg1)) {
                        find_label_and_jump(fp, arg2);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#pal") == 0) {
                if (count >= 2) {
                    // printfを使いと.COMの容量オーバーになるのでprintデバックは今使わない
                    // printf("[PAL] load: %s\n", arg1);

                    if (graph98_load_palette_file(arg1)) {
                        // printfを使いと.COMの容量オーバーになるのでprintデバックは今使わない
                        // printf("[PAL] success\n");
                        scene_dirty = 1;
                    } else {
                        // printfを使いと.COMの容量オーバーになるのでprintデバックは今使わない
                        // printf("[PAL] FAILED\n");
                    }
                } else {
                    // printfを使いと.COMの容量オーバーになるのでprintデバックは今使わないZ
                    // printf("[PAL] invalid args\n");
                }
                continue;
            }

            if (strcmp(cmd, "#se") == 0) {
                if (count >= 2) {
                    if (strcmp(arg1, "beep") == 0) {
                        se86_play_beep();
                    } else if (strcmp(arg1, "beep2") == 0) {
                        se86_play_beep2();
                    }
                }
                continue;
            }





            {
                enum BackgroundId old_bg;
                enum StandId old_left_stand;
                enum FaceId old_left_face;
                enum StandId old_right_stand;
                enum FaceId old_right_face;

                old_bg = current_bg;
                old_left_stand = current_left_stand;
                old_left_face = current_left_face;
                old_right_stand = current_right_stand;
                old_right_face = current_right_face;

                process_command_line(line,
                                     &current_bg,
                                     &current_left_stand,
                                     &current_left_face,
                                     &current_right_stand,
                                     &current_right_face);

                if (current_bg != old_bg) {
                    scene_dirty = 1;
                    stand_dirty = 0;
                } else if (current_left_stand != old_left_stand ||
                           current_left_face != old_left_face ||
                           current_right_stand != old_right_stand ||
                           current_right_face != old_right_face) {
                    stand_dirty = 1;
                }
            }
            continue;
        }

        if (line[0] == '[') {
            extract_name_from_brackets(line, current_name, sizeof(current_name));
            continue;
        }

        name_len = convert_sjis_string_to_jis_array(
            (const unsigned char *)current_name,
            name_jis,
            64
        );

        text_len = convert_sjis_string_to_jis_array(
            (const unsigned char *)line,
            text_jis,
            128
        );

        // 変化があれば全画面再描画
        if (scene_dirty ||
            stand_dirty ||
            last_bg != current_bg) {

            graph98_clear(0);
            ui_draw_background(current_bg);
            ui_draw_current_stands(current_left_stand, current_left_face,
                                   current_right_stand, current_right_face);

            last_bg = current_bg;
            last_left_stand = current_left_stand;
            last_left_face = current_left_face;
            last_right_stand = current_right_stand;
            last_right_face = current_right_face;
            scene_dirty = 0;
            stand_dirty = 0;
        }
        ui_draw_message_jis(name_jis, name_len, text_jis, text_len);
    }

    fclose(fp);
}

// スクリプト再生関数
static void run_script_ascii(void)
{
    FILE *fp;
    char line[256];
    char current_name[64] = "";

    fp = fopen("script.txt", "r");
    if (fp == 0) {
        return;
    }

    while (fgets(line, sizeof(line), fp) != 0) {

        remove_newline(line);

        /* 空行スキップ */
        if (line[0] == '\0') {
            continue;
        }

        /* 名前行 [NAME] */
        if (line[0] == '[') {
            int i;
            int j = 0;

            for (i = 1; line[i] != '\0' && line[i] != ']'; ++i) {
                current_name[j++] = line[i];
            }
            current_name[j] = '\0';

            continue;
        }

        /* セリフ表示 */
        graph98_clear(0);
        ui_draw_background(BG_01);

        /* ここはとりあえず固定立ち絵でもOK */
        ui_draw_stand(STAND_CHARACTER01, FACE_NORMAL, 60, STAND_Y, 0);

        ui_draw_message_ascii(current_name, line);

        input_wait_key();
    }

    fclose(fp);
}


// 文字列を背景IDへ
static enum BackgroundId parse_bg_id(const char *name)
{
    if (strcmp(name, "bg01") == 0) {
        return BG_01;
    }
    if (strcmp(name, "bg02") == 0) {
        return BG_02;
    }
    if (strcmp(name, "bg03") == 0) {
        return BG_03;
    }

    /* 旧スクリプト互換 */
    /*
    if (strcmp(name, "gym") == 0) {
        return BG_01;
    }
    if (strcmp(name, "classroom") == 0) {
        return BG_02;
    }
    if (strcmp(name, "test") == 0) {
        return BG_NONE;
    }
    */

    return BG_NONE;
}

// 文字列を立ち絵IDへ
static enum StandId parse_stand_id(const char *name)
{
    if (strcmp(name, "none") == 0) {
        return STAND_NONE;
    }

    if (strcmp(name, "character01") == 0) {
        return STAND_CHARACTER01;
    }
    if (strcmp(name, "character02") == 0) {
        return STAND_CHARACTER02;
    }
    if (strcmp(name, "character03") == 0) {
        return STAND_CHARACTER03;
    }

    /* 古いスクリプト用の別名。不要なら後で消してOK */
    /*
    if (strcmp(name, "anzai") == 0) {
        return STAND_CHARACTER01;
    }
    if (strcmp(name, "mitsui") == 0) {
        return STAND_CHARACTER02;
    }
    if (strcmp(name, "sakuragi") == 0) {
        return STAND_CHARACTER03;
    }
    */

    return STAND_NONE;
}

// 文字列を表情IDへ
static enum FaceId parse_face_id(const char *name)
{
    if (strcmp(name, "happy") == 0) {
        return FACE_HAPPY;
    }
    if (strcmp(name, "angry") == 0) {
        return FACE_ANGRY;
    }
    if (strcmp(name, "surprised") == 0) {
        return FACE_SURPRISED;
    }

    return FACE_NORMAL;
}

// 背景ファイル取得関数
static const char *get_bg_path(enum BackgroundId bg_id)
{
    if (bg_id == BG_01) {
        return "bg001.g98";
    }
    if (bg_id == BG_02) {
        return "bg002.g98";
    }
    if (bg_id == BG_03) {
        return "bg003.g98";
    }

    return 0;
}

// コマンド行を読む関数
static void process_command_line(const char *line,
                                 enum BackgroundId *bg_id,
                                 enum StandId *left_stand,
                                 enum FaceId *left_face,
                                 enum StandId *right_stand,
                                 enum FaceId *right_face)
{
    char cmd[32];
    char arg1[32];
    char arg2[32];
    int count;

    cmd[0] = '\0';
    arg1[0] = '\0';
    arg2[0] = '\0';

    count = sscanf(line, "%31s %31s %31s", cmd, arg1, arg2);
    if (count <= 0) {
        return;
    }

    if (strcmp(cmd, "#bg") == 0) {
        if (count >= 2) {
            *bg_id = parse_bg_id(arg1);
        }
        return;
    }

    if (strcmp(cmd, "#left") == 0) {
        if (count >= 2) {
            *left_stand = parse_stand_id(arg1);
        }
        if (count >= 3) {
            *left_face = parse_face_id(arg2);
        } else {
            *left_face = FACE_NORMAL;
        }
        return;
    }

    if (strcmp(cmd, "#right") == 0) {
        if (count >= 2) {
            *right_stand = parse_stand_id(arg1);
        }
        if (count >= 3) {
            *right_face = parse_face_id(arg2);
        } else {
            *right_face = FACE_NORMAL;
        }
        return;
    }
}

// 立ち絵を今の状態で描く関数
static void ui_draw_current_stands(enum StandId left_stand,
                                   enum FaceId left_face,
                                   enum StandId right_stand,
                                   enum FaceId right_face)
{
    ui_draw_stand(left_stand, left_face, STAND_LEFT_X, STAND_Y, 0);
    ui_draw_stand(right_stand, right_face, STAND_RIGHT_X, STAND_Y, 1);
}


// 新しい入力待ち関数を追加
static int input_wait_choice_jis(const uint16_t *choice1, int choice1_len,
                                 const uint16_t *choice2, int choice2_len)
{
    uint8_t ch;
    int selected;

    selected = 1;

    for (;;) {
        ui_draw_choice_jis(choice1, choice1_len,
                           choice2, choice2_len,
                           selected);

        __asm__ __volatile__(
            "movb $0x08, %%ah\n\t"
            "int $0x21\n\t"
            "movb %%al, %0"
            : "=rm"(ch)
            :
            : "ax", "cc", "memory");

        if (ch == 0x0D) {
            return selected;
        }

        if (ch == 0x0B) {
            selected = 1;   /* ↑ */
        }

        if (ch == 0x0A) {
            selected = 2;   /* ↓ */
        }

        if (ch == 'W' || ch == 'w' || ch == '8') {
            selected = 1;
        }

        if (ch == 'S' || ch == 's' || ch == '2') {
            selected = 2;
        }
    }
}

// #choice 用の処理関数を追加
static void handle_choice_block(FILE *fp, const char *label1, const char *label2)
{
    char line1[256];
    char line2[256];

    uint16_t choice1_jis[64];
    uint16_t choice2_jis[64];

    int choice1_len;
    int choice2_len;
    int selected;

    if (fgets(line1, sizeof(line1), fp) == 0) {
        return;
    }
    if (fgets(line2, sizeof(line2), fp) == 0) {
        return;
    }

    remove_newline(line1);
    remove_newline(line2);

    choice1_len = convert_sjis_string_to_jis_array(
        (const unsigned char *)line1,
        choice1_jis,
        64
    );

    choice2_len = convert_sjis_string_to_jis_array(
        (const unsigned char *)line2,
        choice2_jis,
        64
    );

    selected = input_wait_choice_jis(choice1_jis, choice1_len,
                                     choice2_jis, choice2_len);

    if (selected == 1) {
        find_label_and_jump(fp, label1);
    } else {
        find_label_and_jump(fp, label2);
    }
}

int main(void)
{
    text98_clear_screen();
    text98_hide_cursor();

    (void)pmd_is_resident();

    graph98_init();
    se86_init();

    if (!graph98_load_palette_file("adv.pal")) {
        graph98_apply_adv_palette();
    }

    graph98_clear(0);

    run_script_sjis();

    return 0;
}
