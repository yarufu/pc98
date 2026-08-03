#ifndef SCRIPT_H
#define SCRIPT_H

#include "game_state.h"

#include <stdint.h>
#include <stdio.h>

enum SystemAction {
    SYSTEM_ACTION_NONE = 0,
    SYSTEM_ACTION_TITLE,
    SYSTEM_ACTION_EXIT
};

enum GameResult {
    GAME_RESULT_SCRIPT_END = 0,
    GAME_RESULT_RETURN_TO_TITLE,
    GAME_RESULT_EXIT_TO_DOS
};

typedef struct {
    GameState *state;
    GameFlag *flags;
    int pmd_available;
    int fm_se_loaded;
    int *request_scene_redraw;
    int *request_script_resume;
    enum SystemAction *system_action;

    void (*set_message_box)(int x0, int y0, int x1, int y1);
    int (*change_ui)(const char *ui_file);
    int (*change_scene)(const char *bg_file,
                        const char *left_sprite,
                        const char *right_sprite,
                        uint8_t scene_mode,
                        int use_interlace);
    void (*refresh_left_stand_only_interlace)(const char *bg_file,
                                              const char *sprite_file);
    void (*refresh_right_stand_only_interlace)(const char *bg_file,
                                               const char *sprite_file);
    void (*refresh_stand_only)(const char *bg_file,
                               const char *sprite_file,
                               int x);
    void (*refresh_status_ui)(int erase);
    void (*draw_message_jis)(const uint16_t *name, int name_len,
                             const uint16_t *jis_codes, int count);
    void (*reset_choice_lines)(void);
    void (*store_choice_line)(int index, const char *line);
    int (*wait_choice)(int choice_count, int allow_save_load);
} ScriptContext;

enum GameResult run_script_sjis(const ScriptContext *ctx);

#endif
