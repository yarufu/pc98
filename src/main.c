#include "graph98.h"
#include "fm86.h"
#include "pmd.h"
#include "mouse98.h"
#include "text98.h"
#include "title.h"
#include "save.h"
#include "script.h"
#include "debug.h"
#include "game_state.h"
#include "menu.h"
#include "input.h"


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// キャラの位置調整
#define STAND_LEFT_X   60
#define STAND_RIGHT_X  320
#define STAND_Y        9

#define STAND_W        256
#define STAND_H        290

/* 背景ワイプ速度。16 または 24 程度を推奨。 */
#define BG_WIPE_STEP_LINES 8


struct Message;

#define MAX_CHOICE_ITEMS 6
#define MAX_CHOICE_CHARS 64
#define MAX_CHOICE_DRAW_CHARS 8
#define CHOICE_COLUMNS 2

static GameFlag g_flags[MAX_FLAGS];
static GameState g_state;
static int g_pmd_available = 0;
static int g_fm_se_loaded = 0;

// マウス制御
static int g_mouse_available = 0;
static int g_request_scene_redraw = 0;
static int g_request_script_resume = 0;
static enum SystemAction g_system_action = SYSTEM_ACTION_NONE;

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

static int g_choice_text_x[MAX_CHOICE_ITEMS] = {160, 340, 160, 340, 160, 340};
static int g_choice_text_y[MAX_CHOICE_ITEMS] = {317, 317, 339, 339, 361, 361};
static int g_choice_band_x0[MAX_CHOICE_ITEMS] = {140, 320, 140, 320, 140, 320};
static int g_choice_band_x1[MAX_CHOICE_ITEMS] = {319, 500, 319, 500, 319, 500};
static int g_choice_band_y0[MAX_CHOICE_ITEMS] = {313, 313, 335, 335, 357, 357};
static int g_choice_band_y1[MAX_CHOICE_ITEMS] = {332, 332, 354, 354, 376, 376};
static uint16_t g_choice_work_jis[MAX_CHOICE_ITEMS][MAX_CHOICE_CHARS];
static int g_choice_work_lens[MAX_CHOICE_ITEMS];
static uint16_t g_choice_saved_jis[MAX_CHOICE_ITEMS][MAX_CHOICE_CHARS];
static int g_choice_saved_lens[MAX_CHOICE_ITEMS];

/* 関数宣言部 */
static void ui_redraw_current_scene_from_state(void);
static void ui_hide_message_window_until_resume(void);
static void ui_draw_wait_mark(int x, int y, unsigned char color);
static void text98_hide_cursor(void);
static void text98_clear_screen(void);
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
static void ui_draw_stand_interlace(enum StandId stand_id, enum FaceId face_id,
                                    int x, int y, int facing_left);
static void ui_draw_stands_for_message(const struct Message *msg);
static void ui_draw_cursor_triangle(int x, int y, unsigned char color);
static int ui_draw_message_page_jis(const uint16_t *name, int name_len,
                                    const uint16_t *jis_codes, int count,
                                    int start_index);
static void ui_draw_message_jis(const uint16_t *name, int name_len,
                                const uint16_t *jis_codes, int count);
static void ui_draw_current_stands(enum StandId left_stand,
                                   enum FaceId left_face,
                                   enum StandId right_stand,
                                   enum FaceId right_face);
static void reset_choice_lines(void);
static void store_choice_line(int index, const char *line) __attribute__((noinline));
static void trim_leading_spaces(char *str);
static void ui_restore_stand_background_rect(int x0, int y0, int x1, int y1, const char *bg_name);
static void ui_refresh_left_stand_only(const char *bg_name,
                                       enum StandId left_stand,
                                       enum FaceId left_face);
static void ui_refresh_left_stand_only_wipe(const char *bg_name,
                                            enum StandId left_stand,
                                            enum FaceId left_face);
static void ui_refresh_left_stand_only_interlace(const char *bg_name,
                                                 enum StandId left_stand,
                                                 enum FaceId left_face);
static void ui_refresh_right_stand_only(const char *bg_name,
                                        enum StandId right_stand,
                                        enum FaceId right_face);
