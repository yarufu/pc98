#ifndef MOUSE98_H
#define MOUSE98_H

int mouse98_init(void);
int mouse98_left_pressed(void);
void mouse98_wait_left_release(void);
int mouse98_right_pressed(void);
void mouse98_wait_right_release(void);
void mouse98_get_motion(int *dx, int *dy);
void mouse98_hide_cursor(void);

#endif
