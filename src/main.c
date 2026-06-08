#include "graph98.h"
#include "fm86.h"
#include "pmd.h"
#include "mouse98.h"


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

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

/* 背景ワイプ速度。16 または 24 程度を推奨。 */
#define BG_WIPE_STEP_LINES 8


struct Message;

enum StandId {
    STAND_NONE = 0,
    STAND_CHARACTER01,
    STAND_CHARACTER02,
    STAND_CHARACTER03,
    STAND_CHARACTER04,
    STAND_CHARACTER05,
    STAND_CHARACTER06,
    STAND_CHARACTER07,
    STAND_CHARACTER08,
    STAND_CHARACTER09,
    STAND_CHARACTER10,
    STAND_CHARACTER11,
    STAND_CHARACTER12,
    STAND_CHARACTER13,
    STAND_CHARACTER14,
    STAND_CHARACTER15,
    STAND_CHARACTER16,
    STAND_CHARACTER17,
    STAND_CHARACTER18,
    STAND_CHARACTER19,
    STAND_CHARACTER20
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
#define SAVE_VERSION 1

typedef struct {
    char bg_name[32];
    int script_line;

    enum StandId left_stand;
    enum FaceId left_face;

    enum StandId right_stand;
    enum FaceId right_face;

    char bgm[64];
} GameState;

typedef struct {
    char magic[8];
    int version;
    GameState state;
    GameFlag flags[MAX_FLAGS];
} SaveData;

static GameFlag g_flags[MAX_FLAGS];
static GameState g_state;
static int g_pmd_available = 0;

// マウス制御
static int g_mouse_available = 0;
static int g_save_key_armed = 1;
static int g_load_key_armed = 1;
static int g_request_scene_redraw = 0;
static int g_request_script_resume = 0;

/* メッセージボックス座標。初期値は従来と同じ */
static int g_msgbox_x0 = 109;
static int g_msgbox_y0 = 313;
static int g_msgbox_x1 = 531;
static int g_msgbox_y1 = 392;

static int g_msg_text_x = 116;
static int g_msg_line1_y = 321;
static int g_msg_line2_y = 345;
static int g_msg_line3_y = 369;

/*
static int g_msgbox_x0 = 50;
static int g_msgbox_y0 = 300;
static int g_msgbox_x1 = 620;
static int g_msgbox_y1 = 390;

static int g_msg_text_x = 57;
static int g_msg_line1_y = 308;
static int g_msg_line2_y = 332;
static int g_msg_line3_y = 356;
*/

static int g_choice_line1_y = 317;
static int g_choice_line2_y = 339;
static int g_choice_text_x = 160;
static int g_choice_cursor_x = 128;
static int g_choice_band_x0 = 140;
static int g_choice_band_x1 = 500;
static int g_choice_band1_y0 = 313;
static int g_choice_band1_y1 = 332;
static int g_choice_band2_y0 = 337;
static int g_choice_band2_y1 = 354;



/* 関数宣言部 */
static int input_wait_key(void);
static void ui_redraw_current_scene_from_state(void);
static void ui_hide_message_window_until_resume(void);
static void ui_draw_wait_mark(int x, int y, unsigned char color);
static void text98_hide_cursor(void);
static void text98_clear_screen(void);
static int input_wait_choice_cursor(const struct Message *msg,
                                    const char *current_bg_name);
static void io_out8(uint16_t port, uint8_t value);
static void get_kanji_font(uint16_t jis_code, unsigned char *buffer);
static void ui_draw_background(const char *bg_name);
static void ui_draw_background_center_wipe(const char *bg_name);
static void ui_draw_message_window(void);
static void ui_set_message_box(int x0, int y0, int x1, int y1);
static int ui_get_message_line_chars(void);
static void ui_draw_background_test(void);
static const char *ui_get_stand_sprite_path(enum StandId stand_id,
                                            enum FaceId face_id,
                                            int facing_left);
static void ui_draw_stand_placeholder(int x, int y);
static void ui_draw_stand(enum StandId stand_id, enum FaceId face_id,
                          int x, int y, int facing_left);
static void ui_draw_stand_center_wipe(enum StandId stand_id, enum FaceId face_id,
                                      int x, int y, int facing_left);
static void ui_draw_stands_for_message(const struct Message *msg);
static void draw_string_kanji(int x, int y, const unsigned char **fonts, int count);
static void ui_draw_cursor_triangle(int x, int y, unsigned char color);
static int ui_draw_message_page_jis(const uint16_t *name, int name_len,
                                    const uint16_t *jis_codes, int count,
                                    int start_index);
static void ui_draw_message_jis(const uint16_t *name, int name_len,
                                const uint16_t *jis_codes, int count);
static enum StandId parse_stand_id(const char *name);
static enum FaceId parse_face_id(const char *name);
static void process_command_line(const char *line,
                                 char *bg_name,
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
static void handle_choice_block(FILE *fp, int *script_line,
                                const char *label1, const char *label2);
static int find_label_and_jump(FILE *fp, int *script_line,
                               const char *label_name);
static void trim_leading_spaces(char *str);
static int find_flag_index(const char *name);
static void set_flag_on(const char *name);
static void set_flag_off(const char *name);
static int is_flag_on(const char *name);
static void ui_restore_stand_background_rect(int x0, int y0, int x1, int y1, const char *bg_name);
static void ui_refresh_left_stand_only(const char *bg_name,
                                       enum StandId left_stand,
                                       enum FaceId left_face);
static void ui_refresh_left_stand_only_wipe(const char *bg_name,
                                            enum StandId left_stand,
                                            enum FaceId left_face);
static void ui_refresh_right_stand_only(const char *bg_name,
                                        enum StandId right_stand,
                                        enum FaceId right_face);
static void ui_refresh_right_stand_only_wipe(const char *bg_name,
                                             enum StandId right_stand,
                                             enum FaceId right_face);

static GameFlag *find_flag(const char *name);
static GameFlag *find_or_create_flag(const char *name);
static void add_flag_value(const char *name, int value);
static void set_flag_value(const char *name, int value);
static int get_flag_value(const char *name);
static void debug_log(const char *fmt, ...);
static int read_script_line(FILE *fp, char *line, int line_size, int *script_line);
static int save_game_state(void);
static int load_game_state(void);
static void extract_name_from_brackets(const char *line, char *out_name, int out_size);
static void resume_script_line(FILE *fp, int *script_line,
                               char *current_name, int current_name_size);
static void handle_save_hotkey(uint8_t ch);
static void handle_load_hotkey(uint8_t ch);

// マウス関連
static int input_key_available(void);
static uint8_t input_read_key(void);


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

    // debug_log("draw_string_kanji start x=%d y=%d count=%d", x, y, count);

    for (i = 0; i < count; ++i) {
        if (fonts[i] != 0) {
            draw_kanji_16(x + (i * 16), y, fonts[i]);
        }
    }
    // debug_log("draw_string_kanji done");
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


static void ui_set_message_box(int x0, int y0, int x1, int y1)
{
    if (x0 < 0 || y0 < 0 || x1 >= 640 || y1 >= 400) {
        return;
    }
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    g_msgbox_x0 = x0;
    g_msgbox_y0 = y0;
    g_msgbox_x1 = x1;
    g_msgbox_y1 = y1;

    g_msg_text_x = g_msgbox_x0 + 7;
    g_msg_line1_y = g_msgbox_y0 + 8;
    g_msg_line2_y = g_msgbox_y0 + 32;
    g_msg_line3_y = g_msgbox_y0 + 56;

    g_choice_line1_y = g_msgbox_y0 + 12;
    g_choice_line2_y = g_msgbox_y0 + 34;
    g_choice_text_x = g_msgbox_x0 + 51;
    g_choice_cursor_x = g_msgbox_x0 + 19;

    g_choice_band_x0 = g_msgbox_x0 + 31;
    g_choice_band_x1 = g_msgbox_x1 - 31;

    g_choice_band1_y0 = g_choice_line1_y;
    g_choice_band1_y1 = g_choice_line1_y + 15;
    g_choice_band2_y0 = g_choice_line2_y;
    g_choice_band2_y1 = g_choice_line2_y + 15;
}

static int ui_get_message_line_chars(void)
{
    int usable_width;
    int chars;

    /*
     * 全角16x16前提。
     * 右側に1文字ぶん余白を残す。
     * 初期値 109,313,531,392 では従来どおり 25 文字になる。
     */
    usable_width = g_msgbox_x1 - g_msg_text_x + 1 - 16;
    chars = usable_width / 16;

    if (chars < 1) {
        chars = 1;
    }

    /*
     * 640px幅でも最大40文字程度。
     * 配列サイズ保護のため上限を付ける。
     */
    if (chars > 40) {
        chars = 40;
    }

    return chars;
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
    // graph98_rect(96, 308, 543, 381, 8);
    // graph98_boxfill(109, 313, 531, 392, 0);
    graph98_boxfill(g_msgbox_x0, g_msgbox_y0, g_msgbox_x1, g_msgbox_y1, 0);
}

static void build_bg_path(const char *bg_name, char *path, int path_size)
{
    int i;

    if (path_size <= 0) {
        return;
    }

    if (bg_name == 0 || bg_name[0] == '\0') {
        path[0] = '\0';
        return;
    }

    for (i = 0; i < path_size - 1 && bg_name[i] != '\0'; ++i) {
        path[i] = bg_name[i];
    }

    if (i < path_size - 1) {
        path[i++] = '.';
    }
    if (i < path_size - 1) {
        path[i++] = 'g';
    }
    if (i < path_size - 1) {
        path[i++] = '9';
    }
    if (i < path_size - 1) {
        path[i++] = '8';
    }

    path[i] = '\0';
}

static void ui_draw_background(const char *bg_name)
{
    char path[40];

    if (bg_name == 0 || bg_name[0] == '\0') {
        ui_draw_background_test();
        return;
    }

    build_bg_path(bg_name, path, sizeof(path));

    if (path[0] != '\0') {
        if (graph98_load_g98(path)) {
            return;
        }
        debug_log("bg load failed: %s", path);
    }

    ui_draw_background_test();
}

static void ui_draw_background_center_wipe(const char *bg_name)
{
    char path[40];

    if (bg_name == 0 || bg_name[0] == '\0') {
        ui_draw_background_test();
        return;
    }

    build_bg_path(bg_name, path, sizeof(path));

    if (path[0] != '\0') {
        if (graph98_load_g98_center_wipe(path, BG_WIPE_STEP_LINES)) {
            return;
        }
        debug_log("bg wipe load failed: %s", path);
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

static void ui_restore_stand_background_rect(int x0, int y0, int x1, int y1, const char *bg_name)
{
    char path[40];

    if (bg_name != 0 && bg_name[0] != '\0') {
        build_bg_path(bg_name, path, sizeof(path));

        if (path[0] != '\0') {
            if (graph98_load_g98_rect(path, x0, y0, x1, y1)) {
                return;
            }
        }
    }

    /*
     * 背景ファイル復元に失敗した場合の保険。
     * ここでは全背景再描画に戻します。
     */
    ui_draw_background(bg_name);
}

static void ui_refresh_left_stand_only(const char *bg_name,
                                       enum StandId left_stand,
                                       enum FaceId left_face)
{
    ui_restore_stand_background_rect(STAND_LEFT_X,
                                     STAND_Y,
                                     STAND_LEFT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_name);
    ui_draw_stand(left_stand, left_face, STAND_LEFT_X, STAND_Y, 0);
}

static void ui_refresh_left_stand_only_wipe(const char *bg_name,
                                            enum StandId left_stand,
                                            enum FaceId left_face)
{
    ui_restore_stand_background_rect(STAND_LEFT_X,
                                     STAND_Y,
                                     STAND_LEFT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_name);
    ui_draw_stand_center_wipe(left_stand, left_face, STAND_LEFT_X, STAND_Y, 0);
}

static void ui_refresh_right_stand_only(const char *bg_name,
                                        enum StandId right_stand,
                                        enum FaceId right_face)
{
    ui_restore_stand_background_rect(STAND_RIGHT_X,
                                     STAND_Y,
                                     STAND_RIGHT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_name);
    ui_draw_stand(right_stand, right_face, STAND_RIGHT_X, STAND_Y, 1);
}

static void ui_refresh_right_stand_only_wipe(const char *bg_name,
                                             enum StandId right_stand,
                                             enum FaceId right_face)
{
    ui_restore_stand_background_rect(STAND_RIGHT_X,
                                     STAND_Y,
                                     STAND_RIGHT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_name);
    ui_draw_stand_center_wipe(right_stand, right_face, STAND_RIGHT_X, STAND_Y, 1);
}

static const char *ui_get_stand_sprite_path(enum StandId stand_id,
                                            enum FaceId face_id,
                                            int facing_left)
{
    static const char face_suffixes[] = {
        'n', /* FACE_NORMAL */
        'h', /* FACE_HAPPY */
        'a', /* FACE_ANGRY */
        's'  /* FACE_SURPRISED */
    };
    static char path[16];
    int stand_no;
    char face_suffix;

    (void)facing_left;

    if (stand_id <= STAND_NONE || stand_id > STAND_CHARACTER20) {
        return 0;
    }

    if (face_id < FACE_NORMAL || face_id > FACE_SURPRISED) {
        face_suffix = 'n';
    } else {
        face_suffix = face_suffixes[face_id];
    }

    stand_no = (int)stand_id;
    snprintf(path, sizeof(path), "c%02d_%c.spr", stand_no, face_suffix);

    return path;
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
        debug_log("sprite load failed: %s", sprite_path);

        // ui_draw_stand_placeholder(x, y);
        graph98_boxfill(x + 20, y + 20, x + 180, y + 80, 4);
        graph98_draw_string(x + 30, y + 45, "SPRITE LOAD NG", 15);

    }



}

static void ui_draw_stand_center_wipe(enum StandId stand_id, enum FaceId face_id,
                                      int x, int y, int facing_left)
{
    const char *sprite_path;

    if (stand_id == STAND_NONE) {
        return;
    }

    sprite_path = ui_get_stand_sprite_path(stand_id, face_id, facing_left);
    if (sprite_path == 0) {
        return;
    }

    if (!graph98_draw_sprite_file_trans_center_wipe(sprite_path, x, y, 0, 16)) {
        debug_log("sprite wipe load failed: %s", sprite_path);
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
    static const int message_max_lines = 3;
    static unsigned char bracket_font0[32];
    static unsigned char bracket_font1[32];
    static unsigned char name_font[26][32];
    static const unsigned char *name_text[28];
    static unsigned char font[120][32];
    static const unsigned char *line1[40];
    static const unsigned char *line2[40];
    static const unsigned char *line3[40];

    int i;
    int name_draw_count;
    int line1_text_limit;
    int line1_count;
    int line2_count;
    int line3_count;
    int draw_count;
    int remaining_count;
    int message_line1_y;
    int message_line2_y;
    int message_line3_y;
    int text_x;
    int message_line_chars;



    ui_draw_message_window();
    message_line_chars = ui_get_message_line_chars();

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

    // debug_log("draw_count=%d", draw_count);
    for (i = 0; i < draw_count; ++i) {
        // debug_log("get_kanji_font: %04X", jis_codes[start_index + i]);
        get_kanji_font(jis_codes[start_index + i], font[i]);
        // debug_log("get_kanji_font done: %04X", jis_codes[start_index + i]);
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

    message_line1_y = g_msg_line1_y;
    message_line2_y = g_msg_line2_y;
    message_line3_y = g_msg_line3_y;
    text_x = g_msg_text_x;

    if (name != 0 && name_draw_count > 0) {
        get_kanji_font(jis_left_bracket, bracket_font0);
        name_text[0] = bracket_font0;

        for (i = 0; i < name_draw_count; ++i) {
            get_kanji_font(name[i], name_font[i]);
            name_text[i + 1] = name_font[i];
        }

        get_kanji_font(jis_right_bracket, bracket_font1);
        name_text[name_draw_count + 1] = bracket_font1;

        draw_string_kanji(g_msg_text_x, message_line1_y, name_text, name_draw_count + 2);

        text_x = g_msg_text_x + (name_draw_count + 2) * 16 + 8;
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
        draw_string_kanji(g_msg_text_x, message_line2_y, line2, line2_count);
    }
    if (line3_count > 0) {
        draw_string_kanji(g_msg_text_x, message_line3_y, line3, line3_count);
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


    // debug_log("ui_draw_message_jis start count=%d", count);

    for (;;) {
        // debug_log("message loop start start_index=%d count=%d", start_index, count);
        page_count = ui_draw_message_page_jis(name, name_len,
                                              jis_codes, count,
                                              start_index);
                                              
        // debug_log("page_count=%d start_index=%d", page_count, start_index);  
                                            
        if (!input_wait_key()) {
            continue;
        }
        // debug_log("after input_wait_key");

        if (g_request_script_resume) {
            // debug_log("break: g_request_script_resume");
            break;
        }

        if (page_count <= 0) {
            // debug_log("break: page_count <= 0");
            break;
        }

        start_index += page_count;
        if (start_index >= count) {
            // debug_log("start_index updated=%d", start_index);
            break;
        }
    }
    // debug_log("ui_draw_message_jis end");
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

    int choice_line1_y;
    int choice_line2_y;
    int choice_text_x;
    int choice_cursor_x;

    choice_line1_y = g_choice_line1_y;
    choice_line2_y = g_choice_line2_y;
    choice_text_x = g_choice_text_x;
    choice_cursor_x = g_choice_cursor_x;

    /*
     * 選択肢中は右上の別ウィンドウを使わず、
     * 下部メッセージウィンドウをそのまま使います。
     */
    ui_draw_message_window();


    /* 選択中の行に帯を描く */
    if (selected == 1) {
        graph98_boxfill(g_choice_band_x0,
                        g_choice_band1_y0,
                        g_choice_band_x1,
                        g_choice_band1_y1,
                        15);
    }
    if (selected == 2) {
        graph98_boxfill(g_choice_band_x0,
                        g_choice_band2_y0,
                        g_choice_band_x1,
                        g_choice_band2_y1,
                        15);
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

/* 現在の背景＋立ち絵を再描画する関数 */
static void ui_redraw_current_scene_from_state(void)
{
    ui_draw_background(g_state.bg_name);

    ui_draw_current_stands(g_state.left_stand, g_state.left_face,
                           g_state.right_stand, g_state.right_face);
}

/* メッセージ非表示中の待機関数 */
static void ui_hide_message_window_until_resume(void)
{
    uint8_t ch;

    ui_redraw_current_scene_from_state();

    for (;;) {
        if (g_mouse_available && mouse98_left_pressed()) {
            mouse98_wait_left_release();
            break;
        }

        if (!input_key_available()) {
            continue;
        }

        ch = input_read_key();
        if (ch == 0x0D) {
            break;
        }
    }
}


/* マウス（左クリック）、Enterキーが押されるまで待ちます。 */
static int input_wait_key(void)
{
    uint8_t ch;


    // debug_log("input_wait_key start");

    for (;;) {
        if (g_mouse_available && mouse98_left_pressed()) {
            mouse98_wait_left_release();
            return 1;
        }

        if (!input_key_available()) {
            g_save_key_armed = 1;
            g_load_key_armed = 1;
            continue;
        }

        ch = input_read_key();

        if ((ch == 'S' || ch == 's') && g_save_key_armed) {
            handle_save_hotkey(ch);
            g_save_key_armed = 0;
            continue;
        }

        if ((ch == 'L' || ch == 'l') && g_load_key_armed) {
            handle_load_hotkey(ch);
            g_load_key_armed = 0;
            continue;
        }

        if (ch != 'S' && ch != 's') {
            g_save_key_armed = 1;
        }

        if (ch != 'L' && ch != 'l') {
            g_load_key_armed = 1;
        }

        if (ch == 'H' || ch == 'h') {
            ui_hide_message_window_until_resume();
            return 0;
        }

        if (ch == 0x0D) {
            return 1;  /* Enter */
        }
    }
    // debug_log("input_wait_key done");
}

// キーボード、マウス入力監視
static int input_key_available(void)
{
    uint8_t status;

    __asm__ __volatile__(
        "movb $0x0B, %%ah\n\t"
        "int $0x21\n\t"
        "movb %%al, %0"
        : "=rm"(status)
        :
        : "ax", "cc", "memory");

    return status != 0;
}

// キーボード、マウス入力データ読み取り
static uint8_t input_read_key(void)
{
    uint8_t ch;

    __asm__ __volatile__(
        "movb $0x08, %%ah\n\t"
        "int $0x21\n\t"
        "movb %%al, %0"
        : "=rm"(ch)
        :
        : "ax", "cc", "memory");

    return ch;
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
                                    const char *current_bg_name)
{
    uint8_t ch;
    int selected;

    selected = 1;

    ui_draw_background(current_bg_name);
    ui_draw_stands_for_message(msg);

    for (;;) {
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
static int find_label_and_jump(FILE *fp, int *script_line, const char *label_name)
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
    if (script_line != 0) {
        *script_line = 0;
    }

    while (read_script_line(fp, line, sizeof(line), script_line)) {
        remove_newline(line);
        trim_leading_spaces(line);
        
        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (line[0] != '#') {
            continue;
        }

        cmd[0] = '\0';
        arg1[0] = '\0';

        count = sscanf(line, "%31s %63s", cmd, arg1);
        if (count >= 2) {
            if (strcmp(cmd, "#label") == 0 && strcmp(arg1, label_name) == 0) {
                debug_log("SCRIPT JUMP LABEL line=%d label=%s",
                          script_line != 0 ? *script_line : 0,
                          label_name);
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

static GameFlag *find_flag(const char *name)
{
    int i;

    for (i = 0; i < MAX_FLAGS; ++i) {

        if (g_flags[i].name[0] == '\0') {
            continue;
        }

        if (strcmp(g_flags[i].name, name) == 0) {
            return &g_flags[i];
        }
    }

    return 0;
}

static GameFlag *find_or_create_flag(const char *name)
{
    int i;
    GameFlag *flag;

    flag = find_flag(name);

    if (flag != 0) {
        return flag;
    }

    for (i = 0; i < MAX_FLAGS; ++i) {

        if (g_flags[i].name[0] == '\0') {

            strcpy(g_flags[i].name, name);
            g_flags[i].value = 0;

            return &g_flags[i];
        }
    }

    return 0;
}

static void add_flag_value(const char *name, int value)
{
    GameFlag *flag;

    flag = find_or_create_flag(name);

    if (flag != 0) {
        flag->value += value;
    }
}

static void set_flag_value(const char *name, int value)
{
    GameFlag *flag;

    flag = find_or_create_flag(name);

    if (flag != 0) {
        flag->value = value;
    }
}

static int get_flag_value(const char *name)
{
    GameFlag *flag;

    flag = find_flag(name);

    if (flag == 0) {
        return 0;
    }

    return flag->value;
}


static void debug_log(const char *fmt, ...)
{
    FILE *fp;
    va_list args;

    fp = fopen("debug.txt", "a");

    if (fp == 0) {
        return;
    }

    va_start(args, fmt);

    vfprintf(fp, fmt, args);

    fprintf(fp, "\n");

    va_end(args);

    fclose(fp);
}

static int read_script_line(FILE *fp, char *line, int line_size, int *script_line)
{
    if (fgets(line, line_size, fp) == 0) {
        return 0;
    }

    if (script_line != 0) {
        (*script_line)++;
        g_state.script_line = *script_line;
    }

    return 1;
}

static int save_game_state(void)
{
    FILE *fp;
    SaveData save_data;

    memset(&save_data, 0, sizeof(save_data));
    memcpy(save_data.magic, "ADV98SAV", 8);
    save_data.version = SAVE_VERSION;
    save_data.state = g_state;
    memcpy(save_data.flags, g_flags, sizeof(g_flags));

    fp = fopen("SAVE.DAT", "wb");
    if (fp == 0) {
        debug_log("SAVE FAILED open file=SAVE.DAT");
        return 0;
    }

    if (fwrite(&save_data, sizeof(save_data), 1, fp) != 1) {
        debug_log("SAVE FAILED write file=SAVE.DAT");
        fclose(fp);
        return 0;
    }

    fclose(fp);

    debug_log("SAVE OK file=SAVE.DAT version=%d line=%d",
              save_data.version,
              save_data.state.script_line);

    return 1;
}

static int load_game_state(void)
{
    FILE *fp;
    SaveData save_data;

    memset(&save_data, 0, sizeof(save_data));

    fp = fopen("SAVE.DAT", "rb");
    if (fp == 0) {
        debug_log("LOAD FAILED open file=SAVE.DAT");
        return 0;
    }

    if (fread(&save_data, sizeof(save_data), 1, fp) != 1) {
        debug_log("LOAD FAILED read file=SAVE.DAT");
        fclose(fp);
        return 0;
    }

    fclose(fp);

    if (memcmp(save_data.magic, "ADV98SAV", 8) != 0) {
        debug_log("LOAD FAILED bad magic");
        return 0;
    }

    if (save_data.version != SAVE_VERSION) {
        debug_log("LOAD FAILED bad version=%d expected=%d",
                  save_data.version,
                  SAVE_VERSION);
        return 0;
    }

    g_state = save_data.state;
    memcpy(g_flags, save_data.flags, sizeof(g_flags));

    debug_log("LOAD OK file=SAVE.DAT version=%d line=%d",
              save_data.version,
              save_data.state.script_line);

    return 1;
}

static void resume_script_line(FILE *fp, int *script_line,
                               char *current_name, int current_name_size)
{
    char line[256];
    int target_line;

    if (fp == 0 || script_line == 0 || current_name == 0 || current_name_size <= 0) {
        return;
    }

    target_line = g_state.script_line;

    fseek(fp, 0L, SEEK_SET);
    *script_line = 0;
    current_name[0] = '\0';

    while (*script_line < target_line - 1) {
        if (!read_script_line(fp, line, sizeof(line), script_line)) {
            break;
        }

        remove_newline(line);
        trim_leading_spaces(line);

        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (line[0] == '[') {
            extract_name_from_brackets(line, current_name, current_name_size);
        }
    }

    debug_log("LOAD RESUME line=%d", target_line);
}

static void handle_save_hotkey(uint8_t ch)
{
    (void)ch;
    save_game_state();
}

static void resume_bgm_after_load(void)
{
    if (!g_pmd_available) {
        return;
    }

    if (g_state.bgm[0] == '\0') {
        return;
    }

    pmd_stop_music();

    if (pmd_load_music_file(g_state.bgm)) {
        pmd_start_music();
        debug_log("BGM resumed after load: %s", g_state.bgm);
    } else {
        debug_log("BGM resume failed after load: %s", g_state.bgm);
    }
}

static void handle_load_hotkey(uint8_t ch)
{
    (void)ch;

    if (load_game_state()) {
        resume_bgm_after_load();

        g_request_scene_redraw = 1;
        g_request_script_resume = 1;
    }
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
    int script_line;

    uint16_t name_jis[64];
    uint16_t text_jis[128];

    int name_len;
    int text_len;

    char last_bg_name[32];
    enum StandId last_left_stand;
    enum StandId last_right_stand;
    enum FaceId last_left_face;
    enum FaceId last_right_face;
    int scene_dirty;

    int stand_dirty;
    int bg_wipe_pending;
    int left_wipe_pending;
    int right_wipe_pending;
    
    
    stand_dirty = 0;
    bg_wipe_pending = 0;
    left_wipe_pending = 0;
    right_wipe_pending = 0;
    script_line = 0;

    current_name[0] = '\0';

    g_state.bg_name[0] = '\0';
    g_state.script_line = 0;
    g_state.left_stand = STAND_NONE;
    g_state.right_stand = STAND_NONE;
    g_state.left_face = FACE_NORMAL;
    g_state.right_face = FACE_NORMAL;
    g_state.bgm[0] = '\0';

    last_bg_name[0] = '\0';
    last_left_stand = STAND_NONE;
    last_right_stand = STAND_NONE;
    last_left_face = FACE_NORMAL;
    last_right_face = FACE_NORMAL;
    scene_dirty = 1;

    fp = fopen("script.txt", "rb");
    if (fp == 0) {
        debug_log("script.txt not found.");
        return;
    }

    for (;;) {
        if (g_request_script_resume) {
            resume_script_line(fp, &script_line,
                               current_name, sizeof(current_name));
            scene_dirty = 1;
            stand_dirty = 0;
            bg_wipe_pending = 0;
            left_wipe_pending = 0;
            right_wipe_pending = 0;
            g_request_scene_redraw = 0;
            g_request_script_resume = 0;
        }

        if (!read_script_line(fp, line, sizeof(line), &script_line)) {
            break;
        }

        g_state.script_line = script_line;
        remove_newline(line);
        trim_leading_spaces(line);
        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (line[0] == '#') {
            char cmd[32];
            char arg1[64];
            char arg2[64];
            char arg3[64];
            int count;

            cmd[0] = '\0';
            arg1[0] = '\0';
            arg2[0] = '\0';
            arg3[0] = '\0';

            count = sscanf(line, "%31s %63s %63s %63s",
                           cmd, arg1, arg2, arg3);

            if (strcmp(cmd, "#choice") == 0) {
                if (count >= 3) {
                    handle_choice_block(fp, &script_line, arg1, arg2);
                }
                continue;
            }

            if (strcmp(cmd, "#label") == 0) {
                continue;
            }

            if (strcmp(cmd, "#jump") == 0) {
                if (count >= 2) {
                    find_label_and_jump(fp, &script_line, arg1);
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
                        find_label_and_jump(fp, &script_line, arg2);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#ifnot") == 0) {
                if (count >= 3) {
                    if (!is_flag_on(arg1)) {
                        find_label_and_jump(fp, &script_line, arg2);
                    }
                }
                continue;
            }

            if (strcmp(cmd,"#add") == 0) {
                if (count >= 3) {
                    add_flag_value(arg1, atoi(arg2));
                }
                continue;
            }

            if (strcmp(cmd, "#setnum") == 0) {
                if (count >= 3) {
                    set_flag_value(arg1, atoi(arg2));
                }
                continue;
            }

            if (strcmp(cmd, "#ifge") == 0) {
                if (count >= 4) {
                    if (get_flag_value(arg1) >= atoi(arg2)) {
                        find_label_and_jump(fp, &script_line, arg3);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#ifeq") == 0) {
                if (count >= 4) {
                    if (get_flag_value(arg1) == atoi(arg2)) {
                        find_label_and_jump(fp, &script_line, arg3);
                    }
                }
                continue;
            }
     
            if (strcmp(cmd, "#pal") == 0) {
                if (count >= 2) {
                    // printf("[PAL] load: %s\n", arg1);
                    if (graph98_load_palette_file(arg1)) {
                        // printf("[PAL] success\n");
                        scene_dirty = 1;
                    } else {
                        debug_log("palette load failed: %s", arg1);
                        // printf("[PAL] FAILED\n");
                    }
                } else {
                    // printf("[PAL] invalid args\n");
                }
                continue;
            }

            // #seはコメントアウト
            // BGM/SEはPMD(#bgm)に任せるため
            /*
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
            */
            // 未知コマンド扱いで下に流れる可能性があるので、一応#seを無視する処理は入れておく
            if (strcmp(cmd, "#se") == 0) {
                continue;
            }

            if (strcmp(cmd, "#bgm") == 0) {
                if (count >= 2) {

                    if (strcmp(g_state.bgm, arg1) == 0) {
                        continue;
                    }

                    if (g_pmd_available) {

                        pmd_stop_music();

                        if (pmd_load_music_file(arg1)) {

                            strcpy(g_state.bgm, arg1);

                            pmd_start_music();
                        }else{
                            debug_log("bgm load failed: %s", arg1);
                        }
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#bgmstart") == 0) {
                if (g_pmd_available) {
                    pmd_start_music();
                }
                continue;
            }

            if (strcmp(cmd, "#bgmstop") == 0) {
                if (g_pmd_available) {
                    pmd_stop_music();
                }
                continue;
            }

            if (strcmp(cmd, "#bgmfade") == 0) {
                if (g_pmd_available) {
                    pmd_fadeout_music();
                }
                continue;
            }

            {
                char old_bg_name[32];
                enum StandId old_left_stand;
                enum FaceId old_left_face;
                enum StandId old_right_stand;
                enum FaceId old_right_face;

                strncpy(old_bg_name, g_state.bg_name, sizeof(old_bg_name) - 1);
                old_bg_name[sizeof(old_bg_name) - 1] = '\0';
                old_left_stand = g_state.left_stand;
                old_left_face = g_state.left_face;
                old_right_stand = g_state.right_stand;
                old_right_face = g_state.right_face;

                if (count >= 2) {
                    if (strcmp(cmd, "#bgwipe") == 0) {
                        bg_wipe_pending = 1;
                    } else if (strcmp(cmd, "#bg") == 0) {
                        bg_wipe_pending = 0;
                    } else if (strcmp(cmd, "#leftwipe") == 0) {
                        left_wipe_pending = 1;
                    } else if (strcmp(cmd, "#left") == 0) {
                        left_wipe_pending = 0;
                    } else if (strcmp(cmd, "#rightwipe") == 0) {
                        right_wipe_pending = 1;
                    } else if (strcmp(cmd, "#right") == 0) {
                        right_wipe_pending = 0;
                    }
                }

                process_command_line(line,
                                     g_state.bg_name,
                                     &g_state.left_stand,
                                     &g_state.left_face,
                                     &g_state.right_stand,
                                     &g_state.right_face);

                if (bg_wipe_pending || strcmp(g_state.bg_name, old_bg_name) != 0) {
                    scene_dirty = 1;
                    stand_dirty = 0;
                } else if (g_state.left_stand != old_left_stand ||
                           g_state.left_face != old_left_face ||
                           g_state.right_stand != old_right_stand ||
                           g_state.right_face != old_right_face ||
                           left_wipe_pending ||
                           right_wipe_pending) {
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

        /* script.txt 末尾の空行で空メッセージが出るのを防ぐ */
        if (text_len <= 0) {
            continue;
        }
        
        // 変化があれば部分的に再描画
        if (scene_dirty || strcmp(last_bg_name, g_state.bg_name) != 0) {

            if (bg_wipe_pending) {
                ui_draw_background_center_wipe(g_state.bg_name);
            } else {
                ui_draw_background(g_state.bg_name);
            }
            if (left_wipe_pending) {
                ui_draw_stand_center_wipe(g_state.left_stand, g_state.left_face,
                                          STAND_LEFT_X, STAND_Y, 0);
            } else {
                ui_draw_stand(g_state.left_stand, g_state.left_face,
                              STAND_LEFT_X, STAND_Y, 0);
            }
            if (right_wipe_pending) {
                ui_draw_stand_center_wipe(g_state.right_stand, g_state.right_face,
                                          STAND_RIGHT_X, STAND_Y, 1);
            } else {
                ui_draw_stand(g_state.right_stand, g_state.right_face,
                              STAND_RIGHT_X, STAND_Y, 1);
            }

            strncpy(last_bg_name, g_state.bg_name, sizeof(last_bg_name) - 1);
            last_bg_name[sizeof(last_bg_name) - 1] = '\0';
            last_left_stand = g_state.left_stand;
            last_left_face = g_state.left_face;
            last_right_stand = g_state.right_stand;
            last_right_face = g_state.right_face;

            scene_dirty = 0;
            stand_dirty = 0;
            bg_wipe_pending = 0;
            left_wipe_pending = 0;
            right_wipe_pending = 0;

        } else if (stand_dirty) {

            /*
             * 左もしくは右立ち絵だけ部分更新する。
             * どっちの条件に引っかからなかった場合は、安全のため全体再描画に戻す。
             */
            if ((g_state.left_stand != last_left_stand ||
                g_state.left_face != last_left_face) &&
                g_state.right_stand == last_right_stand &&
                g_state.right_face == last_right_face) {

                // debug_log("LEFT STAND PARTIAL UPDATE");

                if (left_wipe_pending) {
                    ui_refresh_left_stand_only_wipe(g_state.bg_name,
                                                    g_state.left_stand,
                                                    g_state.left_face);
                } else {
                    ui_refresh_left_stand_only(g_state.bg_name,
                                               g_state.left_stand,
                                               g_state.left_face);
                }

                last_left_stand = g_state.left_stand;
                last_left_face = g_state.left_face;

            } else if (g_state.left_stand == last_left_stand &&
                       g_state.left_face == last_left_face &&
                       (g_state.right_stand != last_right_stand ||
                        g_state.right_face != last_right_face)) {

                // debug_log("RIGHT STAND PARTIAL UPDATE");

                if (right_wipe_pending) {
                    ui_refresh_right_stand_only_wipe(g_state.bg_name,
                                                     g_state.right_stand,
                                                     g_state.right_face);
                } else {
                    ui_refresh_right_stand_only(g_state.bg_name,
                                                g_state.right_stand,
                                                g_state.right_face);
                }

                last_right_stand = g_state.right_stand;
                last_right_face = g_state.right_face;

            } else {

                // debug_log("STAND FULL REDRAW");


                ui_draw_background(g_state.bg_name);
                if (left_wipe_pending) {
                    ui_draw_stand_center_wipe(g_state.left_stand, g_state.left_face,
                                              STAND_LEFT_X, STAND_Y, 0);
                } else {
                    ui_draw_stand(g_state.left_stand, g_state.left_face,
                                  STAND_LEFT_X, STAND_Y, 0);
                }
                if (right_wipe_pending) {
                    ui_draw_stand_center_wipe(g_state.right_stand, g_state.right_face,
                                              STAND_RIGHT_X, STAND_Y, 1);
                } else {
                    ui_draw_stand(g_state.right_stand, g_state.right_face,
                                  STAND_RIGHT_X, STAND_Y, 1);
                }

                strncpy(last_bg_name, g_state.bg_name, sizeof(last_bg_name) - 1);
                last_bg_name[sizeof(last_bg_name) - 1] = '\0';
                last_left_stand = g_state.left_stand;
                last_left_face = g_state.left_face;
                last_right_stand = g_state.right_stand;
                last_right_face = g_state.right_face;
            }

        stand_dirty = 0;
        left_wipe_pending = 0;
        right_wipe_pending = 0;
        }
        ui_draw_message_jis(name_jis, name_len, text_jis, text_len);

        if (g_request_scene_redraw) {
            scene_dirty = 1;
            stand_dirty = 0;
            g_request_scene_redraw = 0;
        }
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
        trim_leading_spaces(line);
        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
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
        ui_draw_background("bg001");

        /* ここはとりあえず固定立ち絵でもOK */
        ui_draw_stand(STAND_CHARACTER01, FACE_NORMAL, 60, STAND_Y, 0);

        ui_draw_message_ascii(current_name, line);

        if (!input_wait_key()) {
            continue;
        }
    }

    fclose(fp);
}


// 文字列を立ち絵IDへ
static enum StandId parse_stand_id(const char *name)
{
    int stand_no;

    if (strcmp(name, "none") == 0) {
        return STAND_NONE;
    }

    if (strncmp(name, "character", 9) == 0) {
        if (sscanf(name + 9, "%d", &stand_no) == 1) {
            if (stand_no >= 1 && stand_no <= 20) {
                return (enum StandId)stand_no;
            }
        }
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

// コマンド行を読む関数
static void process_command_line(const char *line,
                                 char *bg_name,
                                 enum StandId *left_stand,
                                 enum FaceId *left_face,
                                 enum StandId *right_stand,
                                 enum FaceId *right_face)
{
    char cmd[32];
    char arg1[32];
    char arg2[32];
    char arg3[32];
    char arg4[32];
    int count;

    cmd[0] = '\0';
    arg1[0] = '\0';
    arg2[0] = '\0';
    arg3[0] = '\0';
    arg4[0] = '\0';

    count = sscanf(line,
                   "%31s %31s %31s %31s %31s",
                   cmd,
                   arg1,
                   arg2,
                   arg3,
                   arg4);

    if (count <= 0) {
        return;
    }

    if (strcmp(cmd, "#bg") == 0 || strcmp(cmd, "#bgwipe") == 0) {
        if (count >= 2) {
            strncpy(bg_name, arg1, 31);
            bg_name[31] = '\0';
        }
        return;
    }

    if (strcmp(cmd, "#msgbox") == 0) {
        int x0;
        int y0;
        int x1;
        int y1;

        if (count >= 5) {
            x0 = atoi(arg1);
            y0 = atoi(arg2);
            x1 = atoi(arg3);
            y1 = atoi(arg4);

            ui_set_message_box(x0, y0, x1, y1);
        }
        return;
    }



    if (strcmp(cmd, "#left") == 0 || strcmp(cmd, "#leftwipe") == 0) {
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

    if (strcmp(cmd, "#right") == 0 || strcmp(cmd, "#rightwipe") == 0) {
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
static void handle_choice_block(FILE *fp, int *script_line,
                                const char *label1, const char *label2)
{
    char line1[256];
    char line2[256];

    uint16_t choice1_jis[64];
    uint16_t choice2_jis[64];

    int choice1_len;
    int choice2_len;
    int selected;

    if (!read_script_line(fp, line1, sizeof(line1), script_line)) {
        return;
    }
    if (!read_script_line(fp, line2, sizeof(line2), script_line)) {
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
        find_label_and_jump(fp, script_line, label1);
    } else {
        find_label_and_jump(fp, script_line, label2);
    }
}

int main(void)
{

        remove("debug.txt");
        debug_log("ADV98 START");

    // PMD常駐確認
    g_pmd_available = pmd_is_resident();

    if (!g_pmd_available) {
        debug_log("PMD.COM is not resident.");
        debug_log("BGM commands will be ignored.");
    }



    // マウス常駐確認
    g_mouse_available = mouse98_init();
    if (!g_mouse_available) {
        debug_log("Mouse driver is not resident.");
        debug_log("Mouse input will be disabled.");
    }

    text98_clear_screen();
    text98_hide_cursor();

    graph98_init();

    // 86音源初期化はコメントアウト
    // BGMはPMDに任せるため
    // se86_init();

    // パレット読み込み
    if (!graph98_load_palette_file("adv.pal")) {
        debug_log("adv.pal not found. Using built-in palette.");
        graph98_apply_adv_palette();
    }


    run_script_sjis();


    graph98_clear(0);
    text98_clear_screen();
    debug_log("ADV98 END");
    printf("ADV98.EXE finished.\n");

    return 0;
}