static void ui_refresh_right_stand_only_wipe(const char *bg_name,
                                             enum StandId right_stand,
                                             enum FaceId right_face);
static void ui_refresh_right_stand_only_interlace(const char *bg_name,
                                                  enum StandId right_stand,
                                                  enum FaceId right_face);

static void restore_palette_after_load(void);
static void restore_scene_after_load(void);
static void request_loaded_game_resume(void);
static void app_cleanup(void);


static void ui_draw_wait_mark(int x, int y, unsigned char color)
{
    graph98_hline(x + 0, x + 8, y + 0, color);
    graph98_hline(x + 1, x + 7, y + 1, color);
    graph98_hline(x + 2, x + 6, y + 2, color);
    graph98_hline(x + 3, x + 5, y + 3, color);
    graph98_hline(x + 4, x + 4, y + 4, color);
}

static void ui_draw_choice_jis(int choice_count, int selected);


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
    int i;
    int row;
    int col;
    int band_x0;
    int band_x1;
    int cell_w;
    int row_step;
    int band_h;

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

    band_x0 = g_msgbox_x0 + 31;
    band_x1 = g_msgbox_x1 - 31;
    cell_w = (band_x1 - band_x0 + 1) / CHOICE_COLUMNS;
    row_step = 22;
    band_h = 19;

    for (i = 0; i < MAX_CHOICE_ITEMS; ++i) {
        row = i / CHOICE_COLUMNS;
        col = i % CHOICE_COLUMNS;

        g_choice_band_x0[i] = band_x0 + col * cell_w;
        if (col == CHOICE_COLUMNS - 1) {
            g_choice_band_x1[i] = band_x1;
        } else {
            g_choice_band_x1[i] = g_choice_band_x0[i] + cell_w - 1;
        }

        g_choice_band_y0[i] = g_msgbox_y0 + row * row_step;
        g_choice_band_y1[i] = g_choice_band_y0[i] + band_h;
        g_choice_text_x[i] = g_choice_band_x0[i] + 20;
        g_choice_text_y[i] = g_choice_band_y0[i] + 4;
    }
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

