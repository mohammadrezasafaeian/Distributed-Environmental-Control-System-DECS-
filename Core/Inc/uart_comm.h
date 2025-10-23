
#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdint.h>
#include "main.h"

void uart_comm_init(UART_HandleTypeDef* huart);
void uart_comm_send_status(uint16_t adc1[3], uint16_t adc2[3]);

//Call this after every send_command() to track actuator states
void uart_comm_update_actuator_state(uint8_t node, uint8_t command);

#endif
