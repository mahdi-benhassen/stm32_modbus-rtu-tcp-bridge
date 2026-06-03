#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ============================================================
 *  Application Configuration
 *  Transparent Modbus RTU <-> Modbus TCP Bridge
 *  STM32F407VGT6 - FreeRTOS - lwIP
 * ============================================================ */

/* ---- RS485 Communication Defaults ---- */
#define RS485_DEFAULT_BAUDRATE      19200
#define RS485_DATA_BITS             8
#define RS485_PARITY                UART_PARITY_EVEN
#define RS485_STOP_BITS             UART_STOPBITS_1

/* ---- Modbus Timing Constants ---- */
/*
 * At 19200 bps, 11 bits/char (1 start + 8 data + 1 parity + 1 stop):
 *   1 character   = 11 / 19200 = 573 us
 *   1.5 characters = 860 us  (inter-character timeout)
 *   3.5 characters = 2005 us (frame-end silence)
 */
#define MODBUS_3_5_CHAR_TIMEOUT_US  2005U
#define MODBUS_1_5_CHAR_TIMEOUT_US  860U

#define MODBUS_RESPONSE_TIMEOUT_MS  1000U  /* Slave response timeout */

/* ---- Frame Size Limits ---- */
#define MODBUS_RTU_MAX_ADU_SIZE     256U   /* 1 addr + 253 PDU + 2 CRC */
#define MODBUS_TCP_MAX_ADU_SIZE     260U   /* 7 MBAP + 253 PDU        */
#define MODBUS_MBAP_HEADER_SIZE     7U

/* ---- Modbus Exception Codes ---- */
#define MODBUS_EXC_GATEWAY_TARGET_FAILED    0x0B
#define MODBUS_EXC_GATEWAY_PATH_UNAVAILABLE 0x0A

/* ---- FreeRTOS Task Configuration ---- */
#define TCP_SERVER_TASK_NAME        "TcpServer"
#define TCP_SERVER_TASK_STACK       1024U
#define TCP_SERVER_TASK_PRIO        (tskIDLE_PRIORITY + 3)

#define BRIDGE_ENGINE_TASK_NAME     "BridgeEng"
#define BRIDGE_ENGINE_TASK_STACK    1024U
#define BRIDGE_ENGINE_TASK_PRIO     (tskIDLE_PRIORITY + 4)

/* ---- IPC Queue Sizes ---- */
#define BRIDGE_REQUEST_QUEUE_LEN    8U
#define BRIDGE_RESPONSE_QUEUE_LEN   8U

/* ---- Systick / OS Tick ---- */
#define OS_TICK_RATE_HZ             1000U

/* ---- lwIP Memory Pool Sizing ---- */
#define LWIP_MEM_ALIGNMENT          4

#endif /* __APP_CONFIG_H */
