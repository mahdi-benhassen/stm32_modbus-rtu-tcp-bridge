#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/*
 * lwIP sys_arch definitions for FreeRTOS.
 * Maps lwIP mbox/semaphore/thread types to FreeRTOS primitives.
 */

typedef QueueHandle_t    sys_mbox_t;
typedef SemaphoreHandle_t sys_sem_t;
typedef SemaphoreHandle_t sys_mutex_t;
typedef TaskHandle_t     sys_thread_t;
typedef UBaseType_t      sys_prot_t;

#define SYS_MBOX_NULL    ((sys_mbox_t)0)
#define SYS_SEM_NULL     ((sys_sem_t)0)
#define SYS_MUTEX_NULL   ((sys_mutex_t)0)

#ifndef SYS_ARCH_TIMEOUT
#define SYS_ARCH_TIMEOUT 0xFFFFFFFFUL
#endif

#endif /* LWIP_ARCH_SYS_ARCH_H */
