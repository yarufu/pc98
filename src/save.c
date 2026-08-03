#include "save.h"

#include "debug.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SAVE_VERSION_V4 4
#define SAVE_VERSION_V5 5
#define SAVE_VERSION 6

typedef struct {
    char bg_file[BG_FILENAME_SIZE];
    int script_line;
    int call_stack[CALL_STACK_MAX];
    int call_stack_depth;
    char left_sprite[SPRITE_FILENAME_SIZE];
    char right_sprite[SPRITE_FILENAME_SIZE];
    char bgm[64];
} GameStateV4;

typedef struct {
    char bg_file[BG_FILENAME_SIZE];
    int script_line;
    int call_stack[CALL_STACK_MAX];
    int call_stack_depth;
    char left_sprite[SPRITE_FILENAME_SIZE];
    char right_sprite[SPRITE_FILENAME_SIZE];
    char bgm[64];
    char ui_file[UI_FILENAME_SIZE];
} GameStateV5;

typedef struct {
    char magic[8];
    int version;
} SaveDataHeader;

typedef struct {
    char magic[8];
    int version;
    GameStateV4 state;
    GameFlag flags[MAX_FLAGS];
} SaveDataV4;

typedef struct {
    char magic[8];
    int version;
    GameStateV5 state;
    GameFlag flags[MAX_FLAGS];
} SaveDataV5;

typedef struct {
    char magic[8];
    int version;
    GameState state;
    GameFlag flags[MAX_FLAGS];
} SaveDataV6;

typedef union {
    SaveDataHeader header;
    SaveDataV4 v4;
    SaveDataV5 v5;
    SaveDataV6 v6;
} SaveDataBuffer;

_Static_assert(sizeof(GameStateV4) == 124u,
               "version 4 GameState layout changed");
_Static_assert(sizeof(GameStateV5) == 138u,
               "version 5 GameState layout changed");
_Static_assert(sizeof(GameState) == 138u,
               "version 6 GameState layout changed");
_Static_assert(offsetof(GameState, scene_mode) == 137u,
               "version 6 scene mode must reuse V5 tail padding");
_Static_assert(sizeof(SaveDataHeader) == 10u,
               "save header layout changed");
_Static_assert(sizeof(SaveDataV4) == 678u,
               "version 4 SaveData layout changed");
_Static_assert(sizeof(SaveDataV5) == 692u,
               "version 5 SaveData layout changed");
_Static_assert(sizeof(SaveDataV6) == 692u,
               "version 6 SaveData layout changed");
_Static_assert(sizeof(SaveDataBuffer) == sizeof(SaveDataV6),
               "save load buffer must use one V6-sized stack slot");

static const char *g_save_slot_files[SAVE_SLOT_COUNT] = {
    "SAVE1.DAT",
    "SAVE2.DAT",
    "SAVE3.DAT"
};

