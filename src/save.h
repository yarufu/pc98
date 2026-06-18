#ifndef SAVE_H
#define SAVE_H

#define MAX_FLAGS 16
#define CALL_STACK_MAX 8
#define SAVE_SLOT_COUNT 3

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

typedef struct {
    char bg_name[32];
    int script_line;
    int call_stack[CALL_STACK_MAX];
    int call_stack_depth;

    enum StandId left_stand;
    enum FaceId left_face;

    enum StandId right_stand;
    enum FaceId right_face;

    char bgm[64];
} GameState;

const char *save_get_slot_file(int slot_index);
int save_game_state(const char *filename,
                    const GameState *state,
                    const GameFlag *flags);
int load_game_state(const char *filename,
                    GameState *state,
                    GameFlag *flags);

#endif
