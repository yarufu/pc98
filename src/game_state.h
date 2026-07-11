#ifndef GAME_STATE_H
#define GAME_STATE_H

#define MAX_FLAGS 16
#define CALL_STACK_MAX 8
#define SAVE_SLOT_COUNT 3
#define SPRITE_FILENAME_SIZE 13

typedef struct {
    char name[32];
    int value;
} GameFlag;

typedef struct {
    char bg_name[32];
    int script_line;
    int call_stack[CALL_STACK_MAX];
    int call_stack_depth;

    char left_sprite[SPRITE_FILENAME_SIZE];
    char right_sprite[SPRITE_FILENAME_SIZE];

    char bgm[64];
} GameState;

#endif
