#include "menu.h"

#include "graph98.h"
#include "input.h"
#include "mouse98.h"
#include "pmd.h"
#include "save.h"
#include "text98.h"

#include <string.h>

#define MAX_CHOICE_ITEMS 6
#define MAX_CHOICE_CHARS 64
#define SAVE_MENU_ITEM_COUNT (SAVE_SLOT_COUNT + 1)
#define SYSTEM_MENU_ITEM_COUNT 5

/* Shift_JIS: セーブ１～３、ロード１～３、戻る */
static const char g_save_menu_1[] = "\x83\x5A\x81\x5B\x83\x75\x82\x50";
static const char g_save_menu_2[] = "\x83\x5A\x81\x5B\x83\x75\x82\x51";
static const char g_save_menu_3[] = "\x83\x5A\x81\x5B\x83\x75\x82\x52";
static const char g_load_menu_1[] = "\x83\x8D\x81\x5B\x83\x68\x82\x50";
static const char g_load_menu_2[] = "\x83\x8D\x81\x5B\x83\x68\x82\x51";
static const char g_load_menu_3[] = "\x83\x8D\x81\x5B\x83\x68\x82\x52";
static const char g_menu_back[] = "\x96\xDF\x82\xE9";
/* Shift_JIS: セーブ、ロード、タイトルへ戻る、ゲーム終了、キャンセル */
static const char g_system_menu_save[] =
    "\x83\x5A\x81\x5B\x83\x75";
static const char g_system_menu_load[] =
    "\x83\x8D\x81\x5B\x83\x68";
static const char g_system_menu_cancel[] =
    "\x83\x4C\x83\x83\x83\x93\x83\x5A\x83\x8B";
static const char g_system_menu_title[] =
    "\x83\x5E\x83\x43\x83\x67\x83\x8B\x82\xD6\x96\xDF\x82\xE9";
static const char g_system_menu_exit[] =
    "\x83\x51\x81\x5B\x83\x80\x8F\x49\x97\xB9";
/* Shift_JIS: セーブしました */
static const char g_notice_saved[] =
    "\x83\x5A\x81\x5B\x83\x75\x82\xB5\x82\xDC\x82\xB5\x82\xBD";

static const char *g_save_menu_items[SAVE_MENU_ITEM_COUNT] = {
    g_save_menu_1, g_save_menu_2, g_save_menu_3, g_menu_back
};
static const char *g_load_menu_items[SAVE_MENU_ITEM_COUNT] = {
    g_load_menu_1, g_load_menu_2, g_load_menu_3, g_menu_back
};
static const char *g_system_menu_items[SYSTEM_MENU_ITEM_COUNT] = {
    g_system_menu_save,
    g_system_menu_load,
    g_system_menu_title,
    g_system_menu_exit,
    g_system_menu_cancel
};

static MenuContext g_menu;

void menu_init(const MenuContext *ctx)
{
    g_menu = *ctx;
}

int show_selection_menu(const char *const *items, int item_count)
{
    int i;

    if (items == 0 || item_count < 1 || item_count > MAX_CHOICE_ITEMS) {
        return 0;
    }

    g_menu.reset_choice_lines();

    for (i = 0; i < item_count; ++i) {
        g_menu.store_choice_line(i, items[i]);
    }

    return g_menu.wait_choice(item_count, 0);
}

void ui_show_notice(const char *message)
{
    static unsigned char notice_fonts[MAX_CHOICE_CHARS][32];
    static const unsigned char *line_fonts[MAX_CHOICE_CHARS];
    uint16_t message_jis[MAX_CHOICE_CHARS];
    uint8_t ch;
    int message_len;
    int i;
    int width;
    int x0;
    int y0;
    int x1;
    int y1;

    if (message == 0 || message[0] == '\0') {
        return;
    }

    message_len = convert_sjis_string_to_jis_array(
        (const unsigned char *)message,
        message_jis,
        MAX_CHOICE_CHARS);
    if (message_len <= 0) {
        return;
    }

    width = message_len * 16 + 32;
    x0 = (640 - width) / 2;
    x1 = x0 + width - 1;
    y0 = 176;
    y1 = 223;

    graph98_boxfill(x0, y0, x1, y1, 0);
    graph98_rect(x0, y0, x1, y1, 15);

    for (i = 0; i < message_len; ++i) {
        get_kanji_font(message_jis[i], notice_fonts[i]);
        line_fonts[i] = notice_fonts[i];
    }

    draw_jis_string(x0 + 16, y0 + 16, line_fonts, message_len);

    for (;;) {
        if (*g_menu.mouse_available && mouse98_left_pressed()) {
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

    g_menu.redraw_current_scene_from_state();
}

void show_save_menu(void)
{
    int selected;
    const char *filename;

    selected = show_selection_menu(g_save_menu_items, SAVE_MENU_ITEM_COUNT);
    if (selected >= 1 && selected <= SAVE_SLOT_COUNT) {
        filename = save_get_slot_file(selected - 1);
        if (save_game_state(filename, g_menu.state, g_menu.flags)) {
            if (*g_menu.fm_se_loaded) {
                pmd_play_fm_se(1);
            }
            ui_show_notice(g_notice_saved);
        }
    }
}

int show_load_menu(void)
{
    int selected;
    const char *filename;

    selected = show_selection_menu(g_load_menu_items, SAVE_MENU_ITEM_COUNT);
    if (selected >= 1 && selected <= SAVE_SLOT_COUNT) {
        filename = save_get_slot_file(selected - 1);
        return load_game_state(filename, g_menu.state, g_menu.flags);
    }

    return 0;
}

static int handle_load_hotkey(uint8_t ch)
{
    (void)ch;

    if (show_load_menu()) {
        g_menu.restore_scene_after_load();
        g_menu.request_loaded_game_resume();
        return 1;
    }

    return 0;
}

enum SystemAction show_system_menu(void)
{
    int selected;

    selected = show_selection_menu(g_system_menu_items,
                                   SYSTEM_MENU_ITEM_COUNT);
    if (selected == 1) {
        show_save_menu();
        return SYSTEM_ACTION_NONE;
    }

    if (selected == 2) {
        handle_load_hotkey(0);
        return SYSTEM_ACTION_NONE;
    }

    if (selected == 3) {
        if (*g_menu.pmd_available) {
            pmd_stop_music();
        }
        return SYSTEM_ACTION_TITLE;
    }

    if (selected == 4) {
        return SYSTEM_ACTION_EXIT;
    }

    return SYSTEM_ACTION_NONE;
}

int open_system_menu(void)
{
    *g_menu.system_action = show_system_menu();
    if (*g_menu.request_script_resume ||
        *g_menu.system_action != SYSTEM_ACTION_NONE) {
        return 1;
    }

    return 0;
}

int open_system_menu_from_choice(int choice_count, int selected,
                                 long *mouse_accum_x,
                                 long *mouse_accum_y)
{
    memcpy(g_menu.choice_saved_jis, g_menu.choice_work_jis,
           g_menu.choice_jis_bytes);
    memcpy(g_menu.choice_saved_lens, g_menu.choice_work_lens,
           g_menu.choice_lens_bytes);

    if (open_system_menu()) {
        return 1;
    }

    memcpy(g_menu.choice_work_jis, g_menu.choice_saved_jis,
           g_menu.choice_jis_bytes);
    memcpy(g_menu.choice_work_lens, g_menu.choice_saved_lens,
           g_menu.choice_lens_bytes);
    *mouse_accum_x = 0;
    *mouse_accum_y = 0;
    g_menu.draw_choice_jis(choice_count, selected);
    return 0;
}
