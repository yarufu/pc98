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
#define STAND_LEFT_X   64
#define STAND_RIGHT_X  320
#define STAND_Y        9
#define STAND_RIGHT_EDGE 575
#define STAND_BOTTOM_Y   298

#define MAX_CHOICE_ITEMS 6
#define MAX_CHOICE_CHARS 64
#define MAX_CHOICE_DRAW_CHARS 8
#define CHOICE_COLUMNS 2

#define STATUS_X             549
#define STATUS_LEFT_X         13
#define STATUS_WEEKDAY_X      29
#define STATUS_TIME_Y        326
#define STATUS_MONEY_Y       354
#define STATUS_CHAR_WIDTH     16
#define STATUS_CHAR_HEIGHT    16
#define STATUS_CHAR_COUNT      5
#define STATUS_X1 (STATUS_X + STATUS_CHAR_WIDTH * STATUS_CHAR_COUNT - 1)
#define STATUS_LEFT_X1 \
    (STATUS_LEFT_X + STATUS_CHAR_WIDTH * STATUS_CHAR_COUNT - 1)
#define STATUS_WEEKDAY_X1 \
    (STATUS_WEEKDAY_X + STATUS_CHAR_WIDTH * 3 - 1)
#define STATUS_TIME_Y1 (STATUS_TIME_Y + STATUS_CHAR_HEIGHT - 1)
#define STATUS_MONEY_Y1 (STATUS_MONEY_Y + STATUS_CHAR_HEIGHT - 1)
#define STATUS_PANEL_COLOR    15
#define STATUS_SPRITE_FILE "STATUS.SPR"
#define STATUS_DIGIT_JIS  0x2330
#define STATUS_COLON_JIS  0x2127
#define STATUS_SLASH_JIS  0x213F
#define STATUS_YOU_JIS    0x4D4B

static const uint16_t g_status_weekday_jis[7] = {
    0x467C, 0x376E, 0x3250, 0x3F65, 0x4C5A, 0x3662, 0x455A
};

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
static void ui_redraw_current_scene_vram_from_state(void);
static void ui_hide_message_window_until_resume(void);
static void text98_hide_cursor(void);
static void text98_clear_screen(void);
static void ui_draw_background(const char *bg_file);
static void ui_draw_background_interlace(const char *bg_file);
static int ui_draw_current_scene_vram(const char *bg_file,
                                      const char *left_sprite,
                                      const char *right_sprite);
static void ui_draw_message_window(void);
static void ui_set_message_box(int x0, int y0, int x1, int y1);
static int ui_get_message_line_chars(void);
static void ui_refresh_status_ui(int erase);
static void ui_draw_stand(const char *sprite_file, int x, int y);
static int ui_draw_message_page_jis(const uint16_t *name, int name_len,
                                    const uint16_t *jis_codes, int count,
                                    int start_index);
static void ui_draw_message_jis(const uint16_t *name, int name_len,
                                const uint16_t *jis_codes, int count);
static void ui_draw_current_stands(const char *left_sprite,
                                   const char *right_sprite);
static void reset_choice_lines(void);
static void store_choice_line(int index, const char *line) __attribute__((noinline));
static void trim_leading_spaces(char *str);
static void ui_refresh_stand_only(const char *bg_file,
                                  const char *sprite_file,
                                  int x);
static void ui_refresh_left_stand_only_interlace(const char *bg_file,
                                                 const char *sprite_file);
static void ui_refresh_right_stand_only_interlace(const char *bg_file,
                                                  const char *sprite_file);

static void restore_palette_after_load(void);
static void restore_scene_after_load(void);
static void request_loaded_game_resume(void);
static void app_cleanup(void);


static void ui_draw_choice_jis(int choice_count, int selected);

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

static void ui_draw_status_char(int x, int y, uint16_t jis_code)
{
    unsigned char font[32];

    get_kanji_font(jis_code, font);
    draw_jis_char(x, y, font);
}

static void ui_draw_status_digit(int x, int y, int digit)
{
    ui_draw_status_char(x, y, (uint16_t)(STATUS_DIGIT_JIS + digit));
}

static void __attribute__((noinline, optimize("Os")))
ui_draw_status_2digit(int x, int y, int value)
{
    ui_draw_status_digit(x, y, value / 10);
    ui_draw_status_digit(x + STATUS_CHAR_WIDTH, y, value % 10);
}

