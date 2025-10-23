#ifndef NODE_CONTROLLER_H
#define NODE_CONTROLLER_H

#include <stdint.h>
#include "main.h"

// Node state tracking
typedef struct {
    uint8_t assigned_profile;
    uint32_t last_irrigation_time;
    uint8_t irrigation_active;
    uint32_t irrigation_start_time;
} NodeState_t;

// Public API
void node_controller_init(I2C_HandleTypeDef* hi2c);
void node_controller_update(uint16_t adc_node1[3], uint16_t adc_node2[3]);
void node_controller_send_manual_command(uint8_t node, uint8_t command);
NodeState_t* node_controller_get_state(uint8_t node);
void node_controller_assign_profile(uint8_t node, uint8_t profile_index);
void node_controller_read_sensors(uint8_t node, uint16_t* result);
#endif
