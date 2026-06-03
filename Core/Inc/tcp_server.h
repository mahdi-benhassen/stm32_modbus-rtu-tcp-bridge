#ifndef __TCP_SERVER_H
#define __TCP_SERVER_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app_config.h"

void tcp_server_task(void *pvParameters);

#endif /* __TCP_SERVER_H */
