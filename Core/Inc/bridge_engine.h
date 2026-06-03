#ifndef __BRIDGE_ENGINE_H
#define __BRIDGE_ENGINE_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "app_config.h"

/*
 * Bridge engine status codes
 */
typedef enum {
    BRIDGE_OK                        = 0,
    BRIDGE_ERR_CRC                   = 1,
    BRIDGE_ERR_TIMEOUT               = 2,
    BRIDGE_ERR_BUFFER_OVERFLOW       = 3,
    BRIDGE_ERR_INVALID_UNIT_ID       = 4,
    BRIDGE_ERR_CONNECTION_LOST       = 5,
    BRIDGE_ERR_FRAME_TOO_SHORT       = 6,
} bridge_status_t;

/*
 * Request message: TCP Server -> Bridge Engine
 */
typedef struct {
    int         client_sock;
    uint8_t     tcp_frame[MODBUS_TCP_MAX_ADU_SIZE];
    uint16_t    frame_len;
} bridge_request_t;

/*
 * Response message: Bridge Engine -> TCP Server
 */
typedef struct {
    int             client_sock;
    bridge_status_t status;
    uint8_t         tcp_frame[MODBUS_TCP_MAX_ADU_SIZE];
    uint16_t        frame_len;
    uint8_t         exception_code;       /* 0 if no exception */
} bridge_response_t;

void bridge_engine_task(void *pvParameters);

#endif /* __BRIDGE_ENGINE_H */
