/*
 * node_controller.c
 *
 * Manages communication and control logic for irrigation zone controllers.
 * Each zone (ATmega32) has 3 sensors and 4 actuators.
 *
 * Control Strategy:
 * - Scheduled irrigation: Timer-based pump activation
 * - Environmental control: Humidity/temp/light thresholds with hysteresis
 * - I2C communication with automatic recovery on bus errors
 */

#include "node_controller.h"
#include "plant_profiles.h"
#include "uart_comm.h"  // ADD THIS INCLUDE
#include <string.h>

// Command definitions
#define CMD_PUMP_OFF     0x10
#define CMD_PUMP_ON      0x11
#define CMD_HUMID_OFF    0x12
#define CMD_HUMID_ON     0x13
#define CMD_FAN_OFF      0x14
#define CMD_FAN_ON       0x15
#define CMD_LIGHT1_OFF   0x16
#define CMD_LIGHT1_ON    0x17

// Node addresses
#define NODE1_ADDR       (0x08 << 1)
#define NODE2_ADDR       (0x07 << 1)

// Private state
static I2C_HandleTypeDef* i2c_handle = NULL;
static NodeState_t node_states[2] = {
    {255, 0, 0, 0},
    {255, 0, 0, 0}
};
static uint32_t last_control_update = 0;

// Private I2C functions
static void i2c_recovery(void) {
    if (i2c_handle == NULL) return;

    __HAL_I2C_DISABLE(i2c_handle);
    HAL_Delay(10);
    __HAL_I2C_ENABLE(i2c_handle);
    HAL_Delay(10);

    __HAL_I2C_CLEAR_FLAG(i2c_handle, I2C_FLAG_AF);
    __HAL_I2C_CLEAR_FLAG(i2c_handle, I2C_FLAG_BERR);
    __HAL_I2C_CLEAR_FLAG(i2c_handle, I2C_FLAG_ARLO);
    __HAL_I2C_CLEAR_FLAG(i2c_handle, I2C_FLAG_OVR);
}

// MODIFIED: Now tracks which node received the command
static void send_command(uint8_t slave_addr, uint8_t command) {
    if (i2c_handle == NULL) return;

    HAL_StatusTypeDef ref = HAL_ERROR;
    uint8_t retry = 3;

    while (retry-- > 0 && ref != HAL_OK) {
        ref = HAL_I2C_Master_Transmit(i2c_handle, slave_addr, &command, 1, 1000);
        if (ref != HAL_OK) {
            i2c_recovery();
            HAL_Delay(50);
        }
    }

    // CRITICAL: Update actuator state tracking for ESP32 dashboard
    if (ref == HAL_OK) {
        uint8_t node = (slave_addr == NODE1_ADDR) ? 0 : 1;
        uart_comm_update_actuator_state(node, command);
    }
}

static void read_sensors(uint8_t slave_addr, uint16_t* result) {
    if (i2c_handle == NULL || result == NULL) return;

    uint8_t raw_data[6] = {0};
    HAL_StatusTypeDef ref = HAL_ERROR;
    uint8_t retry = 3;

    while (retry-- > 0 && ref != HAL_OK) {
        ref = HAL_I2C_Master_Receive(i2c_handle, slave_addr | 1, raw_data, 6, 1000);
        if (ref != HAL_OK) {
            i2c_recovery();
            HAL_Delay(50);
        }
    }

    if (ref == HAL_OK) {
        for (int i = 0; i < 3; i++) {
            result[i] = (raw_data[2*i] << 8) | raw_data[2*i + 1];
        }
    }
}

// Public functions
void node_controller_init(I2C_HandleTypeDef* hi2c) {
    i2c_handle = hi2c;
}

void node_controller_update(uint16_t adc_node1[3], uint16_t adc_node2[3]) {
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_control_update < 250) {
        return;
    }
    last_control_update = current_time;

    uint16_t adc_readings[2][3];
    uint8_t slave_addrs[2] = {NODE1_ADDR, NODE2_ADDR};

    memcpy(adc_readings[0], adc_node1, sizeof(uint16_t) * 3);
    memcpy(adc_readings[1], adc_node2, sizeof(uint16_t) * 3);

    for (uint8_t node = 0; node < 2; node++) {
        if (node_states[node].assigned_profile == 255) {
            continue;
        }

        PlantProfile_t *profile = get_profile(node_states[node].assigned_profile);
        if (profile == NULL) continue;

        // IRRIGATION CONTROL
        if (node_states[node].irrigation_active) {
            uint32_t irrigation_elapsed = (current_time - node_states[node].irrigation_start_time) / 1000;

            if (irrigation_elapsed >= profile->irrigation_duration_sec) {
                send_command(slave_addrs[node], CMD_PUMP_OFF);
                node_states[node].irrigation_active = 0;
            }
        } else {
            uint32_t time_since_last = (current_time - node_states[node].last_irrigation_time) / 1000;

            // Scheduled irrigation
            if (time_since_last >= profile->irrigation_interval_sec) {
                send_command(slave_addrs[node], CMD_PUMP_ON);
                node_states[node].irrigation_active = 1;
                node_states[node].irrigation_start_time = current_time;
                node_states[node].last_irrigation_time = current_time;
            }

            // HUMIDITY CONTROL (HUMIDIFIER) - WITH HYSTERESIS
            if (adc_readings[node][0] < profile->humidity_threshold) {
                send_command(slave_addrs[node], CMD_HUMID_ON);
            } else if (adc_readings[node][0] > profile->humidity_threshold + 50) {
                send_command(slave_addrs[node], CMD_HUMID_OFF);
            }

            // TEMPERATURE CONTROL (FAN) - WITH HYSTERESIS
            if (adc_readings[node][1] > profile->temp_threshold) {
                send_command(slave_addrs[node], CMD_FAN_ON);
            } else if (adc_readings[node][1] < profile->temp_threshold - 50) {
                send_command(slave_addrs[node], CMD_FAN_OFF);
            }

            // LIGHT CONTROL - WITH HYSTERESIS
            if (adc_readings[node][2] < profile->light_threshold) {
                send_command(slave_addrs[node], CMD_LIGHT1_ON);  // ✓ Already correct with new define
            } else if (adc_readings[node][2] > profile->light_threshold + 100) {
                send_command(slave_addrs[node], CMD_LIGHT1_OFF);  // ✓ Already correct with new define
            }
        }
    }
}

void node_controller_send_manual_command(uint8_t node, uint8_t command) {
    uint8_t addr = (node == 0) ? NODE1_ADDR : NODE2_ADDR;
    send_command(addr, command);
}

NodeState_t* node_controller_get_state(uint8_t node) {
    return (node < 2) ? &node_states[node] : NULL;
}

void node_controller_assign_profile(uint8_t node, uint8_t profile_index) {
    if (node < 2) {
        node_states[node].assigned_profile = profile_index;
        node_states[node].last_irrigation_time = HAL_GetTick();
    }
}

void node_controller_read_sensors(uint8_t node, uint16_t* result) {
    uint8_t addr = (node == 0) ? NODE1_ADDR : NODE2_ADDR;
    read_sensors(addr, result);
}
