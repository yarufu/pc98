#ifndef MENU_H
#define MENU_H

#include "game_state.h"
#include "script.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    GameState *state;
    GameFlag *flags;
    int *pmd_available;
    int *fm_se_loaded;
    int *mouse_available;
    int *request_script_resume;
    enum SystemAction *system_action;

    uint16_t *choice_work_jis;
    uint16_t *choice_saved_jis;
    size_t choice_jis_bytes;
    int *choice_work_lens;
    int *choice_saved_lens;
    size_t choice_lens_bytes;

    void (*reset_choice_lines)(void);
    void (*store_choice_line)(int index, const char *line);
    int (*wait_choice)(int choice_count, int allow_save_load);
    void (*draw_choice_jis)(int choice_count, int selected);
    int (*key_available)(void);
    uint8_t (*read_key)(uint8_t *key_code);
    void (*redraw_current_scene_from_state)(void);
    void (*restore_scene_after_load)(void);
    void (*request_loaded_game_resume)(void);
} MenuContext;

void menu_init(const MenuContext *ctx);

int show_selection_menu(const char *const *items, int item_count);
void ui_show_notice(const char *message);
void show_save_menu(void);
int show_load_menu(void);
enum SystemAction show_system_menu(void);
int open_system_menu(void);
int open_system_menu_from_choice(int choice_count, int selected,
                                 long *mouse_accum_x,
                                 long *mouse_accum_y);

#endif
