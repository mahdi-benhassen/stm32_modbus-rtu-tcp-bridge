#include "rs485_driver.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/*
 * ============================================================
 *  RS485 Half-Duplex Driver
 *
 *  Manages USART1 with:
 *   - GPIO DE (Driver Enable) for TX/RX switching
 *   - DMA for efficient TX transmission
 *   - UART RX interrupt for byte-by-byte reception
 *   - TIM2 (32-bit) for 3.5-character silent interval detection
 *   - TIM3 (32-bit) for slave response timeout
 *
 *  State machine:
 *   IDLE -> TX (DE high, DMA send) -> TX_DONE (DE low) ->
 *   WAIT_RX (TIM3 enabled, TIM2 counting) -> RX_FRAME or TIMEOUT -> IDLE
 * ============================================================ */

/* ---- HAL Peripheral Handles (extern visible for ISR files) ---- */
UART_HandleTypeDef          huart_rs485;
DMA_HandleTypeDef           hdma_rs485_tx;
DMA_HandleTypeDef           hdma_rs485_rx;
TIM_HandleTypeDef           htim_silence;
TIM_HandleTypeDef           htim_timeout;

/* ---- RX Data Management ---- */
static uint8_t  rx_buffer[MODBUS_RTU_MAX_ADU_SIZE];
static volatile uint16_t rx_index = 0;
static volatile bool rx_frame_complete = false;
static volatile bool rx_timeout_flag  = false;
static volatile bool tx_done_flag     = false;

/* ---- Synchronization Objects (created in main.c) ---- */
extern SemaphoreHandle_t rx_frame_semaphore;
extern SemaphoreHandle_t tx_done_semaphore;

/* ============================================================
 *  HAL Peripheral Initialization
 * ============================================================ */

/*
 * HAL peripheral MspInit callbacks.  Called by the HAL peripheral init
 * functions (HAL_UART_Init, HAL_TIM_Base_Init, HAL_DMA_Init) to configure
 * the low-level hardware: pins, clocks, interrupts.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == RS485_USART_INSTANCE) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();

        /* PA9  = USART1_TX (AF7) */
        GPIO_InitStruct.Pin       = RS485_USART_TX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = RS485_USART_AF;
        HAL_GPIO_Init(RS485_USART_TX_PORT, &GPIO_InitStruct);

        /* PA10 = USART1_RX (AF7) */
        GPIO_InitStruct.Pin       = RS485_USART_RX_PIN;
        HAL_GPIO_Init(RS485_USART_RX_PORT, &GPIO_InitStruct);
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == SILENCE_TIMER_INSTANCE) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    } else if (htim->Instance == TIMEOUT_TIMER_INSTANCE) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
}

void HAL_DMA_MspInit(DMA_HandleTypeDef *hdma)
{
    if (hdma->Instance == DMA2_Stream7) {
        __HAL_RCC_DMA2_CLK_ENABLE();
    } else if (hdma->Instance == DMA2_Stream2) {
        __HAL_RCC_DMA2_CLK_ENABLE();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* RS485 DE (Driver Enable) pin - Push-Pull Output */
    GPIO_InitStruct.Pin   = RS485_DE_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RS485_DE_PORT, &GPIO_InitStruct);

    /* Default: Receive mode (DE low) */
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);

    /* Debug LEDs */
    GPIO_InitStruct.Pin   = DEBUG_LED_PIN | DEBUG_LED2_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DEBUG_LED_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED2_PIN, GPIO_PIN_RESET);
}

