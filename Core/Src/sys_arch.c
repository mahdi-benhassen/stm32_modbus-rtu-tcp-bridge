#include "lwip/sys.h"
#include "lwip/arch.h"
#include "lwip/opt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_gcc.h"

/*
 * ============================================================
 *  lwIP OS Abstraction Layer (sys_arch)
 *
 *  Bridges lwIP threading/synchronization primitives to FreeRTOS.
 *  Required by lwIP when NO_SYS=0 and using the socket/netconn API.
 *
 *  Implements: sys_mbox, sys_sem, sys_thread, sys_arch_protect,
 *              sys_now, and timeout helpers.
 * ============================================================ */

static inline int is_in_isr_context(void)
{
    return (__get_IPSR() != 0);
}

/* ---- System Time (milliseconds since boot) ---- */

u32_t sys_now(void)
{
    return (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ---- Mailbox (lwIP message queue) ---- */

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    *mbox = xQueueCreate((UBaseType_t)size, sizeof(void *));
    if (*mbox == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    if (mbox != NULL && *mbox != NULL) {
        /* Drain pending messages to avoid invalid pointers */
        void *msg;
        while (xQueueReceive(*mbox, &msg, 0) == pdTRUE) {}
        vQueueDelete(*mbox);
        *mbox = NULL;
    }
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    if (is_in_isr_context()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        while (xQueueSendToBackFromISR(*mbox, &msg,
               &xHigherPriorityTaskWoken) != pdTRUE) {}
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE) {}
    }
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (is_in_isr_context()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendToBackFromISR(*mbox, &msg,
             &xHigherPriorityTaskWoken) == pdTRUE) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            return ERR_OK;
        }
        return ERR_MEM;
    } else {
        if (xQueueSendToBack(*mbox, &msg, 0) == pdTRUE) {
            return ERR_OK;
        }
        return ERR_MEM;
    }
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout_ms)
{
    TickType_t ticks;
    void *dummy;

    if (msg == NULL) {
        msg = &dummy;
    }

    if (timeout_ms == 0) {
        /* Non-blocking */
        if (xQueueReceive(*mbox, msg, 0) == pdTRUE) {
            return 0;
        }
        return SYS_ARCH_TIMEOUT;
    }

    ticks = pdMS_TO_TICKS(timeout_ms);
    if (ticks == 0) {
        ticks = 1;
    }

    if (xQueueReceive(*mbox, msg, ticks) == pdTRUE) {
        return 0;
    }

    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *dummy;

    if (msg == NULL) {
        msg = &dummy;
    }

    if (xQueueReceive(*mbox, msg, 0) == pdTRUE) {
        return 0;
    }
    return SYS_ARCH_TIMEOUT;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return (mbox != NULL && *mbox != NULL);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    *mbox = NULL;
}

/* ---- Semaphore ---- */

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    *sem = xSemaphoreCreateCounting(0xFFFF, (UBaseType_t)count);
    if (*sem == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    if (sem != NULL && *sem != NULL) {
        vSemaphoreDelete(*sem);
        *sem = NULL;
    }
}

void sys_sem_signal(sys_sem_t *sem)
{
    if (is_in_isr_context()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(*sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        xSemaphoreGive(*sem);
    }
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms)
{
    TickType_t ticks;

    if (timeout_ms == 0) {
        if (xSemaphoreTake(*sem, 0) == pdTRUE) {
            return 0;
        }
        return SYS_ARCH_TIMEOUT;
    }

    ticks = pdMS_TO_TICKS(timeout_ms);
    if (ticks == 0) {
        ticks = 1;
    }

    if (xSemaphoreTake(*sem, ticks) == pdTRUE) {
        return 0;
    }

    return SYS_ARCH_TIMEOUT;
}

int sys_sem_valid(sys_sem_t *sem)
{
    return (sem != NULL && *sem != NULL);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    *sem = NULL;
}

/* ---- Thread ---- */

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                             void *arg, int stacksize, int prio)
{
    TaskHandle_t task_handle;
    BaseType_t ret;

    ret = xTaskCreate((TaskFunction_t)thread,
                      name,
                      (configSTACK_DEPTH_TYPE)stacksize,
                      arg,
                      (UBaseType_t)prio,
                      &task_handle);

    if (ret != pdPASS) {
        return NULL;
    }

    return (sys_thread_t)task_handle;
}

/* ---- Critical Section Protection ---- */

sys_prot_t sys_arch_protect(void)
{
    return (sys_prot_t)taskENTER_CRITICAL_FROM_ISR();
}

void sys_arch_unprotect(sys_prot_t pval)
{
    taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)pval);
}

/* ---- Init ---- */

void sys_init(void)
{
    /* lwIP system init - FreeRTOS already initialized by vTaskStartScheduler */
}
