#include "input.h"

#include "mouse98.h"

#define CHOICE_COLUMNS 2
#define MOUSE_CHOICE_MOTION_THRESHOLD 16
#define CHOICE_RESULT_LOAD_RESUME 0
#define KEY_CODE_ESCAPE 0x00
#define KEY_CODE_CURSOR_UP 0x3A
#define KEY_CODE_CURSOR_LEFT 0x3B
#define KEY_CODE_CURSOR_RIGHT 0x3C
#define KEY_CODE_CURSOR_DOWN 0x3D
#define KEY_CODE_NUMPAD_8 0x43
#define KEY_CODE_NUMPAD_4 0x46
#define KEY_CODE_NUMPAD_6 0x48
#define KEY_CODE_NUMPAD_2 0x4B
#define KEY_CODE_NUMPAD_0 0x4E

static InputContext g_input;

void input_init(const InputContext *ctx)
{
    g_input = *ctx;
}

// PC-98 キーボード BIOS による入力監視
int input_key_available(void)
{
    uint8_t available;

    __asm__ __volatile__(
        "movb $0x01, %%ah\n\t"
        "int $0x18\n\t"
        "movb %%bh, %0"
        : "=rm"(available)
        :
        : "ax", "bx", "cc", "memory");

    return available != 0;
}

// PC-98 キーボード BIOS による文字データとキーコードの読み取り
uint8_t input_read_key(uint8_t *key_code)
{
    uint16_t key;

    __asm__ __volatile__(
        "xorb %%ah, %%ah\n\t"
        "int $0x18\n\t"
        "movw %%ax, %0"
        : "=rm"(key)
        :
        : "ax", "cc", "memory");

    if (key_code != 0) {
        *key_code = (uint8_t)(key >> 8);
    }

    return (uint8_t)key;
}

int input_is_system_menu_key(uint8_t key_code)
{
    return key_code == KEY_CODE_ESCAPE ||
           key_code == KEY_CODE_NUMPAD_0;
}

int input_get_direction(uint8_t ch, uint8_t key_code)
{
    if (key_code == KEY_CODE_CURSOR_UP ||
        key_code == KEY_CODE_NUMPAD_8 || ch == 0x0B || ch == '8') {
        return -2;
    }
    if (key_code == KEY_CODE_CURSOR_DOWN ||
        key_code == KEY_CODE_NUMPAD_2 || ch == 0x0A || ch == '2') {
        return 2;
    }
    if (key_code == KEY_CODE_CURSOR_LEFT ||
        key_code == KEY_CODE_NUMPAD_4 || ch == 0x08 || ch == '4') {
        return -1;
    }
    if (key_code == KEY_CODE_CURSOR_RIGHT ||
        key_code == KEY_CODE_NUMPAD_6 || ch == 0x0C || ch == '6') {
        return 1;
    }

    return 0;
}

/* マウス（左クリック）、Enterキーが押されるまで待ちます。 */
int input_wait_key(void)
{
    uint8_t ch;
    uint8_t key_code;

    // debug_log("input_wait_key start");

    for (;;) {
        if (*g_input.mouse_available && mouse98_left_pressed()) {
            mouse98_wait_left_release();
            return 1;
        }

        if (*g_input.mouse_available && mouse98_right_pressed()) {
            mouse98_wait_right_release();
            ch = 0;
            key_code = KEY_CODE_ESCAPE;
        } else {
            if (!input_key_available()) {
                continue;
            }

            ch = input_read_key(&key_code);
        }

        if (input_is_system_menu_key(key_code)) {
            return g_input.open_system_menu();
        }

        if (ch == 'H' || ch == 'h') {
            g_input.hide_message_window_until_resume();
            return 0;
        }

        if (ch == 0x0D) {
            return 1;  /* Enter */
        }
    }
    // debug_log("input_wait_key done");
}

int input_wait_choice_jis(int choice_count, int allow_save_load)
{
    uint8_t ch;
    uint8_t key_code;
    int selected;
    int next;
    int mouse_dx;
    int mouse_dy;
    long mouse_accum_x;
    long mouse_accum_y;
    long mouse_abs_x;
    long mouse_abs_y;
    int mouse_direction;
    int keyboard_direction;

    selected = 1;
    mouse_accum_x = 0;
    mouse_accum_y = 0;

    if (*g_input.mouse_available) {
        mouse98_hide_cursor();
        mouse98_get_motion(0, 0);
    }

    g_input.draw_choice_jis(choice_count, selected);

    for (;;) {
        mouse_direction = 0;

        if (*g_input.mouse_available) {
            if (mouse98_left_pressed()) {
                mouse98_wait_left_release();
                return selected;
            }

            if (allow_save_load && mouse98_right_pressed()) {
                mouse98_wait_right_release();

                if (g_input.open_system_menu_from_choice(choice_count,
                                                         selected,
                                                         &mouse_accum_x,
                                                         &mouse_accum_y)) {
                    return CHOICE_RESULT_LOAD_RESUME;
                }
                continue;
            }

            mouse98_get_motion(&mouse_dx, &mouse_dy);
            mouse_accum_x += mouse_dx;
            mouse_accum_y += mouse_dy;
            mouse_abs_x = mouse_accum_x >= 0 ? mouse_accum_x : -mouse_accum_x;
            mouse_abs_y = mouse_accum_y >= 0 ? mouse_accum_y : -mouse_accum_y;

            if (mouse_abs_x >= MOUSE_CHOICE_MOTION_THRESHOLD ||
                mouse_abs_y >= MOUSE_CHOICE_MOTION_THRESHOLD) {
                if (mouse_abs_x >= mouse_abs_y) {
                    mouse_direction = mouse_accum_x > 0 ? 1 : -1;
                } else {
                    mouse_direction = mouse_accum_y > 0 ? 2 : -2;
                }
            }

            if (mouse_direction != 0) {
                mouse_accum_x = 0;
                mouse_accum_y = 0;
            }
        }

        next = selected;

        if (mouse_direction == -2) {
            next = selected - CHOICE_COLUMNS;
        } else if (mouse_direction == 2) {
            next = selected + CHOICE_COLUMNS;
        } else if (mouse_direction == -1) {
            if ((selected - 1) % CHOICE_COLUMNS != 0) {
                next = selected - 1;
            }
        } else if (mouse_direction == 1) {
            if ((selected - 1) % CHOICE_COLUMNS != CHOICE_COLUMNS - 1) {
                next = selected + 1;
            }
        } else if (input_key_available()) {
            ch = input_read_key(&key_code);
            keyboard_direction = input_get_direction(ch, key_code);

            if (ch == 0x0D) {
                return selected;
            }

            if (allow_save_load && input_is_system_menu_key(key_code)) {
                if (g_input.open_system_menu_from_choice(choice_count,
                                                         selected,
                                                         &mouse_accum_x,
                                                         &mouse_accum_y)) {
                    return CHOICE_RESULT_LOAD_RESUME;
                }
                continue;
            }

            if (keyboard_direction == -2) {
                next = selected - CHOICE_COLUMNS;
            } else if (keyboard_direction == 2) {
                next = selected + CHOICE_COLUMNS;
            } else if (keyboard_direction == -1) {
                if ((selected - 1) % CHOICE_COLUMNS != 0) {
                    next = selected - 1;
                }
            } else if (keyboard_direction == 1) {
                if ((selected - 1) % CHOICE_COLUMNS != CHOICE_COLUMNS - 1) {
                    next = selected + 1;
                }
            }
        }

        if (next >= 1 && next <= choice_count && next != selected) {
            selected = next;
            g_input.draw_choice_jis(choice_count, selected);
        }
    }
}