static void ui_refresh_left_stand_only_interlace(const char *bg_name,
                                                 enum StandId left_stand,
                                                 enum FaceId left_face)
{
    ui_restore_stand_background_rect(STAND_LEFT_X,
                                     STAND_Y,
                                     STAND_LEFT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_name);
    ui_draw_stand_interlace(left_stand, left_face, STAND_LEFT_X, STAND_Y, 0);
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

static void ui_refresh_right_stand_only_interlace(const char *bg_name,
                                                  enum StandId right_stand,
                                                  enum FaceId right_face)
{
    ui_restore_stand_background_rect(STAND_RIGHT_X,
                                     STAND_Y,
                                     STAND_RIGHT_X + STAND_W - 1,
                                     STAND_Y + STAND_H - 1,
                                     bg_name);
    ui_draw_stand_interlace(right_stand, right_face, STAND_RIGHT_X, STAND_Y, 1);
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

static void ui_draw_stand_interlace(enum StandId stand_id, enum FaceId face_id,
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

    if (!graph98_draw_sprite_file_trans_interlace(sprite_path, x, y, 0)) {
        debug_log("sprite interlace load failed: %s", sprite_path);
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

        draw_jis_string(g_msg_text_x, message_line1_y, name_text, name_draw_count + 2);

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
        draw_jis_string(text_x, message_line1_y, line1, line1_count);
    }
    if (line2_count > 0) {
        draw_jis_string(g_msg_text_x, message_line2_y, line2, line2_count);
    }
    if (line3_count > 0) {
        draw_jis_string(g_msg_text_x, message_line3_y, line3, line3_count);
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

        if (g_system_action != SYSTEM_ACTION_NONE) {
            break;
        }

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

static void ui_draw_choice_jis(int choice_count, int selected)
{
    static unsigned char choice_fonts[MAX_CHOICE_ITEMS][MAX_CHOICE_DRAW_CHARS][32];
    static const unsigned char *line_fonts[MAX_CHOICE_DRAW_CHARS];
    int i;
    int j;
    int draw_count;

    ui_draw_message_window();

    if (selected >= 1 && selected <= choice_count) {
        i = selected - 1;
        graph98_boxfill(g_choice_band_x0[i],
                        g_choice_band_y0[i],
                        g_choice_band_x1[i],
                        g_choice_band_y1[i],
                        15);
    }

    for (i = 0; i < choice_count && i < MAX_CHOICE_ITEMS; ++i) {
        draw_count = g_choice_work_lens[i];
        if (draw_count > MAX_CHOICE_DRAW_CHARS) {
            draw_count = MAX_CHOICE_DRAW_CHARS;
        }

        for (j = 0; j < draw_count; ++j) {
            get_kanji_font(g_choice_work_jis[i][j], choice_fonts[i][j]);
            line_fonts[j] = choice_fonts[i][j];
        }

        if (draw_count > 0) {
            draw_jis_string(g_choice_text_x[i],
                            g_choice_text_y[i],
                            line_fonts,
                            draw_count);
        }
    }
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

        ch = input_read_key(0);
        if (ch == 0x0D) {
            break;
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

static void restore_palette_after_load(void)
{
    FILE *fp;
    char line[256];
    char palette_path[64];
    char cmd[32];
    char arg1[64];
    int line_number;

    strcpy(palette_path, "adv.pal");

    fp = fopen("script.txt", "rb");
    if (fp != 0) {
        line_number = 0;

        while (line_number < g_state.script_line &&
               fgets(line, sizeof(line), fp) != 0) {
            line_number++;
            remove_newline(line);
            trim_leading_spaces(line);

            cmd[0] = '\0';
            arg1[0] = '\0';

            if (sscanf(line, "%31s %63s", cmd, arg1) >= 2 &&
                strcmp(cmd, "#pal") == 0) {
                strcpy(palette_path, arg1);
            }
        }

        fclose(fp);
    } else {
        debug_log("palette restore failed: script.txt not found");
    }

    if (!graph98_load_palette_file(palette_path)) {
        debug_log("palette restore failed: %s", palette_path);

        if (strcmp(palette_path, "adv.pal") != 0 &&
            graph98_load_palette_file("adv.pal")) {
            return;
        }

        graph98_apply_adv_palette();
    }
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

static void restore_scene_after_load(void)
{
    restore_palette_after_load();
    ui_redraw_current_scene_from_state();
    resume_bgm_after_load();
}

static void request_loaded_game_resume(void)
{
    g_request_scene_redraw = 1;
    g_request_script_resume = 1;
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


// 立ち絵を今の状態で描く関数
static void ui_draw_current_stands(enum StandId left_stand,
                                   enum FaceId left_face,
                                   enum StandId right_stand,
                                   enum FaceId right_face)
{
    ui_draw_stand(left_stand, left_face, STAND_LEFT_X, STAND_Y, 0);
    ui_draw_stand(right_stand, right_face, STAND_RIGHT_X, STAND_Y, 1);
}

static void app_cleanup(void)
{
    if (g_pmd_available) {
        pmd_stop_music();
    }

    graph98_clear(0);
    text98_clear_screen();
    debug_log("ADV98 END");
    printf("ADV98.EXE finished.\n");
}


static void reset_choice_lines(void)
{
    int i;

    for (i = 0; i < MAX_CHOICE_ITEMS; ++i) {
        g_choice_work_lens[i] = 0;
    }
}

static void store_choice_line(int index, const char *line)
{
    if (index < 0 || index >= MAX_CHOICE_ITEMS) {
        return;
    }

    g_choice_work_lens[index] = convert_sjis_string_to_jis_array(
        (const unsigned char *)line,
        g_choice_work_jis[index],
        MAX_CHOICE_CHARS
    );
}

int main(void)
{
    enum GameResult game_result;
    TitleContext title_context;
    ScriptContext script_context;
    MenuContext menu_context;
    InputContext input_context;

        debug_log_init();
        debug_log("ADV98 START");

    // PMD常駐確認
    g_pmd_available = pmd_is_resident();

    if (!g_pmd_available) {
        debug_log("PMD.COM is not resident.");
        debug_log("BGM commands will be ignored.");
    } else {
        g_fm_se_loaded = pmd_load_fm_se_file("SE.EFC");
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

    menu_context.state = &g_state;
    menu_context.flags = g_flags;
    menu_context.pmd_available = &g_pmd_available;
    menu_context.fm_se_loaded = &g_fm_se_loaded;
    menu_context.mouse_available = &g_mouse_available;
    menu_context.request_script_resume = &g_request_script_resume;
    menu_context.system_action = &g_system_action;
    menu_context.choice_work_jis = (uint16_t *)g_choice_work_jis;
    menu_context.choice_saved_jis = (uint16_t *)g_choice_saved_jis;
    menu_context.choice_jis_bytes = sizeof(g_choice_work_jis);
    menu_context.choice_work_lens = g_choice_work_lens;
    menu_context.choice_saved_lens = g_choice_saved_lens;
    menu_context.choice_lens_bytes = sizeof(g_choice_work_lens);
    menu_context.reset_choice_lines = reset_choice_lines;
    menu_context.store_choice_line = store_choice_line;
    menu_context.wait_choice = input_wait_choice_jis;
    menu_context.draw_choice_jis = ui_draw_choice_jis;
    menu_context.redraw_current_scene_from_state =
        ui_redraw_current_scene_from_state;
    menu_context.restore_scene_after_load = restore_scene_after_load;
    menu_context.request_loaded_game_resume = request_loaded_game_resume;
    menu_init(&menu_context);

    input_context.mouse_available = &g_mouse_available;
    input_context.hide_message_window_until_resume =
        ui_hide_message_window_until_resume;
    input_context.draw_choice_jis = ui_draw_choice_jis;
    input_context.open_system_menu = open_system_menu;
    input_context.open_system_menu_from_choice = open_system_menu_from_choice;
    input_init(&input_context);

    for (;;) {
        g_system_action = SYSTEM_ACTION_NONE;

        title_context.pmd_available = g_pmd_available;
        title_context.fm_se_loaded = g_fm_se_loaded;
        title_context.show_selection_menu = show_selection_menu;
        title_context.show_load_menu = show_load_menu;
        title_context.restore_scene_after_load = restore_scene_after_load;
        title_context.request_loaded_game_resume = request_loaded_game_resume;
        title_context.draw_message_window = ui_draw_message_window;

        if (!show_title_menu(&title_context)) {
            break;
        }

        script_context.state = &g_state;
        script_context.flags = g_flags;
        script_context.pmd_available = g_pmd_available;
        script_context.fm_se_loaded = g_fm_se_loaded;
        script_context.request_scene_redraw = &g_request_scene_redraw;
        script_context.request_script_resume = &g_request_script_resume;
        script_context.system_action = &g_system_action;
        script_context.set_message_box = ui_set_message_box;
        script_context.draw_background = ui_draw_background;
        script_context.draw_background_center_wipe = ui_draw_background_center_wipe;
        script_context.draw_stand = ui_draw_stand;
        script_context.draw_stand_center_wipe = ui_draw_stand_center_wipe;
        script_context.draw_stand_interlace = ui_draw_stand_interlace;
        script_context.refresh_left_stand_only = ui_refresh_left_stand_only;
        script_context.refresh_left_stand_only_wipe = ui_refresh_left_stand_only_wipe;
        script_context.refresh_left_stand_only_interlace =
            ui_refresh_left_stand_only_interlace;
        script_context.refresh_right_stand_only = ui_refresh_right_stand_only;
        script_context.refresh_right_stand_only_wipe = ui_refresh_right_stand_only_wipe;
        script_context.refresh_right_stand_only_interlace =
            ui_refresh_right_stand_only_interlace;
        script_context.draw_message_jis = ui_draw_message_jis;
        script_context.reset_choice_lines = reset_choice_lines;
        script_context.store_choice_line = store_choice_line;
        script_context.wait_choice = input_wait_choice_jis;

        game_result = run_script_sjis(&script_context);

        if (game_result == GAME_RESULT_SCRIPT_END ||
            game_result == GAME_RESULT_RETURN_TO_TITLE) {
            continue;
        }

        if (game_result == GAME_RESULT_EXIT_TO_DOS) {
            break;
        }
    }


    app_cleanup();

    return 0;
}
