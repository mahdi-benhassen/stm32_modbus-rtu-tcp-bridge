#include "stm32f4xx_hal.h"

/*
 * ============================================================
 *  System Initialization and Core Clock Configuration
 *
 *  STM32F407VGT6 - 168 MHz
 *  HSE: 8 MHz -> PLL (M=8, N=336, P=2, Q=7)
 *  AHB: 168 MHz, APB1: 42 MHz, APB2: 84 MHz
 * ============================================================ */

uint32_t SystemCoreClock = 168000000;

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                    1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

/*
 * SystemInit() is called from startup assembly before main().
 * It sets up the vector table offset, enables FPU, and configures
 * the initial clock source to HSI. The full PLL configuration
 * to 168 MHz is done in SystemClock_Config() called from main().
 */
void SystemInit(void)
{
    /* FPU settings - enable CP10/CP11 full access */
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    #endif

    /* Configure Flash prefetch, instruction & data cache */
    #if (PREFETCH_ENABLE != 0)
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    #endif

    #if (INSTRUCTION_CACHE_ENABLE != 0)
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    #endif

    #if (DATA_CACHE_ENABLE != 0)
    __HAL_FLASH_DATA_CACHE_ENABLE();
    #endif

    /* Set interrupt vector table offset to start of flash */
    SCB->VTOR = FLASH_BASE;

    /* Configure HSI as default system clock during init phase */
    RCC->CR |= (uint32_t)0x00000001; /* HSI ON */

    /* Wait for HSI ready */
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {}

    /* Select HSI as system clock source */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_HSI;

    /* Wait for HSI used as system clock source */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}

    /* Reset RCC configuration */
    RCC->CR &= (uint32_t)0x00000001; /* Only HSI kept */
    RCC->CFGR = 0x00000000;
    RCC->CR &= (uint32_t)0xFEF6FFFF; /* Reset HSEON, CSSON, PLLON */
    RCC->PLLCFGR = 0x24003010;
    RCC->CR &= (uint32_t)0xFFFBFFFF; /* Reset HSEBYP */
    RCC->CIR = 0x00000000;

    /* Disable all interrupts */
    __set_PRIMASK(1);

    /* Configure the Vector Table location add offset */
    SCB->VTOR = FLASH_BASE;
}

/*
 * Update SystemCoreClock variable based on current clock configuration.
 */
void SystemCoreClockUpdate(void)
{
    uint32_t tmp = 0, pllvco = 0, pllp = 2, pllsource = 0, pllm = 2;

    /* Get SYSCLK source */
    tmp = RCC->CFGR & RCC_CFGR_SWS;

    switch (tmp) {
    case 0x00: /* HSI */
        SystemCoreClock = 16000000;
        break;
    case 0x04: /* HSE */
        SystemCoreClock = HSE_VALUE;
        break;
    case 0x08: /* PLL */
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22;
        pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;

        if (pllsource != 0) {
            pllvco = (HSE_VALUE / pllm) *
                     ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
        } else {
            pllvco = (16000000 / pllm) *
                     ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
        }

        pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> 16) + 1) * 2;
        SystemCoreClock = pllvco / pllp;
        break;
    default:
        SystemCoreClock = 16000000;
        break;
    }

    /* Compute HCLK, PCLK1, PCLK2 frequencies */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
    /* HCLK = SYSCLK / tmp */
    tmp = APBPrescTable[((RCC->CFGR & RCC_CFGR_PPRE1) >> 10)];
    /* PCLK1 = HCLK / tmp */
    tmp = APBPrescTable[((RCC->CFGR & RCC_CFGR_PPRE2) >> 13)];
    /* PCLK2 = HCLK / tmp */
}

/*
 * Override HAL_IncTick for FreeRTOS compatibility.
 * When FreeRTOS uses SysTick, HAL ticks come from a separate
 * timer or this function is called by the SysTick hook.
 */
uint32_t uwTick = 0;
uint32_t uwTickFreq = HAL_TICK_FREQ_DEFAULT;

void HAL_IncTick(void)
{
    uwTick += uwTickFreq;
}

uint32_t HAL_GetTick(void)
{
    return uwTick;
}

void HAL_Delay(uint32_t Delay)
{
    uint32_t tickstart = HAL_GetTick();
    uint32_t wait = Delay;

    if (wait < HAL_MAX_DELAY) {
        wait += (uint32_t)uwTickFreq;
    }

    while ((HAL_GetTick() - tickstart) < wait) {
        /* yield to FreeRTOS scheduler */
    }
}

/*
 * SysTick handler: increments HAL tick and calls FreeRTOS SysTick hook
 * if configured. FreeRTOS port's xPortSysTickHandler typically handles
 * the OS tick independently.
 */
void HAL_SYSTICK_Callback(void)
{
    HAL_IncTick();
}

/*
 * Compatibility weak symbols for HAL error returns.
 * Overridden by actual implementations in main.c.
 */
__weak void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

__weak void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    __disable_irq();
    while (1) {}
}
