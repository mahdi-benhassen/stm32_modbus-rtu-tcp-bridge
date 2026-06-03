#include "main.h"

/*
 * ============================================================
 *  System Initialization and Core Clock Update
 *
 *  STM32F407VGT6 - 168 MHz
 *
 *  SystemInit() is called from startup assembly before main().
 *  Configures FPU, caches, sets vector table, and starts HSI
 *  as a safe default clock until main() calls SystemClock_Config().
 *
 *  HAL tick variables are shared with FreeRTOS via SysTick_Handler
 *  in stm32f4xx_it.c (calls both HAL_IncTick and xPortSysTickHandler).
 * ============================================================ */

uint32_t SystemCoreClock = 168000000;

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                    1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

void SystemInit(void)
{
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << (10 * 2)) | (3UL << (11 * 2)));
#endif

#if (PREFETCH_ENABLE != 0)
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
#endif
#if (INSTRUCTION_CACHE_ENABLE != 0)
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif
#if (DATA_CACHE_ENABLE != 0)
    __HAL_FLASH_DATA_CACHE_ENABLE();
#endif

    SCB->VTOR = FLASH_BASE;

    /* Start HSI as safe boot clock */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {}

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}

    /* Reset RCC configuration registers */
    RCC->CR   &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->CFGR  = 0x00000000U;
    RCC->PLLCFGR = 0x24003010U;
    RCC->CR   &= ~RCC_CR_HSEBYP;
    RCC->CIR   = 0x00000000U;

    __set_PRIMASK(1);
    SCB->VTOR = FLASH_BASE;
}

void SystemCoreClockUpdate(void)
{
    uint32_t tmp = 0, pllvco = 0, pllm = 2;

    tmp = RCC->CFGR & RCC_CFGR_SWS;
    switch (tmp) {
    case RCC_CFGR_SWS_HSI:
        SystemCoreClock = HSI_VALUE;
        break;
    case RCC_CFGR_SWS_HSE:
        SystemCoreClock = HSE_VALUE;
        break;
    case RCC_CFGR_SWS_PLL: {
        uint32_t pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> RCC_PLLCFGR_PLLSRC_Pos;
        pllm = (RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos;
        if (pllsource != 0) {
            pllvco = (HSE_VALUE / pllm) *
                     ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
        } else {
            pllvco = (HSI_VALUE / pllm) *
                     ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
        }
        uint32_t pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1U) * 2U;
        SystemCoreClock = pllvco / pllp;
        break;
    }
    default:
        SystemCoreClock = HSI_VALUE;
        break;
    }
}

/*
 * HAL tick — shared between HAL_Delay and FreeRTOS task tick.
 * uwTick/uwTickFreq are defined in stm32f4xx_hal.c.
 * We provide strong overrides for the __weak HAL_IncTick/GetTick/Delay.
 */
void HAL_IncTick(void)
{
    uwTick += (uint32_t)uwTickFreq;
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

    while ((HAL_GetTick() - tickstart) < wait) {}
}
