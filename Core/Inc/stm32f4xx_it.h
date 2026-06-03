#ifndef __STM32F4XX_IT_H
#define __STM32F4XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

void ETH_IRQHandler(void);
void ETH_WKUP_IRQHandler(void);

void USART1_IRQHandler(void);

void DMA2_Stream7_IRQHandler(void);
void DMA2_Stream2_IRQHandler(void);

void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4XX_IT_H */
