#include "menu.h"
#include "plant_profiles.h"
#include "node_controller.h"
#include "ssd1306.h"
#include <string.h>
#include <stdio.h>

typedef enum {
    MENU_MAIN,
    MENU_SELECT_NODE,
    MENU_SELECT_PROFILE,
    MENU_VIEW_STATUS,
    MENU_MANUAL_CONTROL
} MenuState_t;

// Private state
static MenuState_t menu_state = MENU_MAIN;
static uint8_t cursor_position = 0;      // Which item cursor is on (0-3 on screen)
static uint8_t scroll_offset = 0;        // First item shown on screen
static uint8_t selected_node = 0;
static uint8_t manual_mode = 0;
static uint8_t last_manual_key = 0;

#define ITEMS_PER_SCREEN 4

static void reset_cursor(void) {
    cursor_position = 0;
    scroll_offset = 0;
}

void menu_init(void) {
    menu_state = MENU_MAIN;
    reset_cursor();
    selected_node = 0;
    manual_mode = 0;
}

void menu_process_key(uint8_t key) {
    static uint8_t last_key = 0;
    static uint32_t last_key_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (key != 0 && key != last_key && (current_time - last_key_time > 200)) {
        last_key = key;
        last_key_time = current_time;

        switch (menu_state) {
            case MENU_MAIN:
                if (key == 1) {
                    menu_state = MENU_VIEW_STATUS;
                    reset_cursor();
                } else if (key == 2) {
                    menu_state = MENU_SELECT_NODE;
                    reset_cursor();
                } else if (key == 3) {
                    menu_state = MENU_MANUAL_CONTROL;
                    reset_cursor();
                } else if (key == 4) {
                    manual_mode = !manual_mode;
                }
                break;

            case MENU_SELECT_NODE:
                if (key == 1 || key == 2) {
                    selected_node = key - 1;
                    menu_state = MENU_SELECT_PROFILE;
                    reset_cursor();
                } else if (key == 16) {
                    menu_state = MENU_MAIN;
                    reset_cursor();
                }
                break;

            case MENU_SELECT_PROFILE: {
                uint8_t total_profiles = get_num_profiles();

                if (key == 13) {  // UP arrow
                    if (cursor_position > 0) {
                        cursor_position--;
                    } else if (scroll_offset > 0) {
                        scroll_offset--;
                    }
                } else if (key == 14) {  // DOWN arrow
                    uint8_t visible_items = (total_profiles - scroll_offset < ITEMS_PER_SCREEN)
                                            ? (total_profiles - scroll_offset) : ITEMS_PER_SCREEN;

                    if (cursor_position < visible_items - 1) {
                        cursor_position++;
                    } else if (scroll_offset + ITEMS_PER_SCREEN < total_profiles) {
                        scroll_offset++;
                    }
                } else if (key == 15) {  // SELECT/ENTER
                    uint8_t absolute_index = scroll_offset + cursor_position;
                    if (absolute_index < total_profiles) {
                        node_controller_assign_profile(selected_node, absolute_index);
                        menu_state = MENU_MAIN;
                        reset_cursor();
                    }
                } else if (key == 16) {  // BACK
                    menu_state = MENU_SELECT_NODE;
                    reset_cursor();
                }
                break;
            }

            case MENU_VIEW_STATUS:
                if (key == 16) {
                    menu_state = MENU_MAIN;
                    reset_cursor();
                }
                break;

            case MENU_MANUAL_CONTROL:
                if (key >= 1 && key <= 8 && manual_mode) {
                    last_manual_key = key;
                } else if (key == 16) {
                    menu_state = MENU_MAIN;
                    reset_cursor();
                }
                break;
        }
    } else if (key == 0) {
        last_key = 0;
    }
}

