#include "main.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "bridge_engine.h"
#include "tcp_server.h"
#include "rs485_driver.h"

/*
 * ============================================================
 *  Main Entry Point
 *
 *  STM32F407VGT6 - 168 MHz
 *  FreeRTOS 10.x + lwIP 2.x in Socket API mode
 *  Transparent Modbus RTU <-> Modbus TCP Bridge
 * ============================================================ */

/* ---- Global IPC Objects ---- */
QueueHandle_t   bridge_request_queue  = NULL;
QueueHandle_t   bridge_response_queue = NULL;
SemaphoreHandle_t rs485_bus_mutex     = NULL;
SemaphoreHandle_t rx_frame_semaphore  = NULL;
SemaphoreHandle_t tx_done_semaphore   = NULL;

/* ---- System Clock Speed ---- */
uint32_t SystemCoreClock = 168000000;

/* ---- lwIP Network Interface ---- */
extern struct netif gnetif;

/* ---- Function Prototypes ---- */
static void SystemClock_Config(void);
static void MX_ETH_Init(void);
static void MX_GPIO_Init(void);
static void prvSetupHardware(void);

/*
 * System Clock Configuration
 * HSE: 8 MHz
 * PLL: M=8, N=336, P=2 (SYSCLK = 168 MHz)
 *      Q=7  (48 MHz for USB/ETH)
 * AHB: /1   (168 MHz)
 * APB1: /4  (42 MHz, timer clock = 84 MHz)
 * APB2: /2  (84 MHz, timer clock = 168 MHz)
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE Oscillator: 8 MHz */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* AHB, APB1, APB2 clocks */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK |
                                       RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_ETH_Init(void)
{
    /* Ethernet peripheral initialization is handled by the lwIP ethernetif
     * driver. This function is a placeholder for CubeMX compatibility.
     * The actual MAC + PHY init is done in ethernetif.c via
     * HAL_ETH_Init() with RMII configuration. */
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* Default all pins to analog to save power (CubeMX pattern) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_All, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_All, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_All, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_All, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin  = GPIO_PIN_All;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

static void prvSetupHardware(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    /* RS485 driver initializes USART1, DMA, TIM2, TIM3, and DE pin */
    rs485_init();
    /* Ethernet init is handled by lwIP ethernetif driver called from tcpip_init */
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(DEBUG_LED_PORT, DEBUG_LED_PIN);
        HAL_GPIO_TogglePin(DEBUG_LED_PORT, DEBUG_LED2_PIN);
        for (volatile uint32_t i = 0; i < 5000000; i++) {}
    }
}

/*
 * lwIP network interface - defined in ethernetif.c
 * IP configuration: static or DHCP.
 */
static void netif_config(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

    /* Static IP: 192.168.1.100 / 255.255.255.0 / 192.168.1.1 */
    IP4_ADDR(&ipaddr,  192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      192, 168, 1, 1);

    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL,
              ethernetif_init, tcpip_input);

    netif_set_default(&gnetif);
    netif_set_up(&gnetif);
}

/*
 * Main function
 */
int main(void)
{
    prvSetupHardware();

    /* ---- Create FreeRTOS IPC objects ---- */
    bridge_request_queue = xQueueCreate(BRIDGE_REQUEST_QUEUE_LEN,
                                         sizeof(bridge_request_t));
    bridge_response_queue = xQueueCreate(BRIDGE_RESPONSE_QUEUE_LEN,
                                          sizeof(bridge_response_t));
    rs485_bus_mutex    = xSemaphoreCreateMutex();
    rx_frame_semaphore = xSemaphoreCreateBinary();
    tx_done_semaphore  = xSemaphoreCreateBinary();

    if (!bridge_request_queue || !bridge_response_queue ||
        !rs485_bus_mutex || !rx_frame_semaphore || !tx_done_semaphore) {
        Error_Handler();
    }

    /* ---- Initialize lwIP stack ---- */
    tcpip_init(NULL, NULL);
    netif_config();

    /* Wait for network link up (timeout 10 seconds) */
    uint32_t link_timeout = HAL_GetTick() + 10000;
    while (!netif_is_link_up(&gnetif)) {
        if (HAL_GetTick() > link_timeout) {
            break; /* Proceed without link; DHCP/static will keep trying */
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* ---- Create Tasks ---- */
    BaseType_t task_created;

    task_created = xTaskCreate(tcp_server_task,
                               TCP_SERVER_TASK_NAME,
                               TCP_SERVER_TASK_STACK,
                               NULL,
                               TCP_SERVER_TASK_PRIO,
                               NULL);
    if (task_created != pdPASS) {
        Error_Handler();
    }

    task_created = xTaskCreate(bridge_engine_task,
                               BRIDGE_ENGINE_TASK_NAME,
                               BRIDGE_ENGINE_TASK_STACK,
                               NULL,
                               BRIDGE_ENGINE_TASK_PRIO,
                               NULL);
    if (task_created != pdPASS) {
        Error_Handler();
    }

    /* ---- Start FreeRTOS Scheduler ---- */
    vTaskStartScheduler();

    /* Should never reach here */
    Error_Handler();
    return 0;
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    __disable_irq();
    while (1) {}
}
#endif
