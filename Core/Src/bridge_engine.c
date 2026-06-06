#include "bridge_engine.h"
#include "modbus_crc.h"
#include "rs485_driver.h"
#include "main.h"
#include "lwip/sockets.h"
#include <string.h>

/*
 * ============================================================
 *  Bridge Engine Task
 *
 *  Core translation logic for transparent Modbus TCP <-> RTU.
 *
 *  Flow for each request:
 *   1. Dequeue request from TCP server.
 *   2. Acquire RS485 bus mutex (serialize physical wire access).
 *   3. Extract + cache MBAP header (Transaction ID, Protocol ID, Unit ID).
 *   4. Isolate PDU from TCP frame.
 *   5. Build RTU frame: [Unit ID] + [PDU] + [CRC16].
 *   6. Transmit on RS485 (assert DE, DMA TX, wait TC, deassert DE).
 *   7. Start TIM3 (response timeout) + TIM2 (3.5-char silence).
 *   8. Wait for RX frame semaphore (woken by TIM2 or TIM3 ISR).
 *   9. On frame received: validate CRC16.
 *      - If CRC valid  -> rebuild TCP response, send to TCP server queue.
 *      - If CRC invalid -> discard, wait for timeout, exception 0x0B.
 *  10. On timeout: return Modbus Exception 0x0B to TCP client.
 *  11. Release RS485 bus mutex.
 *  12. Loop.
 * ============================================================ */

extern QueueHandle_t bridge_request_queue;
extern QueueHandle_t bridge_response_queue;
extern SemaphoreHandle_t rs485_bus_mutex;
extern SemaphoreHandle_t rx_frame_semaphore;
extern SemaphoreHandle_t tx_done_semaphore;

/* ---- Diagnostic Counters (type in bridge_engine.h) ---- */
bridge_diag_t g_bridge_diag;

/*
 * Build a Modbus Exception response (TCP format).
 * Exception frame: MBAP + Function Code (with 0x80 bit set) + Exception Code.
 */
static uint16_t build_exception_response(const uint8_t *mbap_header,
                                          uint8_t func_code,
                                          uint8_t exception_code,
                                          uint8_t *tcp_out)
{
    /* Copy MBAP header (7 bytes) */
    memcpy(tcp_out, mbap_header, MODBUS_MBAP_HEADER_SIZE);

    /* Length field = 3 bytes (Unit ID + Func + Exc) */
    uint16_t length = lwip_htons(3);
    memcpy(&tcp_out[4], &length, 2);

    /* Unit ID */
    tcp_out[6] = mbap_header[6];

    /* Function code with error flag */
    tcp_out[7] = func_code | 0x80;

    /* Exception code */
    tcp_out[8] = exception_code;

    return 9; /* MBAP(7) + Func(1) + Exc(1) = 9 bytes */
}

/*
 * Build a normal Modbus TCP response from a valid RTU response.
 * TCP Frame = MBAP Header (updated length) + PDU from RTU.
 */
static uint16_t build_tcp_response(const uint8_t *mbap_header,
                                    const uint8_t *rtu_pdu,
                                    uint16_t pdu_len,
                                    uint8_t *tcp_out)
{
    /* MBAP header (7 bytes) */
    memcpy(tcp_out, mbap_header, MODBUS_MBAP_HEADER_SIZE);

    /* Update length field = Unit ID (1) + PDU length (n) */
    uint16_t length = lwip_htons((uint16_t)(1 + pdu_len));
    memcpy(&tcp_out[4], &length, 2);

    /* Unit ID (already correct from original MBAP) */

    /* Append PDU */
    if (pdu_len > 0) {
        memcpy(&tcp_out[7], rtu_pdu, pdu_len);
    }

    return (uint16_t)(MODBUS_MBAP_HEADER_SIZE + pdu_len);
}

