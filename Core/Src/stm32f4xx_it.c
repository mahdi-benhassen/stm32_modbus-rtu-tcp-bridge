#include "stm32f4xx_it.h"
#include "main.h"
#include "rs485_driver.h"
#include "stm32f4xx_hal.h"

/*
 * ============================================================
 *  STM32F4xx Interrupt Service Routines
 * ============================================================ */

/* ---- ETH Handle (defined in ethernetif.c) ---- */
extern ETH_HandleTypeDef heth;

/* ---- HAL Handles (defined in rs485_driver.c) ---- */
extern UART_HandleTypeDef huart_rs485;
extern DMA_HandleTypeDef  hdma_rs485_tx;
extern DMA_HandleTypeDef  hdma_rs485_rx;
extern TIM_HandleTypeDef  htim_silence;
extern TIM_HandleTypeDef  htim_timeout;

/* ---- RS485 ISR helpers ---- */
extern void rs485_uart_rx_isr(UART_HandleTypeDef *huart);
extern void rs485_tim2_period_elapsed_isr(TIM_HandleTypeDef *htim);
extern void rs485_tim3_period_elapsed_isr(TIM_HandleTypeDef *htim);

/* ---- Cortex-M4 Processor Exceptions ---- */

void NMI_Handler(void) {}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void DebugMon_Handler(void) {}

/* ---- SysTick: bridges HAL tick and FreeRTOS tick ---- */

void SysTick_Handler(void)
{
    HAL_IncTick();
    extern void xPortSysTickHandler(void);
    xPortSysTickHandler();
}

/* ---- FreeRTOS Handlers ---- */

/*
 * SVC_Handler and PendSV_Handler are remapped in FreeRTOSConfig.h
 * (vPortSVCHandler, xPortPendSVHandler). The FreeRTOS port will
 * provide the implementations.
 *
 * SysTick_Handler is defined above. It calls HAL_IncTick() for
 * the HAL timebase and xPortSysTickHandler() for the OS tick.
 */

/* ---- Ethernet ---- */

void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&heth);
}

void ETH_WKUP_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&heth);
}

/* ---- USART1 ---- */

void USART1_IRQHandler(void)
{
    rs485_uart_rx_isr(&huart_rs485);
    HAL_UART_IRQHandler(&huart_rs485);
}

/* ---- DMA2 Stream7 (RS485 TX DMA) ---- */

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_rs485_tx);
}

void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_rs485_rx);
}

/* ---- TIM2: 3.5-Char Silence Timer ---- */

void TIM2_IRQHandler(void)
{
    rs485_tim2_period_elapsed_isr(&htim_silence);
}

/* ---- TIM3: Modbus Response Timeout Timer ---- */

void TIM3_IRQHandler(void)
{
    rs485_tim3_period_elapsed_isr(&htim_timeout);
}