static void __attribute__((optimize("Os")))
ui_refresh_status_ui(int erase)
{
    int i;
    int enabled;
    int hour;
    int minute;
    int money;
    int month;
    int day;
    int weekday;
    int divisor;
    int started;
    int back_ready;

    enabled = 0;
    hour = 0;
    minute = 0;
    money = 0;
    month = 0;
    day = 0;
    weekday = -1;

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (strcmp(g_flags[i].name, "status_ui") == 0) {
            enabled = g_flags[i].value;
        } else if (strcmp(g_flags[i].name, "hour") == 0) {
            hour = g_flags[i].value;
        } else if (strcmp(g_flags[i].name, "minute") == 0) {
            minute = g_flags[i].value;
        } else if (strcmp(g_flags[i].name, "money") == 0) {
            money = g_flags[i].value;
        } else if (strcmp(g_flags[i].name, "month") == 0) {
            month = g_flags[i].value;
        } else if (strcmp(g_flags[i].name, "day") == 0) {
            day = g_flags[i].value;
        } else if (strcmp(g_flags[i].name, "weekday") == 0) {
            weekday = g_flags[i].value;
        }
    }

    if (!erase && !enabled) {
        return;
    }

    if (hour < 0) hour = 0;
    if (hour > 99) hour = 99;
    if (minute < 0) minute = 0;
    if (minute > 99) minute = 99;
    if (money < 0) money = 0;
    if (month < 0) month = 0;
    if (month > 99) month = 99;
    if (day < 0) day = 0;
    if (day > 99) day = 99;

    /* Rect transfers and STATUS.SPR share graph98_image_work. */
    back_ready = graph98_prepare_rect_back_vram(
        STATUS_LEFT_X, STATUS_TIME_Y, STATUS_LEFT_X1, STATUS_MONEY_Y1);
    if (back_ready) {
        back_ready = graph98_prepare_rect_back_vram(
            STATUS_X, STATUS_TIME_Y, STATUS_X1, STATUS_MONEY_Y1);
    }
    if (!back_ready) {
        graph98_restore_default_pages();
    }

    graph98_boxfill(STATUS_LEFT_X, STATUS_TIME_Y,
                    STATUS_LEFT_X1, STATUS_TIME_Y1, STATUS_PANEL_COLOR);
    graph98_boxfill(STATUS_WEEKDAY_X, STATUS_MONEY_Y,
                    STATUS_WEEKDAY_X1, STATUS_MONEY_Y1,
                    STATUS_PANEL_COLOR);
    graph98_boxfill(STATUS_X, STATUS_TIME_Y,
                    STATUS_X1, STATUS_TIME_Y1, STATUS_PANEL_COLOR);
    graph98_boxfill(STATUS_X, STATUS_MONEY_Y,
                    STATUS_X1, STATUS_MONEY_Y1, STATUS_PANEL_COLOR);

    if (!erase) {
        if (!graph98_draw_status_file(
                STATUS_SPRITE_FILE,
                STATUS_LEFT_X, STATUS_X,
                STATUS_TIME_Y, STATUS_MONEY_Y,
                month, day, weekday,
                hour, minute, money)) {
            debug_log("status sprite load failed");
            ui_draw_status_2digit(STATUS_LEFT_X, STATUS_TIME_Y, month);
            ui_draw_status_char(STATUS_LEFT_X + STATUS_CHAR_WIDTH * 2,
                                STATUS_TIME_Y, STATUS_SLASH_JIS);
            ui_draw_status_2digit(STATUS_LEFT_X + STATUS_CHAR_WIDTH * 3,
                                  STATUS_TIME_Y, day);
            if (weekday >= 0 && weekday < 7) {
                ui_draw_status_char(STATUS_WEEKDAY_X, STATUS_MONEY_Y,
                                    g_status_weekday_jis[weekday]);
                ui_draw_status_char(STATUS_WEEKDAY_X + STATUS_CHAR_WIDTH,
                                    STATUS_MONEY_Y, STATUS_YOU_JIS);
                ui_draw_status_char(
                    STATUS_WEEKDAY_X + STATUS_CHAR_WIDTH * 2,
                    STATUS_MONEY_Y, g_status_weekday_jis[0]);
            }
            ui_draw_status_2digit(STATUS_X, STATUS_TIME_Y, hour);
            ui_draw_status_char(STATUS_X + STATUS_CHAR_WIDTH * 2,
                                STATUS_TIME_Y, STATUS_COLON_JIS);
            ui_draw_status_2digit(STATUS_X + STATUS_CHAR_WIDTH * 3,
                                  STATUS_TIME_Y, minute);
            divisor = 10000;
            started = 0;
            for (i = 0; i < STATUS_CHAR_COUNT; ++i) {
                int digit;

                digit = (money / divisor) % 10;
                if (digit != 0 || started || divisor == 1) {
                    ui_draw_status_digit(STATUS_X + STATUS_CHAR_WIDTH * i,
                                         STATUS_MONEY_Y, digit);
                    started = 1;
                }
                divisor /= 10;
            }
        }
    }

    if (back_ready) {
        if (!graph98_present_rect_back_vram(
                STATUS_LEFT_X, STATUS_TIME_Y,
                STATUS_LEFT_X1, STATUS_MONEY_Y1)) {
            debug_log("status left rect present failed");
            graph98_restore_default_pages();
        }
        if (!graph98_present_rect_back_vram(
                STATUS_X, STATUS_TIME_Y, STATUS_X1, STATUS_MONEY_Y1)) {
            debug_log("status rect present failed");
            graph98_restore_default_pages();
        }
    }
}

