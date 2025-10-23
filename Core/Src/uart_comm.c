

#include "uart_comm.h"
#include "node_controller.h"
#include "plant_profiles.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef* uart_handle = NULL;

// Track actuator states for each node
typedef struct {
    uint8_t pump_on;
    uint8_t humid_on;
    uint8_t fan_on;
    uint8_t light1_on;
} ActuatorStates_t;

static ActuatorStates_t node_actuators[2] = {{0, 0, 0, 0}, {0, 0, 0, 0}};

void uart_comm_init(UART_HandleTypeDef* huart) {
    uart_handle = huart;
}

// Call this function whenever you send a command to update state tracking
void uart_comm_update_actuator_state(uint8_t node, uint8_t command) {
    if (node >= 2) return;

    switch (command) {
        case 0x10: node_actuators[node].pump_on = 0; break;
        case 0x11: node_actuators[node].pump_on = 1; break;
        case 0x12: node_actuators[node].humid_on = 0; break;
        case 0x13: node_actuators[node].humid_on = 1; break;
        case 0x14: node_actuators[node].fan_on = 0; break;
        case 0x15: node_actuators[node].fan_on = 1; break;
        case 0x16: node_actuators[node].light1_on = 0; break;
        case 0x17: node_actuators[node].light1_on = 1; break;
    }
}

void uart_comm_send_status(uint16_t adc1[3], uint16_t adc2[3]) {
    if (uart_handle == NULL) return;

    char buffer[300];
    NodeState_t* node1 = node_controller_get_state(0);
    NodeState_t* node2 = node_controller_get_state(1);

    snprintf(buffer, sizeof(buffer),
        "{\"node1\":{"
    		"\"humidity\":%d,"
            "\"temp\":%d,"
            "\"light\":%d,"
            "\"profile\":\"%s\","
    		"\"irrigation\":%d,"
    		"\"humid\":%d,"
    		"\"fan\":%d,"
    		"\"light1\":%d"
        "},"
        "\"node2\":{"
    		"\"humidity\":%d,"
            "\"temp\":%d,"
            "\"light\":%d,"
            "\"profile\":\"%s\","
    		"\"irrigation\":%d,"
    		"\"humid\":%d,"
    		"\"fan\":%d,"
    		"\"light1\":%d"
        "}}\r\n",

        // Node 1 data
        adc1[0], adc1[1], adc1[2],
        (node1 != NULL && node1->assigned_profile != 255) ? get_profile_name(node1->assigned_profile) : "None",
        (node1 != NULL) ? node1->irrigation_active : 0,
        node_actuators[0].humid_on,
        node_actuators[0].fan_on,
        node_actuators[0].light1_on,

        // Node 2 data
        adc2[0], adc2[1], adc2[2],
        (node2 != NULL && node2->assigned_profile != 255) ? get_profile_name(node2->assigned_profile) : "None",
        (node2 != NULL) ? node2->irrigation_active : 0,
        node_actuators[1].humid_on,
        node_actuators[1].fan_on,
        node_actuators[1].light1_on
    );

    HAL_UART_Transmit(uart_handle, (uint8_t*)buffer, strlen(buffer), 1000);
}