void menu_display(uint16_t adc1[3], uint16_t adc2[3]) {
    ssd1306_clear();

    char line_buf[32];
    NodeState_t* node1 = node_controller_get_state(0);
    NodeState_t* node2 = node_controller_get_state(1);

    switch (menu_state) {
        case MENU_MAIN:
            ssd1306_print(10, 0, "MAIN MENU");
            ssd1306_draw_line(0, 10, 128, 10);
            ssd1306_print(5, 15, "1.STATUS");
            ssd1306_print(5, 25, "2.ASSIGN PROFILE");
            ssd1306_print(5, 35, "3.MANUAL CTRL");
            snprintf(line_buf, sizeof(line_buf), "4.MODE:%s", manual_mode ? "MAN" : "AUTO");
            ssd1306_print(5, 45, line_buf);
            break;

        case MENU_SELECT_NODE:
            ssd1306_print(5, 0, "SELECT NODE:");
            ssd1306_draw_line(0, 10, 128, 10);
            ssd1306_print(5, 20, "1. NODE 1");
            ssd1306_print(5, 35, "2. NODE 2");
            ssd1306_print(5, 50, "16.BACK");
            break;

        case MENU_SELECT_PROFILE: {
            uint8_t total_profiles = get_num_profiles();

            snprintf(line_buf, sizeof(line_buf), "NODE %d PROFILE:", selected_node + 1);
            ssd1306_print(5, 0, line_buf);
            ssd1306_draw_line(0, 10, 128, 10);

            // Calculate how many items to show
            uint8_t visible_items = (total_profiles - scroll_offset < ITEMS_PER_SCREEN)
                                    ? (total_profiles - scroll_offset) : ITEMS_PER_SCREEN;

            // Display visible items with cursor
            for (uint8_t i = 0; i < visible_items; i++) {
                uint8_t profile_index = scroll_offset + i;
                PlantProfile_t* profile = get_profile(profile_index);
                if (profile != NULL) {
                    uint8_t y_pos = 15 + i * 10;

                    // Draw cursor
                    if (i == cursor_position) {
                        ssd1306_print(0, y_pos, "->");
                    }

                    // Draw profile name
                    snprintf(line_buf, sizeof(line_buf), "%s", profile->name);
                    ssd1306_print(20, y_pos, line_buf);
                }
            }

            // Show scroll indicators
            if (scroll_offset > 0) {
                ssd1306_print(120, 15, "^");  // More items above
            }
            if (scroll_offset + ITEMS_PER_SCREEN < total_profiles) {
                ssd1306_print(120, 45, "v");  // More items below
            }

            // Show instructions at bottom
            ssd1306_print(0, 55, "13^ 14v 15OK 16X");
            break;
        }

        case MENU_VIEW_STATUS:
            ssd1306_print(5, 0, "SYSTEM STATUS");
            ssd1306_draw_line(0, 10, 128, 10);

            if (node1 != NULL) {
                snprintf(line_buf, sizeof(line_buf), "N1:%s",
                    (node1->assigned_profile != 255) ?
                    get_profile_name(node1->assigned_profile) : "NONE");
                ssd1306_print(0, 15, line_buf);
                snprintf(line_buf, sizeof(line_buf), "H:%d T:%d L:%d", adc1[0], adc1[1], adc1[2]);
                ssd1306_print(0, 25, line_buf);
            }

            if (node2 != NULL) {
                snprintf(line_buf, sizeof(line_buf), "N2:%s",
                    (node2->assigned_profile != 255) ?
                    get_profile_name(node2->assigned_profile) : "NONE");
                ssd1306_print(0, 38, line_buf);
                snprintf(line_buf, sizeof(line_buf), "H:%d T:%d L:%d", adc2[0], adc2[1], adc2[2]);
                ssd1306_print(0, 48, line_buf);
            }
            ssd1306_print(0, 58, "16.BACK");
            break;

        case MENU_MANUAL_CONTROL:
            ssd1306_print(5, 0, "MANUAL CONTROL");
            ssd1306_draw_line(0, 10, 128, 10);
            ssd1306_print(0, 15, "N1:1-PMP 2-HUM");
            ssd1306_print(0, 25, "   3-FAN 4-LGT");
            ssd1306_print(0, 38, "N2:5-PMP 6-HUM");
            ssd1306_print(0, 48, "   7-FAN 8-LGT");
            ssd1306_print(0, 58, "16.BACK");
            break;
    }

    ssd1306_update();
}

uint8_t menu_is_manual_mode(void) {
    return manual_mode;
}

uint8_t menu_get_last_manual_key(void) {
    return last_manual_key;
}

void menu_clear_manual_key(void) {
    last_manual_key = 0;
}
