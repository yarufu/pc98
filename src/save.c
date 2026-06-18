#include "save.h"

#include "debug.h"

#include <stdio.h>
#include <string.h>

#define SAVE_VERSION 2

typedef struct {
    char magic[8];
    int version;
    GameState state;
    GameFlag flags[MAX_FLAGS];
} SaveData;

static const char *g_save_slot_files[SAVE_SLOT_COUNT] = {
    "SAVE1.DAT",
    "SAVE2.DAT",
    "SAVE3.DAT"
};

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
    SaveData save_data;

    if (filename == 0 || state == 0 || flags == 0) {
        return 0;
    }

    memset(&save_data, 0, sizeof(save_data));
    memcpy(save_data.magic, "ADV98SAV", 8);
    save_data.version = SAVE_VERSION;
    save_data.state = *state;
    memcpy(save_data.flags, flags, sizeof(save_data.flags));

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
    SaveData save_data;

    if (filename == 0 || state == 0 || flags == 0) {
        return 0;
    }

    memset(&save_data, 0, sizeof(save_data));

    fp = fopen(filename, "rb");
    if (fp == 0) {
        debug_log("LOAD FAILED open file=%s", filename);
        return 0;
    }

    if (fread(&save_data, sizeof(save_data), 1, fp) != 1) {
        debug_log("LOAD FAILED read file=%s", filename);
        fclose(fp);
        return 0;
    }

    fclose(fp);

    if (memcmp(save_data.magic, "ADV98SAV", 8) != 0) {
        debug_log("LOAD FAILED bad magic");
        return 0;
    }

    if (save_data.version != SAVE_VERSION) {
        debug_log("LOAD FAILED bad version=%d expected=%d",
                  save_data.version,
                  SAVE_VERSION);
        return 0;
    }

    *state = save_data.state;
    memcpy(flags, save_data.flags, sizeof(save_data.flags));

    debug_log("LOAD OK file=%s version=%d line=%d",
              filename,
              save_data.version,
              save_data.state.script_line);

    return 1;
}
