#ifndef TITLE_H
#define TITLE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pmd_available;
    int fm_se_loaded;
    int (*show_selection_menu)(const char *const *items, int item_count);
    int (*show_load_menu)(void);
    void (*restore_scene_after_load)(void);
    void (*request_loaded_game_resume)(void);
    void (*draw_message_window)(void);
} TitleContext;

int show_title_menu(const TitleContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
