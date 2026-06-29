#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

typedef struct {
    int *mouse_available;

    void (*hide_message_window_until_resume)(void);
    void (*draw_choice_jis)(int choice_count, int selected);
    int (*open_system_menu)(void);
    int (*open_system_menu_from_choice)(int choice_count, int selected,
                                        long *mouse_accum_x,
                                        long *mouse_accum_y);
} InputContext;

void input_init(const InputContext *ctx);

int input_key_available(void);
uint8_t input_read_key(uint8_t *key_code);
int input_wait_key(void);
int input_is_system_menu_key(uint8_t key_code);
int input_get_direction(uint8_t ch, uint8_t key_code);
int input_wait_choice_jis(int choice_count, int allow_save_load)
    __attribute__((noinline,optimize("O0")));

#endif