static void __attribute__((noinline, optimize("Os")))
ui_draw_background_effect(const char *bg_file, int use_interlace,
                          const char *failure_format)
{
    int ok;

    if (bg_file != 0 && bg_file[0] != '\0') {
        if (use_interlace) {
            ok = graph98_load_g98_interlace(bg_file);
        } else {
            ok = graph98_load_g98(bg_file);
        }

        if (ok) {
            return;
        }
        debug_log(failure_format, bg_file);
    }

    graph98_clear(0);
    graph98_boxfill(20, 20, 180, 80, 4);
    graph98_draw_string(30, 45, "BG LOAD NG", 15);
}

static void ui_draw_background(const char *bg_file)
{
    ui_draw_background_effect(bg_file, 0, "bg load failed: %s");
}

static void ui_draw_background_interlace(const char *bg_file)
{
    ui_draw_background_effect(bg_file, 1, "bg interlace load failed: %s");
}

static int ui_draw_current_scene_vram(const char *bg_file,
                                      const char *left_sprite,
                                      const char *right_sprite)
{
    if (graph98_draw_scene_file_trans_vram(
            bg_file, left_sprite, right_sprite,
            STAND_LEFT_X, STAND_RIGHT_X, STAND_Y, 0)) {
        return 1;
    }

    debug_log("scene draw failed: bg=%s", bg_file != 0 ? bg_file : "");
    return 0;
}

static void ui_refresh_stand_only(const char *bg_file,
                                  const char *sprite_file,
                                  int x)
{
    if (!graph98_draw_stand_file_trans_vram(
            bg_file, sprite_file, x, STAND_Y, 0)) {
        debug_log("stand vram refresh failed: bg=%s sprite=%s x=%d",
                  bg_file != 0 ? bg_file : "(null)",
                  sprite_file != 0 && sprite_file[0] != '\0' ?
                      sprite_file : "none",
                  x);
    }
}

static void ui_refresh_left_stand_only_interlace(const char *bg_file,
                                                 const char *sprite_file)
{
    if (!graph98_draw_stand_file_trans_interlace(
            bg_file, sprite_file, STAND_LEFT_X, STAND_Y, 0)) {
        debug_log("left stand interlace failed: bg=%s sprite=%s",
                  bg_file != 0 ? bg_file : "(null)",
                  sprite_file != 0 && sprite_file[0] != '\0' ?
                      sprite_file : "none");
    }
}

static void ui_refresh_right_stand_only_interlace(const char *bg_file,
                                                  const char *sprite_file)
{
    if (!graph98_draw_stand_file_trans_interlace(
            bg_file, sprite_file, STAND_RIGHT_X, STAND_Y, 0)) {
        debug_log("right stand interlace failed: bg=%s sprite=%s",
                  bg_file != 0 ? bg_file : "(null)",
                  sprite_file != 0 && sprite_file[0] != '\0' ?
                      sprite_file : "none");
    }
}

