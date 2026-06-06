#include "tcp_server.h"
#include "bridge_engine.h"
#include "main.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <stdbool.h>
#include <string.h>

/*
 * ============================================================
 *  TCP Server Task
 *
 *  Responsibilities:
 *   1. Listen on Port 502 (Modbus TCP).
 *   2. Accept up to MAX_TCP_CLIENTS connections.
 *   3. Receive Modbus TCP frames, validate length (≤ 260 bytes).
 *   4. Forward frames to Bridge Engine via request queue.
 *   5. Receive processed responses from Bridge Engine via response queue.
 *   6. Transmit TCP responses back to originating client socket.
 *   7. Handle client disconnections, socket errors gracefully.
 * ============================================================ */

extern QueueHandle_t bridge_request_queue;
extern QueueHandle_t bridge_response_queue;

static int client_sockets[MAX_TCP_CLIENTS];
static uint32_t client_last_active[MAX_TCP_CLIENTS];  /* HAL_GetTick() timestamp */

#define CLIENT_IDLE_TIMEOUT_MS  30000U   /* 30s idle -> close socket */

static void client_sockets_init(void)
{
    for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
        client_sockets[i] = -1;
        client_last_active[i] = 0;
    }
}

static int8_t find_free_slot(void)
{
    for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (client_sockets[i] < 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

static void close_client(uint8_t slot)
{
    if (slot < MAX_TCP_CLIENTS && client_sockets[slot] >= 0) {
        lwip_close(client_sockets[slot]);
        client_sockets[slot] = -1;
    }
}

void tcp_server_task(void *pvParameters)
{
    (void)pvParameters;

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    int listen_sock, max_sd, activity, new_sock;
    fd_set readfds;
    struct timeval tv;
    bridge_request_t req;
    bridge_response_t resp;

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    client_sockets_init();

    /* Create listening socket */
    listen_sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        goto error_exit;
    }

    /* Set socket options */
    int opt = 1;
    lwip_setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = lwip_htons(MODBUS_TCP_PORT);

    if (lwip_bind(listen_sock, (struct sockaddr *)&server_addr,
                  sizeof(server_addr)) < 0) {
        lwip_close(listen_sock);
        goto error_exit;
    }

    if (lwip_listen(listen_sock, MAX_TCP_CLIENTS) < 0) {
        lwip_close(listen_sock);
        goto error_exit;
    }

    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_SET);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        max_sd = listen_sock;

        for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
            if (client_sockets[i] >= 0) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_sd) {
                    max_sd = client_sockets[i];
                }
            }
        }

        /* 100 ms select timeout to allow processing response queue */
        tv.tv_sec  = 0;
        tv.tv_usec = 100000;

        activity = lwip_select(max_sd + 1, &readfds, NULL, NULL, &tv);

        /* Check for incoming connections */
        if (activity > 0 && FD_ISSET(listen_sock, &readfds)) {
            client_addr_len = sizeof(client_addr);
            new_sock = lwip_accept(listen_sock,
                                    (struct sockaddr *)&client_addr,
                                    &client_addr_len);
            if (new_sock >= 0) {
                int8_t slot = find_free_slot();
                if (slot >= 0) {
                    client_sockets[(uint8_t)slot] = new_sock;
                    client_last_active[(uint8_t)slot] = HAL_GetTick();
                } else {
                    lwip_close(new_sock);
                }
            }
        }

        /* Check existing clients for incoming data */
        for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
            int sock = client_sockets[i];
            if (sock < 0) continue;
            if (!FD_ISSET(sock, &readfds)) continue;

            uint8_t buf[MODBUS_TCP_MAX_ADU_SIZE];
            int len = lwip_recv(sock, buf, MODBUS_TCP_MAX_ADU_SIZE, 0);

            if (len <= 0) {
                close_client(i);
                continue;
            }

            /* Validate frame size */
            if ((uint16_t)len > MODBUS_TCP_MAX_ADU_SIZE) {
                close_client(i);
                continue;
            }

            /* Modbus TCP minimum: 7 (MBAP) + 1 (func code) = 8 bytes */
            if (len < 8) {
                continue;
            }

            /* Forward to Bridge Engine */
            req.client_sock = sock;
            req.frame_len   = (uint16_t)len;
            memcpy(req.tcp_frame, buf, (size_t)len);

            client_last_active[i] = HAL_GetTick();

            if (xQueueSend(bridge_request_queue, &req, pdMS_TO_TICKS(100))
                != pdTRUE) {
                /* Queue full - drop frame; client will timeout on its end */
            }
        }

        /* -------------------------------------------------------
         * Process responses from Bridge Engine
         * ----------------------------------------------------- */
        while (xQueueReceive(bridge_response_queue, &resp, 0) == pdTRUE) {
            int target_sock = resp.client_sock;

            /* Check socket still valid */
            bool sock_valid = false;
            for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
                if (client_sockets[i] == target_sock) {
                    sock_valid = true;
                    break;
                }
            }

            if (!sock_valid) continue;

            if (resp.status == BRIDGE_OK && resp.frame_len > 0) {
                lwip_send(target_sock, resp.tcp_frame,
                        (size_t)resp.frame_len, 0);
            } else if (resp.exception_code != 0) {
                lwip_send(target_sock, resp.tcp_frame,
                        (size_t)resp.frame_len, 0);
            }
            /* BRIDGE_ERR_CONNECTION_LOST -> client already gone, nothing to do */
        }

        /* -------------------------------------------------------
         * Idle client timeout: close sockets with no activity
         * for CLIENT_IDLE_TIMEOUT_MS (30 seconds).
         * ----------------------------------------------------- */
        uint32_t now = HAL_GetTick();
        for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
            if (client_sockets[i] < 0) continue;
            if ((now - client_last_active[i]) >= CLIENT_IDLE_TIMEOUT_MS) {
                close_client(i);
            }
        }

        taskYIELD();
    }

error_exit:
    while (1) {
        HAL_GPIO_TogglePin(DEBUG_LED_PORT, DEBUG_LED2_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
