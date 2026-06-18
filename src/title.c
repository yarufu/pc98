#include "title.h"

#include "debug.h"
#include "graph98.h"
#include "pmd.h"

#include <stdio.h>

#define TITLE_MENU_ITEM_COUNT 3

/* Shift_JIS: はじめから、ロード、終了 */
static const char g_title_menu_start[] =
    "\x82\xCD\x82\xB6\x82\xDF\x82\xA9\x82\xE7";
static const char g_title_menu_load[] = "\x83\x8D\x81\x5B\x83\x68";
static const char g_title_menu_exit[] = "\x8F\x49\x97\xB9";

static const char *g_title_menu_items[TITLE_MENU_ITEM_COUNT] = {
    g_title_menu_start, g_title_menu_load, g_title_menu_exit
};

static int g_title_bgm_playing = 0;

static void ui_draw_title_screen(const TitleContext *ctx)
{
    if (!graph98_load_palette_file("TITLE.PAL")) {
        debug_log("TITLE.PAL load failed. Keeping current palette.");
    }

    graph98_clear(0);

    if (!graph98_load_g98("TITLE.G98")) {
        graph98_clear(0);
        debug_log("TITLE.G98 load failed. Using black background.");
    }

    if (ctx->draw_message_window != 0) {
        ctx->draw_message_window();
    }
}

static void title_bgm_start(const TitleContext *ctx)
{
    FILE *fp;

    if (!ctx->pmd_available || g_title_bgm_playing) {
        return;
    }

    fp = fopen("TITLE.M", "rb");
    if (fp == 0) {
        return;
    }
    fclose(fp);

    if (!pmd_load_music_file("TITLE.M")) {
        debug_log("TITLE.M load failed.");
        return;
    }

    pmd_start_music();
    g_title_bgm_playing = 1;
}

static void title_bgm_stop(const TitleContext *ctx)
{
    if (!ctx->pmd_available || !g_title_bgm_playing) {
        return;
    }

    pmd_stop_music();
    g_title_bgm_playing = 0;
}

int show_title_menu(const TitleContext *ctx)
{
    int selected;

    if (ctx == 0 ||
        ctx->show_selection_menu == 0 ||
        ctx->show_load_menu == 0 ||
        ctx->restore_scene_after_load == 0 ||
        ctx->request_loaded_game_resume == 0) {
        return 0;
    }

    ui_draw_title_screen(ctx);
    title_bgm_start(ctx);

    for (;;) {
        selected = ctx->show_selection_menu(g_title_menu_items,
                                            TITLE_MENU_ITEM_COUNT);

        if (selected == 1) {
            title_bgm_stop(ctx);
            return 1;
        }

        if (selected == 2) {
            if (ctx->show_load_menu()) {
                title_bgm_stop(ctx);
                ctx->restore_scene_after_load();
                ctx->request_loaded_game_resume();
                return 1;
            }
            ui_draw_title_screen(ctx);
            title_bgm_start(ctx);
            continue;
        }

        if (selected == 3) {
            return 0;
        }
    }
}
