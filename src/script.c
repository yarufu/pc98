#include "script.h"

#include "graph98.h"
#include "pmd.h"
#include "text98.h"

#include <stdlib.h>
#include <string.h>

#define STAND_LEFT_X   60
#define STAND_RIGHT_X  320
#define STAND_Y        9
#define CHOICE_RESULT_LOAD_RESUME 0

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
                ctx->debug_log("SCRIPT JUMP LABEL line=%d label=%s",
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

// フラグ関数
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

static void set_flag_on(const ScriptContext *ctx, const char *name)
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

static void set_flag_off(const ScriptContext *ctx, const char *name)
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

static int is_flag_on(const ScriptContext *ctx, const char *name)
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

static GameFlag *find_or_create_flag(const ScriptContext *ctx, const char *name)
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

static void add_flag_value(const ScriptContext *ctx, const char *name, int value)
{
    GameFlag *flag;

    flag = find_or_create_flag(ctx, name);

    if (flag != 0) {
        flag->value += value;
    }
}

static void set_flag_value(const ScriptContext *ctx, const char *name, int value)
{
    GameFlag *flag;

    flag = find_or_create_flag(ctx, name);

    if (flag != 0) {
        flag->value = value;
    }
}

static int get_flag_value(const ScriptContext *ctx, const char *name)
{
    GameFlag *flag;

    flag = find_flag(ctx, name);

    if (flag == 0) {
        return 0;
    }

    return flag->value;
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

    ctx->debug_log("LOAD RESUME line=%d", target_line);
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
static void process_command_line(const ScriptContext *ctx,
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

    if (strcmp(cmd, "#bg") == 0 || strcmp(cmd, "#bgwipe") == 0) {
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



    if (strcmp(cmd, "#left") == 0 || strcmp(cmd, "#leftwipe") == 0) {
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

    if (strcmp(cmd, "#right") == 0 || strcmp(cmd, "#rightwipe") == 0) {
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

    char last_bg_name[32];
    enum StandId last_left_stand;
    enum StandId last_right_stand;
    enum FaceId last_left_face;
    enum FaceId last_right_face;
    int scene_dirty;

    int stand_dirty;
    int bg_wipe_pending;
    int left_wipe_pending;
    int right_wipe_pending;

    if (ctx == 0 || ctx->state == 0 || ctx->flags == 0 ||
        ctx->request_scene_redraw == 0 ||
        ctx->request_script_resume == 0 ||
        ctx->system_action == 0) {
        return GAME_RESULT_EXIT_TO_DOS;
    }

    state = ctx->state;

    stand_dirty = 0;
    bg_wipe_pending = 0;
    left_wipe_pending = 0;
    right_wipe_pending = 0;
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

    last_bg_name[0] = '\0';
    last_left_stand = STAND_NONE;
    last_right_stand = STAND_NONE;
    last_left_face = FACE_NORMAL;
    last_right_face = FACE_NORMAL;
    scene_dirty = 1;

    fp = fopen("script.txt", "rb");
    if (fp == 0) {
        ctx->debug_log("script.txt not found.");
        return GAME_RESULT_EXIT_TO_DOS;
    }

    for (;;) {
        if (*ctx->request_script_resume) {
            resume_script_line(ctx, fp, &script_line,
                               current_name, sizeof(current_name));
            scene_dirty = 1;
            stand_dirty = 0;
            bg_wipe_pending = 0;
            left_wipe_pending = 0;
            right_wipe_pending = 0;
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
            char cmd[32];
            char arg1[64];
            char arg2[64];
            char arg3[64];
            int count;

            cmd[0] = '\0';
            arg1[0] = '\0';
            arg2[0] = '\0';
            arg3[0] = '\0';

            count = sscanf(line, "%31s %63s %63s %63s",
                           cmd, arg1, arg2, arg3);

            if (strcmp(cmd, "#choice") == 0) {
                handle_choice_block(ctx, fp, &script_line);
                if (*ctx->system_action != SYSTEM_ACTION_NONE) {
                    break;
                }
                continue;
            }

            if (strcmp(cmd, "#endchoice") == 0) {
                continue;
            }

            if (strcmp(cmd, "#label") == 0) {
                continue;
            }

            if (strcmp(cmd, "#jump") == 0) {
                if (count >= 2) {
                    find_label_and_jump(ctx, fp, &script_line, arg1);
                }
                continue;
            }

            if (strcmp(cmd, "#call") == 0) {
                if (count >= 2) {
                    long return_file_pos;
                    int return_line;

                    if (state->call_stack_depth < 0 ||
                        state->call_stack_depth > CALL_STACK_MAX) {
                        ctx->debug_log("SCRIPT CALL invalid stack depth=%d; reset",
                                       state->call_stack_depth);
                        state->call_stack_depth = 0;
                    }

                    if (state->call_stack_depth >= CALL_STACK_MAX) {
                        ctx->debug_log("SCRIPT CALL stack full label=%s", arg1);
                        continue;
                    }

                    return_file_pos = ftell(fp);
                    return_line = script_line;

                    if (find_label_and_jump(ctx, fp, &script_line, arg1)) {
                        state->call_stack[state->call_stack_depth] = return_line;
                        state->call_stack_depth++;
                        ctx->debug_log("SCRIPT CALL label=%s return_line=%d depth=%d",
                                       arg1, return_line,
                                       state->call_stack_depth);
                    } else {
                        if (return_file_pos >= 0) {
                            fseek(fp, return_file_pos, SEEK_SET);
                        } else {
                            seek_script_after_line(ctx, fp, &script_line, return_line);
                        }
                        script_line = return_line;
                        state->script_line = script_line;
                        ctx->debug_log("SCRIPT CALL label not found: %s", arg1);
                    }
                } else {
                    ctx->debug_log("SCRIPT CALL missing label");
                }
                continue;
            }

            if (strcmp(cmd, "#return") == 0) {
                int return_line;

                if (state->call_stack_depth <= 0 ||
                    state->call_stack_depth > CALL_STACK_MAX) {
                    ctx->debug_log("SCRIPT RETURN without call depth=%d",
                                   state->call_stack_depth);
                    if (state->call_stack_depth < 0 ||
                        state->call_stack_depth > CALL_STACK_MAX) {
                        state->call_stack_depth = 0;
                    }
                    continue;
                }

                return_line =
                    state->call_stack[state->call_stack_depth - 1];
                if (seek_script_after_line(ctx, fp, &script_line, return_line)) {
                    state->call_stack_depth--;
                    state->script_line = script_line;
                    ctx->debug_log("SCRIPT RETURN line=%d depth=%d",
                                   return_line, state->call_stack_depth);
                } else {
                    ctx->debug_log("SCRIPT RETURN invalid line=%d", return_line);
                }
                continue;
            }

            if (strcmp(cmd, "#set") == 0) {
                if (count >= 2) {
                    set_flag_on(ctx, arg1);
                }
                continue;
            }

            if (strcmp(cmd, "#reset") == 0) {
                if (count >= 2) {
                    set_flag_off(ctx, arg1);
                }
                continue;
            }

            if (strcmp(cmd, "#if") == 0) {
                if (count >= 3) {
                    if (is_flag_on(ctx, arg1)) {
                        find_label_and_jump(ctx, fp, &script_line, arg2);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#ifnot") == 0) {
                if (count >= 3) {
                    if (!is_flag_on(ctx, arg1)) {
                        find_label_and_jump(ctx, fp, &script_line, arg2);
                    }
                }
                continue;
            }

            if (strcmp(cmd,"#add") == 0) {
                if (count >= 3) {
                    add_flag_value(ctx, arg1, atoi(arg2));
                }
                continue;
            }

            if (strcmp(cmd, "#setnum") == 0) {
                if (count >= 3) {
                    set_flag_value(ctx, arg1, atoi(arg2));
                }
                continue;
            }

            if (strcmp(cmd, "#ifge") == 0) {
                if (count >= 4) {
                    if (get_flag_value(ctx, arg1) >= atoi(arg2)) {
                        find_label_and_jump(ctx, fp, &script_line, arg3);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#ifeq") == 0) {
                if (count >= 4) {
                    if (get_flag_value(ctx, arg1) == atoi(arg2)) {
                        find_label_and_jump(ctx, fp, &script_line, arg3);
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#pal") == 0) {
                if (count >= 2) {
                    // printf("[PAL] load: %s\n", arg1);
                    if (graph98_load_palette_file(arg1)) {
                        // printf("[PAL] success\n");
                        scene_dirty = 1;
                    } else {
                        ctx->debug_log("palette load failed: %s", arg1);
                        // printf("[PAL] FAILED\n");
                    }
                } else {
                    // printf("[PAL] invalid args\n");
                }
                continue;
            }

            // #seはコメントアウト
            // BGM/SEはPMD(#bgm)に任せるため
            /*
            if (strcmp(cmd, "#se") == 0) {
                if (count >= 2) {
                    if (strcmp(arg1, "beep") == 0) {
                        se86_play_beep();
                    } else if (strcmp(arg1, "beep2") == 0) {
                        se86_play_beep2();
                    }
                }
                continue;
            }
            */
            // 未知コマンド扱いで下に流れる可能性があるので、一応#seを無視する処理は入れておく
            if (strcmp(cmd, "#se") == 0) {
                continue;
            }

            if (strcmp(cmd, "#bgm") == 0) {
                if (count >= 2) {

                    if (strcmp(state->bgm, arg1) == 0) {
                        continue;
                    }

                    strcpy(state->bgm, arg1);

                    if (ctx->pmd_available) {

                        pmd_stop_music();

                        if (pmd_load_music_file(arg1)) {
                            pmd_start_music();
                        }else{
                            ctx->debug_log("bgm load failed: %s", arg1);
                        }
                    }
                }
                continue;
            }

            if (strcmp(cmd, "#bgmstart") == 0) {
                if (ctx->pmd_available) {
                    pmd_start_music();
                }
                continue;
            }

            if (strcmp(cmd, "#bgmstop") == 0) {
                if (ctx->pmd_available) {
                    pmd_stop_music();
                }
                continue;
            }

            if (strcmp(cmd, "#bgmfade") == 0) {
                if (ctx->pmd_available) {
                    pmd_fadeout_music();
                }
                continue;
            }

            {
                char old_bg_name[32];
                enum StandId old_left_stand;
                enum FaceId old_left_face;
                enum StandId old_right_stand;
                enum FaceId old_right_face;

                strncpy(old_bg_name, state->bg_name, sizeof(old_bg_name) - 1);
                old_bg_name[sizeof(old_bg_name) - 1] = '\0';
                old_left_stand = state->left_stand;
                old_left_face = state->left_face;
                old_right_stand = state->right_stand;
                old_right_face = state->right_face;

                if (count >= 2) {
                    if (strcmp(cmd, "#bgwipe") == 0) {
                        bg_wipe_pending = 1;
                    } else if (strcmp(cmd, "#bg") == 0) {
                        bg_wipe_pending = 0;
                    } else if (strcmp(cmd, "#leftwipe") == 0) {
                        left_wipe_pending = 1;
                    } else if (strcmp(cmd, "#left") == 0) {
                        left_wipe_pending = 0;
                    } else if (strcmp(cmd, "#rightwipe") == 0) {
                        right_wipe_pending = 1;
                    } else if (strcmp(cmd, "#right") == 0) {
                        right_wipe_pending = 0;
                    }
                }

                process_command_line(ctx, line,
                                     state->bg_name,
                                     &state->left_stand,
                                     &state->left_face,
                                     &state->right_stand,
                                     &state->right_face);

                if (bg_wipe_pending || strcmp(state->bg_name, old_bg_name) != 0) {
                    scene_dirty = 1;
                    stand_dirty = 0;
                } else if (state->left_stand != old_left_stand ||
                           state->left_face != old_left_face ||
                           state->right_stand != old_right_stand ||
                           state->right_face != old_right_face ||
                           left_wipe_pending ||
                           right_wipe_pending) {
                    stand_dirty = 1;
                }
            }
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

        // 変化があれば部分的に再描画
        if (scene_dirty || strcmp(last_bg_name, state->bg_name) != 0) {

            if (bg_wipe_pending) {
                ctx->draw_background_center_wipe(state->bg_name);
            } else {
                ctx->draw_background(state->bg_name);
            }
            if (left_wipe_pending) {
                ctx->draw_stand_center_wipe(state->left_stand, state->left_face,
                                            STAND_LEFT_X, STAND_Y, 0);
            } else {
                ctx->draw_stand(state->left_stand, state->left_face,
                                STAND_LEFT_X, STAND_Y, 0);
            }
            if (right_wipe_pending) {
                ctx->draw_stand_center_wipe(state->right_stand, state->right_face,
                                            STAND_RIGHT_X, STAND_Y, 1);
            } else {
                ctx->draw_stand(state->right_stand, state->right_face,
                                STAND_RIGHT_X, STAND_Y, 1);
            }

            strncpy(last_bg_name, state->bg_name, sizeof(last_bg_name) - 1);
            last_bg_name[sizeof(last_bg_name) - 1] = '\0';
            last_left_stand = state->left_stand;
            last_left_face = state->left_face;
            last_right_stand = state->right_stand;
            last_right_face = state->right_face;

            scene_dirty = 0;
            stand_dirty = 0;
            bg_wipe_pending = 0;
            left_wipe_pending = 0;
            right_wipe_pending = 0;

        } else if (stand_dirty) {

            /*
             * 左もしくは右立ち絵だけ部分更新する。
             * どっちの条件に引っかからなかった場合は、安全のため全体再描画に戻す。
             */
            if ((state->left_stand != last_left_stand ||
                state->left_face != last_left_face) &&
                state->right_stand == last_right_stand &&
                state->right_face == last_right_face) {

                // debug_log("LEFT STAND PARTIAL UPDATE");

                if (left_wipe_pending) {
                    ctx->refresh_left_stand_only_wipe(state->bg_name,
                                                      state->left_stand,
                                                      state->left_face);
                } else {
                    ctx->refresh_left_stand_only(state->bg_name,
                                                 state->left_stand,
                                                 state->left_face);
                }

                last_left_stand = state->left_stand;
                last_left_face = state->left_face;

            } else if (state->left_stand == last_left_stand &&
                       state->left_face == last_left_face &&
                       (state->right_stand != last_right_stand ||
                        state->right_face != last_right_face)) {

                // debug_log("RIGHT STAND PARTIAL UPDATE");

                if (right_wipe_pending) {
                    ctx->refresh_right_stand_only_wipe(state->bg_name,
                                                       state->right_stand,
                                                       state->right_face);
                } else {
                    ctx->refresh_right_stand_only(state->bg_name,
                                                  state->right_stand,
                                                  state->right_face);
                }

                last_right_stand = state->right_stand;
                last_right_face = state->right_face;

            } else {

                // debug_log("STAND FULL REDRAW");


                ctx->draw_background(state->bg_name);
                if (left_wipe_pending) {
                    ctx->draw_stand_center_wipe(state->left_stand, state->left_face,
                                                STAND_LEFT_X, STAND_Y, 0);
                } else {
                    ctx->draw_stand(state->left_stand, state->left_face,
                                    STAND_LEFT_X, STAND_Y, 0);
                }
                if (right_wipe_pending) {
                    ctx->draw_stand_center_wipe(state->right_stand, state->right_face,
                                                STAND_RIGHT_X, STAND_Y, 1);
                } else {
                    ctx->draw_stand(state->right_stand, state->right_face,
                                    STAND_RIGHT_X, STAND_Y, 1);
                }

                strncpy(last_bg_name, state->bg_name, sizeof(last_bg_name) - 1);
                last_bg_name[sizeof(last_bg_name) - 1] = '\0';
                last_left_stand = state->left_stand;
                last_left_face = state->left_face;
                last_right_stand = state->right_stand;
                last_right_face = state->right_face;
            }

        stand_dirty = 0;
        left_wipe_pending = 0;
        right_wipe_pending = 0;
        }
        ctx->draw_message_jis(name_jis, name_len, text_jis, text_len);

        if (*ctx->system_action != SYSTEM_ACTION_NONE) {
            break;
        }

        if (*ctx->request_scene_redraw) {
            scene_dirty = 1;
            stand_dirty = 0;
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
