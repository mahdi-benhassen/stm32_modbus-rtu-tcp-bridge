#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Include the raw CMSIS device header (stm32f407xx.h) instead of
 * stm32f4xx.h.  The latter pulls in stm32f4xx_hal.h internally at
 * line ~287, creating a circular dependency with stm32f4xx_hal_def.h
 * (hal_def includes stm32f4xx.h, stm32f4xx.h includes hal, hal
 * needs types from hal_def before hal_def finished defining them).
 */
#include "stm32f407xx.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal.h"

/*
 * The standalone stm32f4xx_hal_driver repo ships a thin stm32f4xx_hal.h
 * that does NOT auto-include module headers.  Pull in every module we use.
 */
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_rcc_ex.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_eth.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_pwr_ex.h"
#include "stm32f4xx_hal_tim_ex.h"

#include "app_config.h"

void Error_Handler(void);

/* lwIP netif init — defined in ethernetif.c */
struct netif;
#include "lwip/netif.h"
err_t ethernetif_init(struct netif *netif);

/* ---- Ethernet PHY RMII Pin Mapping (STM32F407VGT6) ---- */
#define ETH_RMII_REF_CLK_PIN        GPIO_PIN_1
#define ETH_RMII_REF_CLK_PORT       GPIOA
#define ETH_RMII_MDIO_PIN           GPIO_PIN_2
#define ETH_RMII_MDIO_PORT          GPIOA
#define ETH_RMII_CRS_DV_PIN         GPIO_PIN_7
#define ETH_RMII_CRS_DV_PORT        GPIOA
#define ETH_RMII_RXD0_PIN           GPIO_PIN_4
#define ETH_RMII_RXD0_PORT          GPIOC
#define ETH_RMII_RXD1_PIN           GPIO_PIN_5
#define ETH_RMII_RXD1_PORT          GPIOC
#define ETH_RMII_TX_EN_PIN          GPIO_PIN_11
#define ETH_RMII_TX_EN_PORT         GPIOB
#define ETH_RMII_TXD0_PIN           GPIO_PIN_12
#define ETH_RMII_TXD0_PORT          GPIOB
#define ETH_RMII_TXD1_PIN           GPIO_PIN_13
#define ETH_RMII_TXD1_PORT          GPIOB
#define ETH_MDC_PIN                 GPIO_PIN_1
#define ETH_MDC_PORT                GPIOC

#define ETH_PHY_ADDR                0x00U

/* ---- RS485 Interface ---- */
#define RS485_USART_INSTANCE        USART1
#define RS485_DE_PORT               GPIOB
#define RS485_DE_PIN                GPIO_PIN_0
#define RS485_USART_TX_PIN          GPIO_PIN_9
#define RS485_USART_TX_PORT         GPIOA
#define RS485_USART_RX_PIN          GPIO_PIN_10
#define RS485_USART_RX_PORT         GPIOA
#define RS485_USART_AF              GPIO_AF7_USART1

/* ---- Timer Instances ---- */
#define SILENCE_TIMER_INSTANCE      TIM2    /* 3.5-character silent interval */
#define TIMEOUT_TIMER_INSTANCE      TIM3    /* Slave response timeout     */

/* ---- DMA Streams ---- */
#define RS485_TX_DMA_STREAM         DMA2_Stream7
#define RS485_TX_DMA_CHANNEL        DMA_CHANNEL_4
#define RS485_RX_DMA_STREAM         DMA2_Stream2
#define RS485_RX_DMA_CHANNEL        DMA_CHANNEL_4

/* ---- Modbus TCP ---- */
#define MODBUS_TCP_PORT             502
#define MAX_TCP_CLIENTS             4

/* ---- LED / Debug ---- */
#define DEBUG_LED_PORT              GPIOD
#define DEBUG_LED_PIN               GPIO_PIN_12
#define DEBUG_LED2_PORT             GPIOD
#define DEBUG_LED2_PIN              GPIO_PIN_13

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
