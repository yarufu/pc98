#ifndef GAME_FLAGS_H
#define GAME_FLAGS_H

#include "script.h"

void set_flag_on(const ScriptContext *ctx, const char *name);
void set_flag_off(const ScriptContext *ctx, const char *name);
int is_flag_on(const ScriptContext *ctx, const char *name);
void add_flag_value(const ScriptContext *ctx, const char *name, int value);
void set_flag_value(const ScriptContext *ctx, const char *name, int value);
int get_flag_value(const ScriptContext *ctx, const char *name);

#endif