static void MX_USART1_UART_Init(void)
{
    huart_rs485.Instance          = RS485_USART_INSTANCE;
    huart_rs485.Init.BaudRate     = RS485_DEFAULT_BAUDRATE;
    huart_rs485.Init.WordLength   = UART_WORDLENGTH_9B;
    huart_rs485.Init.StopBits     = RS485_STOP_BITS;
    huart_rs485.Init.Parity       = RS485_PARITY;
    huart_rs485.Init.Mode         = UART_MODE_TX_RX;
    huart_rs485.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart_rs485.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart_rs485) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA for USART1 TX */
    hdma_rs485_tx.Instance                 = RS485_TX_DMA_STREAM;
    hdma_rs485_tx.Init.Channel             = RS485_TX_DMA_CHANNEL;
    hdma_rs485_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_rs485_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_rs485_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_rs485_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rs485_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_rs485_tx.Init.Mode                = DMA_NORMAL;
    hdma_rs485_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
    hdma_rs485_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_rs485_tx) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&huart_rs485, hdmatx, hdma_rs485_tx);

    /* DMA for USART1 RX (not used for transfer, but handle exists for ISR) */
    memset(&hdma_rs485_rx, 0, sizeof(hdma_rs485_rx));
    hdma_rs485_rx.Instance                 = RS485_RX_DMA_STREAM;
    hdma_rs485_rx.Init.Channel             = RS485_RX_DMA_CHANNEL;
    hdma_rs485_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_rs485_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_rs485_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_rs485_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rs485_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_rs485_rx.Init.Mode                = DMA_NORMAL;
    hdma_rs485_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;
    hdma_rs485_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_rs485_rx) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&huart_rs485, hdmarx, hdma_rs485_rx);

    /* Enable DMA TX complete interrupt (DMA2 Stream7) */
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    /* Enable DMA RX complete interrupt (DMA2 Stream2) - used only for error */
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
}

static void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    /*
     * TIM2: 3.5-character silent interval timer.
     * APB1 timer clock = 84 MHz (when APB1 prescaler != 1).
     * Prescaler = 84-1 = 83  ->  1 MHz tick (1 us resolution).
     * Auto-reload = 2005  ->  2005 us ~ 2 ms timeout.
     */
    htim_silence.Instance               = SILENCE_TIMER_INSTANCE;
    htim_silence.Init.Prescaler         = 83;
    htim_silence.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_silence.Init.Period            = MODBUS_3_5_CHAR_TIMEOUT_US;
    htim_silence.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_silence.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim_silence) != HAL_OK) {
        Error_Handler();
    }

    /* TIM2 Update Interrupt = silence detected */
    HAL_NVIC_SetPriority(TIM2_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

static void MX_TIM3_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    /*
     * TIM3: Modbus response timeout timer.
     * Prescaler = 8400-1 = 8399  ->  10 kHz tick (100 us resolution).
     * Auto-reload = timeout_ms * 10.
     * Configured dynamically in rs485_start_rx_timeout().
     */
    htim_timeout.Instance               = TIMEOUT_TIMER_INSTANCE;
    htim_timeout.Init.Prescaler         = 8399;
    htim_timeout.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_timeout.Init.Period            = 10000;
    htim_timeout.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_timeout.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim_timeout) != HAL_OK) {
        Error_Handler();
    }

    /* TIM3 Update Interrupt = response timeout */
    HAL_NVIC_SetPriority(TIM3_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

/* ============================================================
 *  Public API
 * ============================================================ */

void rs485_init(void)
{
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_DMA_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();

    /* Configure and enable USART RX interrupt */
    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void rs485_set_transmit_mode(void)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_SET);

    /* Disable RX interrupt during transmission */
    __HAL_UART_DISABLE_IT(&huart_rs485, UART_IT_RXNE);
}

void rs485_set_receive_mode(void)
{
    /* Wait for USART TC flag with 10ms timeout to prevent ISR deadlock */
    uint32_t tc_timeout = 10000;
    while (!__HAL_UART_GET_FLAG(&huart_rs485, UART_FLAG_TC) && --tc_timeout) {}

    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_RESET);

    /* Clear any stale RX data */
    __HAL_UART_FLUSH_DRREGISTER(&huart_rs485);

    /* Re-enable RX interrupt */
    __HAL_UART_ENABLE_IT(&huart_rs485, UART_IT_RXNE);
}

void rs485_transmit_dma(const uint8_t *data, uint16_t len)
{
    tx_done_flag = false;
    rs485_set_transmit_mode();

    if (HAL_UART_Transmit_DMA(&huart_rs485, (uint8_t *)data, len) != HAL_OK) {
        rs485_set_receive_mode();
    }
}

bool rs485_is_tx_complete(void)
{
    return tx_done_flag;
}

void rs485_wait_tx_complete(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (!tx_done_flag) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            break;
        }
        taskYIELD();
    }
}

