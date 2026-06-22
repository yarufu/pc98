#include "game_flags.h"

#include <string.h>

static int find_flag_index(const ScriptContext *ctx, const char *name)
{
    int i;

    if (name == 0 || name[0] == '\0') {
        return -1;
    }

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (ctx->flags[i].name[0] == '\0') {
            continue;
        }

        if (strcmp(ctx->flags[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

void set_flag_on(const ScriptContext *ctx, const char *name)
{
    int i;
    int idx;

    if (name == 0 || name[0] == '\0') {
        return;
    }

    idx = find_flag_index(ctx, name);
    if (idx >= 0) {
        ctx->flags[idx].value = 1;
        return;
    }

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (ctx->flags[i].name[0] == '\0') {
            strcpy(ctx->flags[i].name, name);
            ctx->flags[i].value = 1;
            return;
        }
    }
}

void set_flag_off(const ScriptContext *ctx, const char *name)
{
    int idx;

    if (name == 0 || name[0] == '\0') {
        return;
    }

    idx = find_flag_index(ctx, name);
    if (idx >= 0) {
        ctx->flags[idx].value = 0;
    }
}

int is_flag_on(const ScriptContext *ctx, const char *name)
{
    int idx;

    idx = find_flag_index(ctx, name);
    if (idx >= 0) {
        return ctx->flags[idx].value;
    }

    return 0;
}

static GameFlag *find_flag(const ScriptContext *ctx, const char *name)
{
    int i;

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (ctx->flags[i].name[0] == '\0') {
            continue;
        }

        if (strcmp(ctx->flags[i].name, name) == 0) {
            return &ctx->flags[i];
        }
    }

    return 0;
}

static GameFlag *find_or_create_flag(const ScriptContext *ctx,
                                     const char *name)
{
    int i;
    GameFlag *flag;

    flag = find_flag(ctx, name);
    if (flag != 0) {
        return flag;
    }

    for (i = 0; i < MAX_FLAGS; ++i) {
        if (ctx->flags[i].name[0] == '\0') {
            strcpy(ctx->flags[i].name, name);
            ctx->flags[i].value = 0;
            return &ctx->flags[i];
        }
    }

    return 0;
}

void add_flag_value(const ScriptContext *ctx, const char *name, int value)
{
    GameFlag *flag;

    flag = find_or_create_flag(ctx, name);
    if (flag != 0) {
        flag->value += value;
    }
}

void set_flag_value(const ScriptContext *ctx, const char *name, int value)
{
    GameFlag *flag;

    flag = find_or_create_flag(ctx, name);
    if (flag != 0) {
        flag->value = value;
    }
}

int get_flag_value(const ScriptContext *ctx, const char *name)
{
    GameFlag *flag;

    flag = find_flag(ctx, name);
    if (flag == 0) {
        return 0;
    }

    return flag->value;
}
