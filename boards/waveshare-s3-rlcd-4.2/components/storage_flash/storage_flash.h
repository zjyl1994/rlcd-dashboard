#ifndef STORAGE_FLASH_H
#define STORAGE_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

bool storage_flash_init(void);
bool storage_flash_save_last_message(const char *text, const struct tm *update_time);
bool storage_flash_load_last_message(char *text, size_t text_size,
                                     struct tm *update_time);

#ifdef __cplusplus
}
#endif

#endif
