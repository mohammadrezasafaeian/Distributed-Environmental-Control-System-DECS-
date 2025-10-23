#ifndef MENU_H
#define MENU_H

#include <stdint.h>

// Public API
void menu_init(void);
void menu_process_key(uint8_t key);
void menu_display(uint16_t adc1[3], uint16_t adc2[3]);
uint8_t menu_is_manual_mode(void);
uint8_t menu_get_last_manual_key(void);  // For main.c to handle manual commands
void menu_clear_manual_key(void);

#endif
