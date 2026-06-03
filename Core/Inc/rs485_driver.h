#ifndef __RS485_DRIVER_H
#define __RS485_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "app_config.h"

void rs485_init(void);

void rs485_set_transmit_mode(void);
void rs485_set_receive_mode(void);

void rs485_transmit_dma(const uint8_t *data, uint16_t len);

bool rs485_is_tx_complete(void);
void rs485_wait_tx_complete(uint32_t timeout_ms);

void rs485_flush_rx_buffer(void);
void rs485_start_rx_timeout(uint32_t timeout_ms);
void rs485_stop_rx_timeout(void);

bool rs485_is_frame_received(void);
bool rs485_is_response_timeout(void);

uint16_t rs485_get_rx_count(void);
uint8_t* rs485_get_rx_buffer(void);

void rs485_signal_frame_received(void);
void rs485_signal_response_timeout(void);

#endif /* __RS485_DRIVER_H */
