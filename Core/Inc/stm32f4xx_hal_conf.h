#ifndef __STM32F4XX_HAL_CONF_H
#define __STM32F4XX_HAL_CONF_H

/*
 * HAL source files include stm32f4xx_hal.h directly (not via main.h),
 * so the CMSIS types (uint32_t, __IO, __weak, register bases, etc.)
 * and HAL base types (HAL_StatusTypeDef) must be available before
 * hal.h uses them.  Pull them in here since hal.h includes this file
 * first thing (line ~29).
 */
#include "stm32f4xx.h"           /* CMSIS device: registers, __IO, uint32_t */
#include "stm32f4xx_hal_def.h"   /* HAL_StatusTypeDef, HAL_LockTypeDef, etc. */

#define HAL_MODULE_ENABLED

/* Only enable the HAL modules actually used by this project */
/* Unused: ADC, CAN, CRC, DAC, DCMI, I2C, RTC, SD, SPI */
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#define HSE_VALUE                       8000000UL
#define HSI_VALUE                       16000000UL
#define HSE_STARTUP_TIMEOUT             100U
#define LSE_VALUE                       32768UL
#define LSE_STARTUP_TIMEOUT             5000U
#define EXTERNAL_CLOCK_VALUE            12288000U

#define USE_HAL_ETH_REGISTER_CALLBACKS          0U
#define USE_HAL_TIM_REGISTER_CALLBACKS          0U
#define USE_HAL_UART_REGISTER_CALLBACKS         0U

#define  VDD_VALUE                    3300U
#define  TICK_INT_PRIORITY            0x0FU
#define  USE_RTOS                     0U
#define  PREFETCH_ENABLE              1U
#define  INSTRUCTION_CACHE_ENABLE     1U
#define  DATA_CACHE_ENABLE            1U

#define  assert_param(expr) ((void)0U)

#define USE_SPI_CRC                     0U

/* PHY access timeouts (milliseconds) — required by HAL ETH driver */
#define PHY_READ_TO                     ((uint32_t)2000U)
#define PHY_WRITE_TO                    ((uint32_t)2000U)

/*
 * Conditionally pull in module headers that the thin stm32f4xx_hal.h
 * no longer auto-includes.  HAL source files (hal.c, hal_rcc.c, etc.)
 * use macros/functions from sibling modules and need these available.
 */
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f4xx_hal_cortex.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_ETH_MODULE_ENABLED
#include "stm32f4xx_hal_eth.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f4xx_hal_gpio.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_pwr_ex.h"
#endif
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_rcc_ex.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_tim_ex.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f4xx_hal_uart.h"
#endif

#endif /* __STM32F4XX_HAL_CONF_H */
