#ifndef SAVE_H
#define SAVE_H

#include "game_state.h"

const char *save_get_slot_file(int slot_index);
int save_game_state(const char *filename,
                    const GameState *state,
                    const GameFlag *flags);
int load_game_state(const char *filename,
                    GameState *state,
                    GameFlag *flags);

#endif