static int save_string_has_nul(const char *text, unsigned int size)
{
    unsigned int i;

    for (i = 0; i < size; ++i) {
        if (text[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void save_terminate_state_strings(GameState *state)
{
    state->bg_file[BG_FILENAME_SIZE - 1] = '\0';
    state->left_sprite[SPRITE_FILENAME_SIZE - 1] = '\0';
    state->right_sprite[SPRITE_FILENAME_SIZE - 1] = '\0';
    state->bgm[sizeof(state->bgm) - 1] = '\0';
    state->ui_file[UI_FILENAME_SIZE - 1] = '\0';
    if (state->ui_file[0] == '\0') {
        strcpy(state->ui_file, DEFAULT_UI_FILE);
    }
    if (state->scene_mode > SCENE_MODE_FULLSCREEN_CG) {
        state->scene_mode = SCENE_MODE_SEPARATED_UI;
    }
}

static void save_terminate_flag_names(GameFlag *flags)
{
    int i;

    for (i = 0; i < MAX_FLAGS; ++i) {
        flags[i].name[sizeof(flags[i].name) - 1] = '\0';
    }
}

static int save_v5_state_strings_valid(const GameStateV5 *state)
{
    return save_string_has_nul(state->bg_file, sizeof(state->bg_file)) &&
           save_string_has_nul(state->left_sprite,
                               sizeof(state->left_sprite)) &&
           save_string_has_nul(state->right_sprite,
                               sizeof(state->right_sprite)) &&
           save_string_has_nul(state->bgm, sizeof(state->bgm)) &&
           save_string_has_nul(state->ui_file, sizeof(state->ui_file));
}

static int save_v6_state_valid(const GameState *state)
{
    return save_string_has_nul(state->bg_file, sizeof(state->bg_file)) &&
           save_string_has_nul(state->left_sprite,
                               sizeof(state->left_sprite)) &&
           save_string_has_nul(state->right_sprite,
                               sizeof(state->right_sprite)) &&
           save_string_has_nul(state->bgm, sizeof(state->bgm)) &&
           save_string_has_nul(state->ui_file, sizeof(state->ui_file)) &&
           state->scene_mode <= SCENE_MODE_FULLSCREEN_CG;
}

static void save_convert_v4_state(GameState *state,
                                  const GameStateV4 *old_state)
{
    memset(state, 0, sizeof(*state));
    memcpy(state->bg_file, old_state->bg_file, sizeof(old_state->bg_file));
    state->script_line = old_state->script_line;
    memcpy(state->call_stack, old_state->call_stack,
           sizeof(old_state->call_stack));
    state->call_stack_depth = old_state->call_stack_depth;
    memcpy(state->left_sprite, old_state->left_sprite,
           sizeof(old_state->left_sprite));
    memcpy(state->right_sprite, old_state->right_sprite,
           sizeof(old_state->right_sprite));
    memcpy(state->bgm, old_state->bgm, sizeof(old_state->bgm));
    strcpy(state->ui_file, DEFAULT_UI_FILE);
    state->scene_mode = SCENE_MODE_SEPARATED_UI;
    save_terminate_state_strings(state);
}

static void save_convert_v5_state(GameState *state,
                                  const GameStateV5 *old_state)
{
    memset(state, 0, sizeof(*state));
    memcpy(state, old_state, sizeof(*old_state));
    state->scene_mode = SCENE_MODE_SEPARATED_UI;
    save_terminate_state_strings(state);
}

const char *save_get_slot_file(int slot_index)
{
    if (slot_index < 0 || slot_index >= SAVE_SLOT_COUNT) {
        return 0;
    }

    return g_save_slot_files[slot_index];
}

int save_game_state(const char *filename,
                    const GameState *state,
                    const GameFlag *flags)
{
    FILE *fp;
    SaveDataV6 save_data;

    if (filename == 0 || state == 0 || flags == 0) {
        return 0;
    }

    memset(&save_data, 0, sizeof(save_data));
    memcpy(save_data.magic, "ADV98SAV", 8);
    save_data.version = SAVE_VERSION;
    save_data.state = *state;
    memcpy(save_data.flags, flags, sizeof(save_data.flags));
    save_terminate_state_strings(&save_data.state);
    save_terminate_flag_names(save_data.flags);

    fp = fopen(filename, "wb");
    if (fp == 0) {
        debug_log("SAVE FAILED open file=%s", filename);
        return 0;
    }

    if (fwrite(&save_data, sizeof(save_data), 1, fp) != 1) {
        debug_log("SAVE FAILED write file=%s", filename);
        fclose(fp);
        return 0;
    }

    fclose(fp);

    debug_log("SAVE OK file=%s version=%d line=%d",
              filename,
              save_data.version,
              save_data.state.script_line);

    return 1;
}

int load_game_state(const char *filename,
                    GameState *state,
                    GameFlag *flags)
{
    FILE *fp;
    SaveDataBuffer save_data;
    int version;

    if (filename == 0 || state == 0 || flags == 0) {
        return 0;
    }

    fp = fopen(filename, "rb");
    if (fp == 0) {
        debug_log("LOAD FAILED open file=%s", filename);
        return 0;
    }

    if (fread(&save_data.header, sizeof(save_data.header), 1, fp) != 1) {
        debug_log("LOAD FAILED read header file=%s", filename);
        fclose(fp);
        return 0;
    }

    if (memcmp(save_data.header.magic, "ADV98SAV", 8) != 0) {
        debug_log("LOAD FAILED bad magic");
        fclose(fp);
        return 0;
    }

    version = save_data.header.version;
    if (version != SAVE_VERSION_V4 &&
        version != SAVE_VERSION_V5 && version != SAVE_VERSION) {
        debug_log("LOAD FAILED bad version=%d expected=4, 5 or 6", version);
        fclose(fp);
        return 0;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        debug_log("LOAD FAILED seek file=%s", filename);
        fclose(fp);
        return 0;
    }

    if (version == SAVE_VERSION_V4) {
        if (fread(&save_data.v4, sizeof(save_data.v4), 1, fp) != 1) {
            debug_log("LOAD FAILED read V4 file=%s", filename);
            fclose(fp);
            return 0;
        }
        if (memcmp(save_data.v4.magic, "ADV98SAV", 8) != 0 ||
            save_data.v4.version != SAVE_VERSION_V4) {
            debug_log("LOAD FAILED changed V4 header");
            fclose(fp);
            return 0;
        }
        save_convert_v4_state(state, &save_data.v4.state);
        memcpy(flags, save_data.v4.flags, sizeof(save_data.v4.flags));
    } else if (version == SAVE_VERSION_V5) {
        if (fread(&save_data.v5, sizeof(save_data.v5), 1, fp) != 1) {
            debug_log("LOAD FAILED read V5 file=%s", filename);
            fclose(fp);
            return 0;
        }
        if (memcmp(save_data.v5.magic, "ADV98SAV", 8) != 0 ||
            save_data.v5.version != SAVE_VERSION_V5 ||
            !save_v5_state_strings_valid(&save_data.v5.state)) {
            debug_log("LOAD FAILED invalid V5 data");
            fclose(fp);
            return 0;
        }
        save_convert_v5_state(state, &save_data.v5.state);
        memcpy(flags, save_data.v5.flags, sizeof(save_data.v5.flags));
    } else {
        if (fread(&save_data.v6, sizeof(save_data.v6), 1, fp) != 1) {
            debug_log("LOAD FAILED read V6 file=%s", filename);
            fclose(fp);
            return 0;
        }
        if (memcmp(save_data.v6.magic, "ADV98SAV", 8) != 0 ||
            save_data.v6.version != SAVE_VERSION ||
            !save_v6_state_valid(&save_data.v6.state)) {
            debug_log("LOAD FAILED invalid V6 data");
            fclose(fp);
            return 0;
        }
        *state = save_data.v6.state;
        memcpy(flags, save_data.v6.flags, sizeof(save_data.v6.flags));
        save_terminate_state_strings(state);
    }

    fclose(fp);
    save_terminate_flag_names(flags);

    debug_log("LOAD OK file=%s version=%d line=%d",
              filename,
              version,
              state->script_line);

    return 1;
}