void rs485_flush_rx_buffer(void)
{
    rx_index = 0;
    rx_frame_complete = false;
    rx_timeout_flag = false;
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

void rs485_start_rx_timeout(uint32_t timeout_ms)
{
    /*
     * TIM3 runs at 10 kHz (100 us per tick).
     * Period = timeout_ms * 10
     */
    __HAL_TIM_SET_AUTORELOAD(&htim_timeout, (uint32_t)(timeout_ms * 10));
    __HAL_TIM_SET_COUNTER(&htim_timeout, 0);
    __HAL_TIM_CLEAR_FLAG(&htim_timeout, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim_timeout);

    /*
     * Reset TIM2 (silence timer) and start it.
     */
    __HAL_TIM_SET_COUNTER(&htim_silence, 0);
    __HAL_TIM_CLEAR_FLAG(&htim_silence, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim_silence);
}

void rs485_stop_rx_timeout(void)
{
    HAL_TIM_Base_Stop_IT(&htim_timeout);
    HAL_TIM_Base_Stop_IT(&htim_silence);
}

bool rs485_is_frame_received(void)
{
    return rx_frame_complete;
}

bool rs485_is_response_timeout(void)
{
    return rx_timeout_flag;
}

uint16_t rs485_get_rx_count(void)
{
    return rx_index;
}

uint8_t* rs485_get_rx_buffer(void)
{
    return rx_buffer;
}

void rs485_signal_frame_received(void)
{
    rx_frame_complete = false;
}

void rs485_signal_response_timeout(void)
{
    rx_timeout_flag = false;
}

/* ============================================================
 *  Interrupt Callbacks (called from stm32f4xx_it.c)
 * ============================================================ */

/*
 * Called when USART1 receives a byte.
 * Stores the byte and resets the 3.5-char silence timer.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != RS485_USART_INSTANCE) return;

    /* The byte is already in the DR - read it */
    uint8_t byte = (uint8_t)(huart->Instance->DR & 0xFF);

    if (rx_index < MODBUS_RTU_MAX_ADU_SIZE) {
        rx_buffer[rx_index++] = byte;
    }

    /* Reset the 3.5-char silence timer on each received byte */
    if (htim_silence.Instance != NULL) {
        __HAL_TIM_SET_COUNTER(&htim_silence, 0);
        __HAL_TIM_CLEAR_FLAG(&htim_silence, TIM_FLAG_UPDATE);
    }
}

/*
 * DMA TX complete callback.
 * Does NOT switch to RX mode yet - wait for USART TC.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != RS485_USART_INSTANCE) return;

    /*
     * DMA transfer is complete, but the UART shift register may still
     * be shifting out the last byte. Switch to RX mode and signal
     * completion after TC is confirmed.
     */
    rs485_set_receive_mode();
    tx_done_flag = true;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(tx_done_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * This is the key callback: UART RXNE interrupt.
 * Invoked from USART1_IRQHandler.
 */
void rs485_uart_rx_isr(UART_HandleTypeDef *huart)
{
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart->Instance->DR & 0xFF);

        if (rx_index < MODBUS_RTU_MAX_ADU_SIZE) {
            rx_buffer[rx_index++] = byte;
        }

        /* Reset silence timer on every received byte */
        if (htim_silence.Instance != NULL) {
            __HAL_TIM_SET_COUNTER(&htim_silence, 0);
            __HAL_TIM_CLEAR_FLAG(&htim_silence, TIM_FLAG_UPDATE);
        }
    }

    /* Clear overrun flag if set */
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }
}

/*
 * TIM2 Update ISR: 3.5-character silence detected -> frame complete.
 */
void rs485_tim2_period_elapsed_isr(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != SILENCE_TIMER_INSTANCE) return;

    if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);

        /* Stop both timers */
        rs485_stop_rx_timeout();

        if (rx_index > 0) {
            rx_frame_complete = true;

            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(rx_frame_semaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/*
 * TIM3 Update ISR: Response timeout.
 */
void rs485_tim3_period_elapsed_isr(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIMEOUT_TIMER_INSTANCE) return;

    if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);

        rs485_stop_rx_timeout();
        rx_timeout_flag = true;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(rx_frame_semaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
