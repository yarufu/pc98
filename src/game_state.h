#ifndef GAME_STATE_H
#define GAME_STATE_H

#define MAX_FLAGS 16
#define CALL_STACK_MAX 8
#define SAVE_SLOT_COUNT 3
#define BG_FILENAME_SIZE 13
#define SPRITE_FILENAME_SIZE 13
#define UI_FILENAME_SIZE 13
#define DEFAULT_UI_FILE "UI.G98"

typedef struct {
    char name[32];
    int value;
} GameFlag;

typedef struct {
    char bg_file[BG_FILENAME_SIZE];
    int script_line;
    int call_stack[CALL_STACK_MAX];
    int call_stack_depth;

    char left_sprite[SPRITE_FILENAME_SIZE];
    char right_sprite[SPRITE_FILENAME_SIZE];

    char bgm[64];
    char ui_file[UI_FILENAME_SIZE];
} GameState;

#endif
