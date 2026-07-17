#ifndef GRAPH98_H
#define GRAPH98_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GRAPH98_WIDTH = 640,
    GRAPH98_HEIGHT = 400,
    GRAPH98_COLORS = 16
};

void graph98_init(void);
void graph98_apply_adv_palette(void);
int graph98_load_palette_file(const char *path);
void graph98_pset(int x, int y, unsigned char color);
void graph98_hline(int x0, int x1, int y, unsigned char color);
void graph98_vline(int x, int y0, int y1, unsigned char color);
void graph98_rect(int x0, int y0, int x1, int y1, unsigned char color);
void graph98_boxfill(int x0, int y0, int x1, int y1, unsigned char color);
void graph98_clear(unsigned char color);
int graph98_load_g98(const char *path);
int graph98_load_g98_interlace(const char *path);
int graph98_draw_scene_file_trans_vram(
    const char *background_path,
    const char *left_sprite_path,
    const char *right_sprite_path,
    int left_x,
    int right_x,
    int stand_y,
    unsigned char transparent_color);
int graph98_prepare_rect_back_vram(int x0, int y0, int x1, int y1);
int graph98_present_rect_back_vram(int x0, int y0, int x1, int y1);
void graph98_restore_default_pages(void);
int graph98_draw_sprite_file_trans(const char *path, int x, int y,
                                   unsigned char transparent_color);
int graph98_draw_status_digits_file(const char *path,
                                    int time_x, int time_y,
                                    int money_x, int money_y,
                                    int hour, int minute, int money);
int graph98_draw_stand_file_trans_interlace(
    const char *background_path, const char *sprite_path,
    int x, int y, unsigned char transparent_color);
int graph98_draw_stand_file_trans_vram(
    const char *background_path, const char *sprite_path,
    int x, int y, unsigned char transparent_color);
void graph98_draw_digit(int x, int y, int digit, unsigned char color);
void graph98_draw_number(int x, int y, int value, unsigned char color);
void graph98_draw_char(int x, int y, char ch, unsigned char color);
void graph98_draw_string(int x, int y, const char *str, unsigned char color);
int graph98_load_g98_rect(const char *path, int x0, int y0, int x1, int y1);
void graph98_wait_vsync(void);

#ifdef __cplusplus
}
#endif

#endif