void bridge_engine_task(void *pvParameters)
{
    (void)pvParameters;

    bridge_request_t  req;
    bridge_response_t resp;

    uint8_t  rtu_tx_buffer[MODBUS_RTU_MAX_ADU_SIZE];
    uint8_t  mbap_cache[MODBUS_MBAP_HEADER_SIZE];
    uint16_t rtu_tx_len;
    uint16_t pdu_len;

    while (1) {
        /* Block until a TCP request arrives */
        if (xQueueReceive(bridge_request_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        g_bridge_diag.requests++;

        /* Acquire RS485 bus mutex - serializes bus access */
        if (xSemaphoreTake(rs485_bus_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
            continue;
        }

        HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_SET);

        /* ============================================
         * STEP 1: Extract and validate MBAP Header
         * MBAP Layout:
         *   [0-1]: Transaction ID
         *   [2-3]: Protocol ID (must be 0x0000)
         *   [4-5]: Length (Unit ID + PDU = n + 1 bytes)
         *   [6]:   Unit ID (1-247 valid, 0 = broadcast)
         * ============================================ */
        if (req.frame_len < MODBUS_MBAP_HEADER_SIZE + 1) {
            g_bridge_diag.frame_too_short++;
            resp.status         = BRIDGE_ERR_FRAME_TOO_SHORT;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = 0;
            resp.exception_code = 0;
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        memcpy(mbap_cache, req.tcp_frame, MODBUS_MBAP_HEADER_SIZE);

        /* Validate Protocol ID = 0 */
        uint16_t protocol_id = ((uint16_t)req.tcp_frame[2] << 8) |
                                (uint16_t)req.tcp_frame[3];
        if (protocol_id != 0x0000) {
            g_bridge_diag.protocol_errors++;
            resp.status         = BRIDGE_OK;
            resp.client_sock    = req.client_sock;
            resp.exception_code = 0;
            resp.frame_len      = build_exception_response(mbap_cache,
                                   req.tcp_frame[7],
                                   MODBUS_EXC_GATEWAY_PATH_UNAVAILABLE,
                                   resp.tcp_frame);
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        uint8_t unit_id = mbap_cache[6];

        /* Validate Unit ID range */
        if (unit_id == 0 || unit_id > 247) {
            resp.status         = BRIDGE_ERR_INVALID_UNIT_ID;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = 0;
            resp.exception_code = 0;
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        /* PDU starts after MBAP header (byte 7) */
        pdu_len = req.frame_len - MODBUS_MBAP_HEADER_SIZE;

        if (pdu_len == 0 || pdu_len > 253) {
            resp.status         = BRIDGE_ERR_BUFFER_OVERFLOW;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = 0;
            resp.exception_code = 0;
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        /* ============================================
         * STEP 2: Build Modbus RTU Frame
         * RTU = [Unit ID(1)] + [PDU(n)] + [CRC16(2)]
         * ============================================ */
        rtu_tx_buffer[0] = unit_id;
        memcpy(&rtu_tx_buffer[1], &req.tcp_frame[MODBUS_MBAP_HEADER_SIZE],
               pdu_len);

        uint16_t crc = modbus_crc16(rtu_tx_buffer, (uint16_t)(1 + pdu_len));
        rtu_tx_buffer[1 + pdu_len]     = (uint8_t)(crc & 0xFF);     /* CRC Low  */
        rtu_tx_buffer[1 + pdu_len + 1] = (uint8_t)(crc >> 8);       /* CRC High */
        rtu_tx_len = (uint16_t)(1 + pdu_len + 2);

        /* ============================================
         * STEP 3: Transmit on RS485
         * Drain any stale semaphore from previous transaction first.
         * ============================================ */
        while (xSemaphoreTake(rx_frame_semaphore, 0) == pdTRUE) {} /* drain stale */
        rs485_signal_frame_received();
        rs485_signal_response_timeout();
        rs485_flush_rx_buffer();
        rs485_transmit_dma(rtu_tx_buffer, rtu_tx_len);

        /* Wait for TX completion (with timeout) */
        uint32_t tx_start = HAL_GetTick();
        while (!rs485_is_tx_complete()) {
            if ((HAL_GetTick() - tx_start) >= 100) {
                /* TX timeout - treat as bus failure */
                resp.status         = BRIDGE_ERR_TIMEOUT;
                resp.client_sock    = req.client_sock;
                resp.frame_len      = build_exception_response(mbap_cache,
                                       req.tcp_frame[7],
                                       MODBUS_EXC_GATEWAY_TARGET_FAILED,
                                       resp.tcp_frame);
                xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
                rs485_set_receive_mode();
                HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
                xSemaphoreGive(rs485_bus_mutex);
                goto next_request;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        /* ============================================
         * STEP 4: Wait for RS485 slave response
         * ============================================ */
        rs485_start_rx_timeout(MODBUS_RESPONSE_TIMEOUT_MS);

        /*
         * Wait for either:
         *  - rx_frame_semaphore (TIM2 silence = frame received)
         *  - rx_frame_semaphore (TIM3 timeout = no response)
         */
        BaseType_t sem_result = xSemaphoreTake(rx_frame_semaphore,
                                                pdMS_TO_TICKS(
                                                    MODBUS_RESPONSE_TIMEOUT_MS
                                                    + 100));

        rs485_stop_rx_timeout();

        /* ============================================
         * STEP 5: Process received frame or timeout
         * ============================================ */
        if (sem_result != pdTRUE || rs485_is_response_timeout()) {
            /* No response from slave -> Exception 0x0B */
            rs485_signal_response_timeout();
            rs485_flush_rx_buffer();

            g_bridge_diag.timeouts++;

            resp.status         = BRIDGE_ERR_TIMEOUT;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = build_exception_response(mbap_cache,
                                   req.tcp_frame[7],
                                   MODBUS_EXC_GATEWAY_TARGET_FAILED,
                                   resp.tcp_frame);
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        if (!rs485_is_frame_received()) {
            /* No frame data - unexpected */
            rs485_flush_rx_buffer();
            HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        rs485_signal_frame_received();

        uint16_t rx_len = rs485_get_rx_count();
        uint8_t *rx_data = rs485_get_rx_buffer();

        /* ============================================
         * STEP 6: Validate CRC16 on received RTU frame
         * RTU RX = [Unit ID(1)] + [PDU(n)] + [CRC16(2)]
         * ============================================ */
        if (rx_len < 4) {
            /* Frame too short even for address + CRC */
            rs485_flush_rx_buffer();
            resp.status         = BRIDGE_ERR_FRAME_TOO_SHORT;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = build_exception_response(mbap_cache,
                                   req.tcp_frame[7],
                                   MODBUS_EXC_GATEWAY_TARGET_FAILED,
                                   resp.tcp_frame);
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        uint16_t rx_crc_calc = modbus_crc16(rx_data, (uint16_t)(rx_len - 2));
        uint16_t rx_crc_recv = (uint16_t)rx_data[rx_len - 1] << 8 |
                                (uint16_t)rx_data[rx_len - 2];

        if (rx_crc_calc != rx_crc_recv) {
            g_bridge_diag.crc_errors++;
            /* CRC mismatch - corrupted frame, discard.
             * The bus is expected to remain silent after a corrupt frame.
             * Return Exception 0x0B to the TCP client. */
            rs485_flush_rx_buffer();
            resp.status         = BRIDGE_ERR_CRC;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = build_exception_response(mbap_cache,
                                   req.tcp_frame[7],
                                   MODBUS_EXC_GATEWAY_TARGET_FAILED,
                                   resp.tcp_frame);
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        /* ============================================
         * STEP 7: Build TCP response from valid RTU
         *
         * RTU RX: [Unit ID(1)] + [PDU(n)] + [CRC(2)]
         * Strip CRC and Unit ID; PDU starts at byte 1.
         * TCP TX: [MBAP Header(7)] + [PDU(n)]
         *
         * Verify the received Unit ID matches the request.
         * A mismatch means a different slave responded or
         * the frame is corrupt beyond CRC detection.
         * ============================================ */
        if (rx_data[0] != unit_id) {
            g_bridge_diag.unit_id_mismatch++;
            rs485_flush_rx_buffer();
            resp.status         = BRIDGE_ERR_TIMEOUT;
            resp.client_sock    = req.client_sock;
            resp.frame_len      = build_exception_response(mbap_cache,
                                   req.tcp_frame[7],
                                   MODBUS_EXC_GATEWAY_TARGET_FAILED,
                                   resp.tcp_frame);
            xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));
            HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
            xSemaphoreGive(rs485_bus_mutex);
            continue;
        }

        uint16_t rtu_pdu_len = (uint16_t)(rx_len - 3); /* minus addr + CRC */
        uint8_t  *rtu_pdu    = &rx_data[1];

        resp.status         = BRIDGE_OK;
        resp.client_sock    = req.client_sock;
        resp.exception_code = 0;
        resp.frame_len      = build_tcp_response(mbap_cache, rtu_pdu,
                                                  rtu_pdu_len, resp.tcp_frame);

        g_bridge_diag.responses++;

        xQueueSend(bridge_response_queue, &resp, pdMS_TO_TICKS(100));

        rs485_flush_rx_buffer();

        HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
        xSemaphoreGive(rs485_bus_mutex);

next_request:
        /* Small guard time between consecutive RTU transactions */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
