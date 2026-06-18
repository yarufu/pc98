#ifndef TEXT98_H
#define TEXT98_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t sjis_to_jis(uint8_t sjis_hi, uint8_t sjis_lo);
void get_kanji_font(uint16_t jis_code, unsigned char *buffer);
void draw_jis_char(int x, int y, const unsigned char *font);
void draw_jis_string(int x, int y, const unsigned char **fonts, int count);
int convert_sjis_string_to_jis_array(const unsigned char *src,
                                     uint16_t *dst,
                                     int max_chars);
int convert_message_text_sjis_to_jis_array(const unsigned char *src,
                                           uint16_t *dst,
                                           int max_chars);

#ifdef __cplusplus
}
#endif

#endif
