#include "script.h"

#include "debug.h"
#include "game_flags.h"
#include "graph98.h"
#include "pmd.h"
#include "text98.h"

#include <stdlib.h>
#include <string.h>

#define STAND_LEFT_X   60
#define STAND_RIGHT_X  320
#define STAND_Y        9
#define CHOICE_RESULT_LOAD_RESUME 0

enum CommandResult {
    COMMAND_NOT_HANDLED = 0,
    COMMAND_HANDLED,
    COMMAND_JUMPED
};

typedef struct {
    char cmd[32];
    char arg1[64];
    char arg2[64];
    char arg3[64];
    int count;
} ParsedCommand;

typedef struct {
    char last_bg_name[32];
    enum StandId last_left_stand;
    enum StandId last_right_stand;
    enum FaceId last_left_face;
    enum FaceId last_right_face;
    int scene_dirty;
    int stand_dirty;
    int bg_interlace_pending;
    int left_interlace_pending;
    int right_interlace_pending;
} SceneRenderState;

// 改行削除関数
static void remove_newline(char *str)
{
    int len = (int)strlen(str);

    while (len > 0 &&
           (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        --len;
    }
}

// 先頭空白を飛ばす小関数
static void trim_leading_spaces(char *str)
{
    int i;
    int j;

    i = 0;
    while (str[i] == ' ' || str[i] == '\t') {
        i++;
    }

    if (i == 0) {
        return;
    }

    j = 0;
    while (str[i] != '\0') {
        str[j++] = str[i++];
    }
    str[j] = '\0';
}

static int read_script_line(const ScriptContext *ctx,
                            FILE *fp,
                            char *line,
                            int line_size,
                            int *script_line)
{
    if (fgets(line, line_size, fp) == 0) {
        return 0;
    }

    if (script_line != 0) {
        (*script_line)++;
        ctx->state->script_line = *script_line;
    }

    return 1;
}

// ラベルを探してそこへ飛ぶ関数
static int find_label_and_jump(const ScriptContext *ctx,
                               FILE *fp,
                               int *script_line,
                               const char *label_name)
{
    char line[256];
    char cmd[32];
    char arg1[64];
    int count;

    if (fp == 0 || label_name == 0 || label_name[0] == '\0') {
        return 0;
    }

    /* いったん先頭へ戻ってラベルを探す */
    fseek(fp, 0L, SEEK_SET);
    if (script_line != 0) {
        *script_line = 0;
    }

    while (read_script_line(ctx, fp, line, sizeof(line), script_line)) {
        remove_newline(line);
        trim_leading_spaces(line);

        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (line[0] != '#') {
            continue;
        }

        cmd[0] = '\0';
        arg1[0] = '\0';

        count = sscanf(line, "%31s %63s", cmd, arg1);
        if (count >= 2) {
            if (strcmp(cmd, "#label") == 0 && strcmp(arg1, label_name) == 0) {
                debug_log("SCRIPT JUMP LABEL line=%d label=%s",
                          script_line != 0 ? *script_line : 0,
                          label_name);
                /*
                 * ここで見つかった時点で、
                 * fp は #label 行の次の行を指している。
                 * そのまま戻れば、次の fgets() から本編再開できる。
                 */
                return 1;
            }
        }
    }

    return 0;
}

/* target_line を読み終えた状態にし、次の read_script_line() から再開する。 */
static int seek_script_after_line(const ScriptContext *ctx,
                                  FILE *fp,
                                  int *script_line,
                                  int target_line)
{
    char line[256];

    if (fp == 0 || script_line == 0 || target_line < 0) {
        return 0;
    }

    fseek(fp, 0L, SEEK_SET);
    *script_line = 0;

    while (*script_line < target_line) {
        if (!read_script_line(ctx, fp, line, sizeof(line), script_line)) {
            return 0;
        }
    }

    return 1;
}

// [名前] から名前部分だけ取り出す
static void extract_name_from_brackets(const char *line, char *out_name, int out_size)
{
    int i;
    int j;

    j = 0;

    for (i = 1; line[i] != '\0' && line[i] != ']'; ++i) {
        if (j < out_size - 1) {
            out_name[j++] = line[i];
        }
    }

    out_name[j] = '\0';
}

static void resume_script_line(const ScriptContext *ctx,
                               FILE *fp,
                               int *script_line,
                               char *current_name,
                               int current_name_size)
{
    char line[256];
    int target_line;

    if (fp == 0 || script_line == 0 || current_name == 0 || current_name_size <= 0) {
        return;
    }

    target_line = ctx->state->script_line;

    fseek(fp, 0L, SEEK_SET);
    *script_line = 0;
    current_name[0] = '\0';

    while (*script_line < target_line - 1) {
        if (!read_script_line(ctx, fp, line, sizeof(line), script_line)) {
            break;
        }

        remove_newline(line);
        trim_leading_spaces(line);

        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (line[0] == '[') {
            extract_name_from_brackets(line, current_name, current_name_size);
        }
    }

    debug_log("LOAD RESUME line=%d", target_line);
}

static void handle_choice_block(const ScriptContext *ctx, FILE *fp, int *script_line)
{
    int choice_count;
    int choice_start_line;
    int choice_end_line;
    int selected;
    char choice_work_line[256];

    choice_start_line = *script_line;

    ctx->reset_choice_lines();
    choice_count = 0;

    while (read_script_line(ctx, fp, choice_work_line, 256, script_line)) {
        remove_newline(choice_work_line);
        trim_leading_spaces(choice_work_line);

        if (choice_work_line[0] == '\0' ||
            choice_work_line[0] == ';') {
            continue;
        }

        if (strcmp(choice_work_line, "#endchoice") == 0) {
            break;
        }

        if (choice_count < 6) {
            ctx->store_choice_line(choice_count, choice_work_line);
            choice_count++;
        }
    }

    choice_end_line = *script_line;

    if (choice_count < 2) {
        ctx->state->script_line = choice_end_line;
        return;
    }

    ctx->state->script_line = choice_start_line;
    selected = ctx->wait_choice(choice_count, 1);
    if (selected == CHOICE_RESULT_LOAD_RESUME) {
        return;
    }

    ctx->state->script_line = choice_end_line;
    set_flag_value(ctx, "choice", selected);
}

// 文字列を立ち絵IDへ
static enum StandId parse_stand_id(const char *name)
{
    int stand_no;

    if (strcmp(name, "none") == 0) {
        return STAND_NONE;
    }

    if (strncmp(name, "character", 9) == 0) {
        if (sscanf(name + 9, "%d", &stand_no) == 1) {
            if (stand_no >= 1 && stand_no <= 20) {
                return (enum StandId)stand_no;
            }
        }
    }

    /* 古いスクリプト用の別名。不要なら後で消してOK */
    /*
    if (strcmp(name, "anzai") == 0) {
        return STAND_CHARACTER01;
    }
    if (strcmp(name, "mitsui") == 0) {
        return STAND_CHARACTER02;
    }
    if (strcmp(name, "sakuragi") == 0) {
        return STAND_CHARACTER03;
    }
    */

    return STAND_NONE;
}

// 文字列を表情IDへ
static enum FaceId parse_face_id(const char *name)
{
    if (strcmp(name, "happy") == 0) {
        return FACE_HAPPY;
    }
    if (strcmp(name, "angry") == 0) {
        return FACE_ANGRY;
    }
    if (strcmp(name, "surprised") == 0) {
        return FACE_SURPRISED;
    }

    return FACE_NORMAL;
}

// コマンド行を読む関数
static void __attribute__((optimize("Os")))
process_command_line(const ScriptContext *ctx,
                     const char *line,
                     char *bg_name,
                     enum StandId *left_stand,
                     enum FaceId *left_face,
                     enum StandId *right_stand,
                     enum FaceId *right_face)
{
    char cmd[32];
    char arg1[32];
    char arg2[32];
    char arg3[32];
    char arg4[32];
    int count;

    cmd[0] = '\0';
    arg1[0] = '\0';
    arg2[0] = '\0';
    arg3[0] = '\0';
    arg4[0] = '\0';

    count = sscanf(line,
                   "%31s %31s %31s %31s %31s",
                   cmd,
                   arg1,
                   arg2,
                   arg3,
                   arg4);

    if (count <= 0) {
        return;
    }

    if (strcmp(cmd, "#bg") == 0 || strcmp(cmd, "#bginterlace") == 0) {
        if (count >= 2) {
            strncpy(bg_name, arg1, 31);
            bg_name[31] = '\0';
        }
        return;
    }

    if (strcmp(cmd, "#msgbox") == 0) {
        int x0;
        int y0;
        int x1;
        int y1;

        if (count >= 5) {
            x0 = atoi(arg1);
            y0 = atoi(arg2);
            x1 = atoi(arg3);
            y1 = atoi(arg4);

            ctx->set_message_box(x0, y0, x1, y1);
        }
        return;
    }



    if (strcmp(cmd, "#left") == 0 || strcmp(cmd, "#leftinterlace") == 0) {
        if (count >= 2) {
            *left_stand = parse_stand_id(arg1);
        }
        if (count >= 3) {
            *left_face = parse_face_id(arg2);
        } else {
            *left_face = FACE_NORMAL;
        }
        return;
    }

    if (strcmp(cmd, "#right") == 0 || strcmp(cmd, "#rightinterlace") == 0) {
        if (count >= 2) {
            *right_stand = parse_stand_id(arg1);
        }
        if (count >= 3) {
            *right_face = parse_face_id(arg2);
        } else {
            *right_face = FACE_NORMAL;
        }
        return;
    }
}

static void parse_command(const char *line, ParsedCommand *command)
{
    command->cmd[0] = '\0';
    command->arg1[0] = '\0';
    command->arg2[0] = '\0';
    command->arg3[0] = '\0';
    command->count = sscanf(line, "%31s %63s %63s %63s",
                            command->cmd, command->arg1,
                            command->arg2, command->arg3);
}

static enum CommandResult handle_control_command(const ScriptContext *ctx,
                                                 FILE *fp,
                                                 int *script_line,
                                                 const ParsedCommand *command)
{
    GameState *state;

    state = ctx->state;

    if (strcmp(command->cmd, "#label") == 0) {
        return COMMAND_HANDLED;
    }

    if (strcmp(command->cmd, "#jump") == 0) {
        if (command->count >= 2 &&
            find_label_and_jump(ctx, fp, script_line, command->arg1)) {
            return COMMAND_JUMPED;
        }
        return COMMAND_HANDLED;
    }

    if (strcmp(command->cmd, "#call") == 0) {
        if (command->count >= 2) {
            long return_file_pos;
            int return_line;

            if (state->call_stack_depth < 0 ||
                state->call_stack_depth > CALL_STACK_MAX) {
                debug_log("SCRIPT CALL invalid stack depth=%d; reset",
                          state->call_stack_depth);
                state->call_stack_depth = 0;
            }

            if (state->call_stack_depth >= CALL_STACK_MAX) {
                debug_log("SCRIPT CALL stack full label=%s", command->arg1);
                return COMMAND_HANDLED;
            }

            return_file_pos = ftell(fp);
            return_line = *script_line;

            if (find_label_and_jump(ctx, fp, script_line, command->arg1)) {
                state->call_stack[state->call_stack_depth] = return_line;
                state->call_stack_depth++;
                debug_log("SCRIPT CALL label=%s return_line=%d depth=%d",
                          command->arg1, return_line,
                          state->call_stack_depth);
                return COMMAND_JUMPED;
            }

            if (return_file_pos >= 0) {
                fseek(fp, return_file_pos, SEEK_SET);
            } else {
                seek_script_after_line(ctx, fp, script_line, return_line);
            }
            *script_line = return_line;
            state->script_line = *script_line;
            debug_log("SCRIPT CALL label not found: %s", command->arg1);
        } else {
            debug_log("SCRIPT CALL missing label");
        }
        return COMMAND_HANDLED;
    }

    if (strcmp(command->cmd, "#return") == 0) {
        int return_line;

        if (state->call_stack_depth <= 0 ||
            state->call_stack_depth > CALL_STACK_MAX) {
            debug_log("SCRIPT RETURN without call depth=%d",
                      state->call_stack_depth);
            if (state->call_stack_depth < 0 ||
                state->call_stack_depth > CALL_STACK_MAX) {
                state->call_stack_depth = 0;
            }
            return COMMAND_HANDLED;
        }

        return_line = state->call_stack[state->call_stack_depth - 1];
        if (seek_script_after_line(ctx, fp, script_line, return_line)) {
            state->call_stack_depth--;
            state->script_line = *script_line;
            debug_log("SCRIPT RETURN line=%d depth=%d",
                      return_line, state->call_stack_depth);
            return COMMAND_JUMPED;
        }
        debug_log("SCRIPT RETURN invalid line=%d", return_line);
        return COMMAND_HANDLED;
    }

    return COMMAND_NOT_HANDLED;
}

static enum CommandResult handle_flag_command(const ScriptContext *ctx,
                                              FILE *fp,
                                              int *script_line,
                                              const ParsedCommand *command)
{
    if (strcmp(command->cmd, "#set") == 0) {
        if (command->count >= 2) {
            set_flag_on(ctx, command->arg1);
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#reset") == 0) {
        if (command->count >= 2) {
            set_flag_off(ctx, command->arg1);
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#if") == 0) {
        if (command->count >= 3 && is_flag_on(ctx, command->arg1)) {
            if (find_label_and_jump(ctx, fp, script_line, command->arg2)) {
                return COMMAND_JUMPED;
            }
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#ifnot") == 0) {
        if (command->count >= 3 && !is_flag_on(ctx, command->arg1)) {
            if (find_label_and_jump(ctx, fp, script_line, command->arg2)) {
                return COMMAND_JUMPED;
            }
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#add") == 0) {
        if (command->count >= 3) {
            add_flag_value(ctx, command->arg1, atoi(command->arg2));
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#setnum") == 0) {
        if (command->count >= 3) {
            set_flag_value(ctx, command->arg1, atoi(command->arg2));
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#ifge") == 0) {
        if (command->count >= 4 &&
            get_flag_value(ctx, command->arg1) >= atoi(command->arg2)) {
            if (find_label_and_jump(ctx, fp, script_line, command->arg3)) {
                return COMMAND_JUMPED;
            }
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#ifeq") == 0) {
        if (command->count >= 4 &&
            get_flag_value(ctx, command->arg1) == atoi(command->arg2)) {
            if (find_label_and_jump(ctx, fp, script_line, command->arg3)) {
                return COMMAND_JUMPED;
            }
        }
        return COMMAND_HANDLED;
    }

    return COMMAND_NOT_HANDLED;
}

static int parse_fm_se_number(const char *text, int *number)
{
    int value;
    int i;

    if (text == 0 || text[0] == '\0' || number == 0) {
        return 0;
    }

    value = 0;
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
        if (value > 127) {
            return 0;
        }
    }

    *number = value;
    return 1;
}

static enum CommandResult handle_external_command(const ScriptContext *ctx,
                                                  const ParsedCommand *command,
                                                  SceneRenderState *render)
{
    if (strcmp(command->cmd, "#pal") == 0) {
        if (command->count >= 2) {
            if (graph98_load_palette_file(command->arg1)) {
                render->scene_dirty = 1;
            } else {
                debug_log("palette load failed: %s", command->arg1);
            }
        }
        return COMMAND_HANDLED;
    }

    if (strcmp(command->cmd, "#se") == 0) {
        int se_number;

        if (ctx->fm_se_loaded && command->count >= 2 &&
            parse_fm_se_number(command->arg1, &se_number)) {
            pmd_play_fm_se((uint8_t)se_number);
        }
        return COMMAND_HANDLED;
    }

    if (strcmp(command->cmd, "#bgm") == 0) {
        if (command->count >= 2) {
            if (strcmp(ctx->state->bgm, command->arg1) == 0) {
                return COMMAND_HANDLED;
            }
            strcpy(ctx->state->bgm, command->arg1);
            if (ctx->pmd_available) {
                pmd_stop_music();
                if (pmd_load_music_file(command->arg1)) {
                    pmd_start_music();
                } else {
                    debug_log("bgm load failed: %s", command->arg1);
                }
            }
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#bgmstart") == 0) {
        if (ctx->pmd_available) {
            pmd_start_music();
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#bgmstop") == 0) {
        if (ctx->pmd_available) {
            pmd_stop_music();
        }
        return COMMAND_HANDLED;
    }
    if (strcmp(command->cmd, "#bgmfade") == 0) {
        if (ctx->pmd_available) {
            pmd_fadeout_music();
        }
        return COMMAND_HANDLED;
    }

    return COMMAND_NOT_HANDLED;
}

static enum CommandResult __attribute__((optimize("Os")))
handle_display_command(const ScriptContext *ctx,
                       const char *line,
                       const ParsedCommand *command,
                       SceneRenderState *render)
{
    char old_bg_name[32];
    enum StandId old_left_stand;
    enum FaceId old_left_face;
    enum StandId old_right_stand;
    enum FaceId old_right_face;
    GameState *state;

    if (strcmp(command->cmd, "#bg") != 0 &&
        strcmp(command->cmd, "#bginterlace") != 0 &&
        strcmp(command->cmd, "#left") != 0 &&
        strcmp(command->cmd, "#leftinterlace") != 0 &&
        strcmp(command->cmd, "#right") != 0 &&
        strcmp(command->cmd, "#rightinterlace") != 0 &&
        strcmp(command->cmd, "#msgbox") != 0) {
        return COMMAND_NOT_HANDLED;
    }

    state = ctx->state;
    strncpy(old_bg_name, state->bg_name, sizeof(old_bg_name) - 1);
    old_bg_name[sizeof(old_bg_name) - 1] = '\0';
    old_left_stand = state->left_stand;
    old_left_face = state->left_face;
    old_right_stand = state->right_stand;
    old_right_face = state->right_face;

    if (strcmp(command->cmd, "#bginterlace") == 0) {
        render->bg_interlace_pending = 1;
        if (command->count < 2) {
            state->bg_name[0] = '\0';
            render->scene_dirty = 1;
        }
    } else if (strcmp(command->cmd, "#bg") == 0) {
        render->bg_interlace_pending = 0;
        if (command->count < 2) {
            state->bg_name[0] = '\0';
            render->scene_dirty = 1;
        }
    } else if (command->count >= 2) {
        if (strcmp(command->cmd, "#leftinterlace") == 0) {
            render->left_interlace_pending = 1;
        } else if (strcmp(command->cmd, "#left") == 0) {
            render->left_interlace_pending = 0;
        } else if (strcmp(command->cmd, "#rightinterlace") == 0) {
            render->right_interlace_pending = 1;
        } else if (strcmp(command->cmd, "#right") == 0) {
            render->right_interlace_pending = 0;
        }
    }

    process_command_line(ctx, line, state->bg_name,
                         &state->left_stand, &state->left_face,
                         &state->right_stand, &state->right_face);

    if (render->bg_interlace_pending ||
        strcmp(state->bg_name, old_bg_name) != 0) {
        render->scene_dirty = 1;
        render->stand_dirty = 0;
    } else if (state->left_stand != old_left_stand ||
               state->left_face != old_left_face ||
               state->right_stand != old_right_stand ||
               state->right_face != old_right_face ||
               render->left_interlace_pending ||
               render->right_interlace_pending) {
        render->stand_dirty = 1;
    }

    return COMMAND_HANDLED;
}

static void __attribute__((optimize("Os")))
draw_full_scene(const ScriptContext *ctx,
                SceneRenderState *render,
                int use_background_effect)
{
    GameState *state;

    state = ctx->state;
    if (use_background_effect && render->bg_interlace_pending) {
        ctx->draw_background_interlace(state->bg_name);
    } else {
        ctx->draw_background(state->bg_name);
    }
    if (render->left_interlace_pending) {
        ctx->draw_stand_interlace(state->left_stand, state->left_face,
                                  STAND_LEFT_X, STAND_Y, 0);
    } else {
        ctx->draw_stand(state->left_stand, state->left_face,
                        STAND_LEFT_X, STAND_Y, 0);
    }
    if (render->right_interlace_pending) {
        ctx->draw_stand_interlace(state->right_stand, state->right_face,
                                  STAND_RIGHT_X, STAND_Y, 1);
    } else {
        ctx->draw_stand(state->right_stand, state->right_face,
                        STAND_RIGHT_X, STAND_Y, 1);
    }

    strncpy(render->last_bg_name, state->bg_name,
            sizeof(render->last_bg_name) - 1);
    render->last_bg_name[sizeof(render->last_bg_name) - 1] = '\0';
    render->last_left_stand = state->left_stand;
    render->last_left_face = state->left_face;
    render->last_right_stand = state->right_stand;
    render->last_right_face = state->right_face;
}

static void __attribute__((optimize("Os")))
redraw_scene_if_needed(const ScriptContext *ctx,
                       SceneRenderState *render)
{
    GameState *state;

    state = ctx->state;
    if (render->scene_dirty ||
        strcmp(render->last_bg_name, state->bg_name) != 0) {
        draw_full_scene(ctx, render, 1);
        render->scene_dirty = 0;
        render->stand_dirty = 0;
        render->bg_interlace_pending = 0;
        render->left_interlace_pending = 0;
        render->right_interlace_pending = 0;
        return;
    }

    if (!render->stand_dirty) {
        return;
    }

    if ((state->left_stand != render->last_left_stand ||
         state->left_face != render->last_left_face) &&
        state->right_stand == render->last_right_stand &&
        state->right_face == render->last_right_face) {
        if (render->left_interlace_pending) {
            ctx->refresh_left_stand_only_interlace(state->bg_name,
                                                   state->left_stand,
                                                   state->left_face);
        } else {
            ctx->refresh_left_stand_only(state->bg_name,
                                         state->left_stand,
                                         state->left_face);
        }
        render->last_left_stand = state->left_stand;
        render->last_left_face = state->left_face;
    } else if (state->left_stand == render->last_left_stand &&
               state->left_face == render->last_left_face &&
               (state->right_stand != render->last_right_stand ||
                state->right_face != render->last_right_face)) {
        if (render->right_interlace_pending) {
            ctx->refresh_right_stand_only_interlace(state->bg_name,
                                                    state->right_stand,
                                                    state->right_face);
        } else {
            ctx->refresh_right_stand_only(state->bg_name,
                                          state->right_stand,
                                          state->right_face);
        }
        render->last_right_stand = state->right_stand;
        render->last_right_face = state->right_face;
    } else {
        /* The fallback redraw uses the normal background path. */
        draw_full_scene(ctx, render, 0);
    }

    render->stand_dirty = 0;
    render->left_interlace_pending = 0;
    render->right_interlace_pending = 0;
}

// 日本語 script.txt を再生する関数
enum GameResult run_script_sjis(const ScriptContext *ctx)
{
    FILE *fp;
    char line[256];
    char current_name[128];
    int script_line;
    GameState *state;

    uint16_t name_jis[64];
    uint16_t text_jis[128];

    int name_len;
    int text_len;

    SceneRenderState render;

    if (ctx == 0 || ctx->state == 0 || ctx->flags == 0 ||
        ctx->request_scene_redraw == 0 ||
        ctx->request_script_resume == 0 ||
        ctx->system_action == 0 ||
        ctx->draw_background_interlace == 0 ||
        ctx->draw_stand_interlace == 0 ||
        ctx->refresh_left_stand_only_interlace == 0 ||
        ctx->refresh_right_stand_only_interlace == 0) {
        return GAME_RESULT_EXIT_TO_DOS;
    }

    state = ctx->state;

    render.stand_dirty = 0;
    render.bg_interlace_pending = 0;
    render.left_interlace_pending = 0;
    render.right_interlace_pending = 0;
    script_line = 0;

    current_name[0] = '\0';

    if (!*ctx->request_script_resume) {
        memset(ctx->flags, 0, sizeof(GameFlag) * MAX_FLAGS);
        memset(state, 0, sizeof(*state));
        state->left_stand = STAND_NONE;
        state->right_stand = STAND_NONE;
        state->left_face = FACE_NORMAL;
        state->right_face = FACE_NORMAL;
    }

    render.last_bg_name[0] = '\0';
    render.last_left_stand = STAND_NONE;
    render.last_right_stand = STAND_NONE;
    render.last_left_face = FACE_NORMAL;
    render.last_right_face = FACE_NORMAL;
    render.scene_dirty = 1;

    fp = fopen("script.txt", "rb");
    if (fp == 0) {
        debug_log("script.txt not found.");
        return GAME_RESULT_EXIT_TO_DOS;
    }

    for (;;) {
        if (*ctx->request_script_resume) {
            resume_script_line(ctx, fp, &script_line,
                               current_name, sizeof(current_name));
            render.scene_dirty = 1;
            render.stand_dirty = 0;
            render.bg_interlace_pending = 0;
            render.left_interlace_pending = 0;
            render.right_interlace_pending = 0;
            *ctx->request_scene_redraw = 0;
            *ctx->request_script_resume = 0;
        }

        if (!read_script_line(ctx, fp, line, sizeof(line), &script_line)) {
            break;
        }

        state->script_line = script_line;
        remove_newline(line);
        trim_leading_spaces(line);
        /* 空行・コメント行スキップ */
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (line[0] == '#') {
            ParsedCommand command;

            parse_command(line, &command);

            /*
             * Command dispatch (commands currently used by script.txt):
             *   here:     #choice, #endchoice
             *   control:  #label, #jump, #call, #return
             *   flags:    #setnum, #ifeq
             *   external: #pal, #bgm
             *   display:  #bg, #bginterlace, #left, #leftinterlace,
             *             #right, #rightinterlace
             * Other supported commands are routed through the same groups.
             */

            if (strcmp(command.cmd, "#choice") == 0) {
                handle_choice_block(ctx, fp, &script_line);
                if (*ctx->system_action != SYSTEM_ACTION_NONE) {
                    break;
                }
                continue;
            }

            if (strcmp(command.cmd, "#endchoice") == 0) {
                continue;
            }

            if (handle_control_command(ctx, fp, &script_line, &command) !=
                COMMAND_NOT_HANDLED) {
                continue;
            }

            if (handle_flag_command(ctx, fp, &script_line, &command) !=
                COMMAND_NOT_HANDLED) {
                continue;
            }

            if (handle_external_command(ctx, &command, &render) !=
                COMMAND_NOT_HANDLED) {
                continue;
            }

            handle_display_command(ctx, line, &command, &render);
            continue;
        }

        if (line[0] == '[') {
            extract_name_from_brackets(line, current_name, sizeof(current_name));
            continue;
        }

        name_len = convert_message_text_sjis_to_jis_array(
            (const unsigned char *)current_name,
            name_jis,
            64
        );

        text_len = convert_message_text_sjis_to_jis_array(
            (const unsigned char *)line,
            text_jis,
            128
        );

        /* script.txt 末尾の空行で空メッセージが出るのを防ぐ */
        if (text_len <= 0) {
            continue;
        }

        redraw_scene_if_needed(ctx, &render);
        ctx->draw_message_jis(name_jis, name_len, text_jis, text_len);

        if (*ctx->system_action != SYSTEM_ACTION_NONE) {
            break;
        }

        if (*ctx->request_scene_redraw) {
            render.scene_dirty = 1;
            render.stand_dirty = 0;
            *ctx->request_scene_redraw = 0;
        }
    }

    fclose(fp);

    if (*ctx->system_action == SYSTEM_ACTION_EXIT) {
        return GAME_RESULT_EXIT_TO_DOS;
    }
    if (*ctx->system_action == SYSTEM_ACTION_TITLE) {
        return GAME_RESULT_RETURN_TO_TITLE;
    }

    return GAME_RESULT_SCRIPT_END;
}
