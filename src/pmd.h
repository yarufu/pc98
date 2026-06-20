#ifndef PMD_H
#define PMD_H

#include <stdint.h>

int pmd_is_resident(void);
void pmd_start_music(void);
void pmd_stop_music(void);
void pmd_fadeout_music(void);
void pmd_play_ssg_se(uint8_t no);
void pmd_play_fm_se(uint8_t no);
int pmd_get_music_buffer(uint16_t *seg, uint16_t *ofs);
unsigned long pmd_get_music_buffer_size(void);
int pmd_load_music_file(const char *filename);
int pmd_load_fm_se_file(const char *filename);


#endif