static void ui_draw_stand(const char *sprite_file, int x, int y)
{
    if (sprite_file == 0 || sprite_file[0] == '\0') {
        return;
    }

    if (!graph98_draw_sprite_file_trans(sprite_file, x, y, 0)) {
        debug_log("sprite load failed: %s", sprite_file);

        graph98_boxfill(x + 20, y + 20, x + 180, y + 80, 4);
        graph98_draw_string(x + 30, y + 45, "SPRITE LOAD NG", 15);
    }
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
    int back_ready;

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

    back_ready = graph98_prepare_rect_back_vram(
        g_msgbox_x0, g_msgbox_y0, g_msgbox_x1, g_msgbox_y1);
    if (!back_ready) {
        graph98_restore_default_pages();
    }

    ui_draw_message_window();

    if (name != 0 && name_draw_count > 0) {
        draw_jis_string(g_msg_text_x, message_line1_y,
                        name_text, name_draw_count + 2);
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

    if (back_ready &&
        !graph98_present_rect_back_vram(
            g_msgbox_x0, g_msgbox_y0, g_msgbox_x1, g_msgbox_y1)) {
        debug_log("message rect present failed");
        graph98_restore_default_pages();
    }

    ui_refresh_status_ui(0);

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
    ui_draw_background(g_state.bg_file);

    ui_draw_current_stands(g_state.left_sprite, g_state.right_sprite);
}

static void ui_redraw_current_scene_vram_from_state(void)
{
    graph98_restore_default_pages();
    if (!ui_draw_current_scene_vram(
            g_state.bg_file, g_state.left_sprite, g_state.right_sprite)) {
        graph98_restore_default_pages();
        ui_redraw_current_scene_from_state();
    }
    ui_refresh_status_ui(0);
}

/* メッセージ非表示中の待機関数 */
static void ui_hide_message_window_until_resume(void)
{
    uint8_t ch;

    graph98_restore_default_pages();

    if (g_msgbox_x0 <= STAND_RIGHT_EDGE &&
        g_msgbox_x1 >= STAND_LEFT_X &&
        g_msgbox_y0 <= STAND_BOTTOM_Y &&
        g_msgbox_y1 >= STAND_Y) {
        ui_redraw_current_scene_from_state();
    } else if (g_state.bg_file[0] == '\0' ||
               !graph98_load_g98_rect(g_state.bg_file,
                                      g_msgbox_x0, g_msgbox_y0,
                                      g_msgbox_x1, g_msgbox_y1) ||
               !graph98_load_g98_rect(g_state.bg_file,
                                      STATUS_LEFT_X, STATUS_TIME_Y,
                                      STATUS_LEFT_X1, STATUS_MONEY_Y1) ||
               !graph98_load_g98_rect(g_state.bg_file,
                                      STATUS_X, STATUS_TIME_Y,
                                      STATUS_X1, STATUS_MONEY_Y1)) {
        debug_log("H hide rect restore failed: %s", g_state.bg_file);
        ui_redraw_current_scene_from_state();
    }

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
    graph98_restore_default_pages();
    restore_palette_after_load();
    ui_redraw_current_scene_vram_from_state();
    resume_bgm_after_load();
}

static void request_loaded_game_resume(void)
{
    g_request_scene_redraw = 1;
    g_request_script_resume = 1;
}

// 立ち絵を今の状態で描く関数
static void ui_draw_current_stands(const char *left_sprite,
                                   const char *right_sprite)
{
    ui_draw_stand(left_sprite, STAND_LEFT_X, STAND_Y);
    ui_draw_stand(right_sprite, STAND_RIGHT_X, STAND_Y);
}

static void app_cleanup(void)
{
    if (g_pmd_available) {
        pmd_stop_music();
    }

    graph98_restore_default_pages();
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
        ui_redraw_current_scene_vram_from_state;
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
        script_context.draw_background_interlace = ui_draw_background_interlace;
        script_context.draw_scene_vram = ui_draw_current_scene_vram;
        script_context.draw_stand = ui_draw_stand;
        script_context.refresh_left_stand_only_interlace =
            ui_refresh_left_stand_only_interlace;
        script_context.refresh_right_stand_only_interlace =
            ui_refresh_right_stand_only_interlace;
        script_context.refresh_stand_only = ui_refresh_stand_only;
        script_context.refresh_status_ui = ui_refresh_status_ui;
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
