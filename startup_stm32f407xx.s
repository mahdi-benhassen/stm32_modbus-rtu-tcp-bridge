/*
 * startup_stm32f407xx.s
 *
 * STM32F407VGT6 Startup Code for GCC / ARM Cortex-M4 with FPU
 * Minimal startup - initializes data/bss, calls SystemInit, calls main.
 */

  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

  .global g_pfnVectors
  .global Default_Handler

  .word _sidata
  .word _sdata
  .word _edata
  .word _sbss
  .word _ebss

  .section .isr_vector, "a", %progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* External Interrupts */
  .word WWDG_IRQHandler
  .word PVD_IRQHandler
  .word TAMP_STAMP_IRQHandler
  .word RTC_WKUP_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Stream0_IRQHandler
  .word DMA1_Stream1_IRQHandler
  .word DMA1_Stream2_IRQHandler
  .word DMA1_Stream3_IRQHandler
  .word DMA1_Stream4_IRQHandler
  .word DMA1_Stream5_IRQHandler
  .word DMA1_Stream6_IRQHandler
  .word ADC_IRQHandler
  .word CAN1_TX_IRQHandler
  .word CAN1_RX0_IRQHandler
  .word CAN1_RX1_IRQHandler
  .word CAN1_SCE_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_TIM9_IRQHandler
  .word TIM1_UP_TIM10_IRQHandler
  .word TIM1_TRG_COM_TIM11_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTC_Alarm_IRQHandler
  .word OTG_FS_WKUP_IRQHandler
  .word TIM8_BRK_TIM12_IRQHandler
  .word TIM8_UP_TIM13_IRQHandler
  .word TIM8_TRG_COM_TIM14_IRQHandler
  .word TIM8_CC_IRQHandler
  .word DMA1_Stream7_IRQHandler
  .word FSMC_IRQHandler
  .word SDIO_IRQHandler
  .word TIM5_IRQHandler
  .word SPI3_IRQHandler
  .word UART4_IRQHandler
  .word UART5_IRQHandler
  .word TIM6_DAC_IRQHandler
  .word TIM7_IRQHandler
  .word DMA2_Stream0_IRQHandler
  .word DMA2_Stream1_IRQHandler
  .word DMA2_Stream2_IRQHandler
  .word DMA2_Stream3_IRQHandler
  .word DMA2_Stream4_IRQHandler
  .word ETH_IRQHandler
  .word ETH_WKUP_IRQHandler
  .word CAN2_TX_IRQHandler
  .word CAN2_RX0_IRQHandler
  .word CAN2_RX1_IRQHandler
  .word CAN2_SCE_IRQHandler
  .word OTG_FS_IRQHandler
  .word DMA2_Stream5_IRQHandler
  .word DMA2_Stream6_IRQHandler
  .word DMA2_Stream7_IRQHandler
  .word USART6_IRQHandler
  .word I2C3_EV_IRQHandler
  .word I2C3_ER_IRQHandler
  .word OTG_HS_EP1_OUT_IRQHandler
  .word OTG_HS_EP1_IN_IRQHandler
  .word OTG_HS_WKUP_IRQHandler
  .word OTG_HS_IRQHandler
  .word DCMI_IRQHandler
  .word 0
  .word HASH_RNG_IRQHandler
  .word FPU_IRQHandler

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack

  /* Copy .data from FLASH to RAM */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  b     .LoopCopyDataInit

.CopyDataInit:
  ldr   r3, [r2], #4
  str   r3, [r0], #4

.LoopCopyDataInit:
  cmp   r0, r1
  bcc   .CopyDataInit

  /* Zero-fill .bss */
  ldr   r0, =_sbss
  ldr   r1, =_ebss
  mov   r2, #0
  b     .LoopFillZerobss

.FillZerobss:
  str   r2, [r0], #4

.LoopFillZerobss:
  cmp   r0, r1
  bcc   .FillZerobss

  /* Enable FPU */
  ldr   r0, =0xE000ED88
  ldr   r1, [r0]
  orr   r1, r1, #(0xF << 20)
  str   r1, [r0]
  dsb
  isb

  /* Call SystemInit (HAL) */
  bl    SystemInit
  /* Call main */
  bl    main
  /* Loop if main returns */
  b     .

  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b   Infinite_Loop
  .size Default_Handler, .-Default_Handler

  .macro def_default_handler handler_name
  .weak \handler_name
  .thumb_set \handler_name, Default_Handler
  .endm

  def_default_handler NMI_Handler
  def_default_handler HardFault_Handler
  def_default_handler MemManage_Handler
  def_default_handler BusFault_Handler
  def_default_handler UsageFault_Handler
  def_default_handler SVC_Handler
  def_default_handler DebugMon_Handler
  def_default_handler PendSV_Handler
  def_default_handler SysTick_Handler
  def_default_handler WWDG_IRQHandler
  def_default_handler PVD_IRQHandler
  def_default_handler TAMP_STAMP_IRQHandler
  def_default_handler RTC_WKUP_IRQHandler
  def_default_handler FLASH_IRQHandler
  def_default_handler RCC_IRQHandler
  def_default_handler EXTI0_IRQHandler
  def_default_handler EXTI1_IRQHandler
  def_default_handler EXTI2_IRQHandler
  def_default_handler EXTI3_IRQHandler
  def_default_handler EXTI4_IRQHandler
  def_default_handler DMA1_Stream0_IRQHandler
  def_default_handler DMA1_Stream1_IRQHandler
  def_default_handler DMA1_Stream2_IRQHandler
  def_default_handler DMA1_Stream3_IRQHandler
  def_default_handler DMA1_Stream4_IRQHandler
  def_default_handler DMA1_Stream5_IRQHandler
  def_default_handler DMA1_Stream6_IRQHandler
  def_default_handler ADC_IRQHandler
  def_default_handler CAN1_TX_IRQHandler
  def_default_handler CAN1_RX0_IRQHandler
  def_default_handler CAN1_RX1_IRQHandler
  def_default_handler CAN1_SCE_IRQHandler
  def_default_handler EXTI9_5_IRQHandler
  def_default_handler TIM1_BRK_TIM9_IRQHandler
  def_default_handler TIM1_UP_TIM10_IRQHandler
  def_default_handler TIM1_TRG_COM_TIM11_IRQHandler
  def_default_handler TIM1_CC_IRQHandler
  def_default_handler TIM4_IRQHandler
  def_default_handler I2C1_EV_IRQHandler
  def_default_handler I2C1_ER_IRQHandler
  def_default_handler I2C2_EV_IRQHandler
  def_default_handler I2C2_ER_IRQHandler
  def_default_handler SPI1_IRQHandler
  def_default_handler SPI2_IRQHandler
  def_default_handler USART2_IRQHandler
  def_default_handler USART3_IRQHandler
  def_default_handler EXTI15_10_IRQHandler
  def_default_handler RTC_Alarm_IRQHandler
  def_default_handler OTG_FS_WKUP_IRQHandler
  def_default_handler TIM8_BRK_TIM12_IRQHandler
  def_default_handler TIM8_UP_TIM13_IRQHandler
  def_default_handler TIM8_TRG_COM_TIM14_IRQHandler
  def_default_handler TIM8_CC_IRQHandler
  def_default_handler DMA1_Stream7_IRQHandler
  def_default_handler FSMC_IRQHandler
  def_default_handler SDIO_IRQHandler
  def_default_handler TIM5_IRQHandler
  def_default_handler SPI3_IRQHandler
  def_default_handler UART4_IRQHandler
  def_default_handler UART5_IRQHandler
  def_default_handler TIM6_DAC_IRQHandler
  def_default_handler TIM7_IRQHandler
  def_default_handler DMA2_Stream0_IRQHandler
  def_default_handler DMA2_Stream1_IRQHandler
  def_default_handler DMA2_Stream3_IRQHandler
  def_default_handler DMA2_Stream4_IRQHandler
  def_default_handler CAN2_TX_IRQHandler
  def_default_handler CAN2_RX0_IRQHandler
  def_default_handler CAN2_RX1_IRQHandler
  def_default_handler CAN2_SCE_IRQHandler
  def_default_handler OTG_FS_IRQHandler
  def_default_handler DMA2_Stream5_IRQHandler
  def_default_handler DMA2_Stream6_IRQHandler
  def_default_handler USART6_IRQHandler
  def_default_handler I2C3_EV_IRQHandler
  def_default_handler I2C3_ER_IRQHandler
  def_default_handler OTG_HS_EP1_OUT_IRQHandler
  def_default_handler OTG_HS_EP1_IN_IRQHandler
  def_default_handler OTG_HS_WKUP_IRQHandler
  def_default_handler OTG_HS_IRQHandler
  def_default_handler DCMI_IRQHandler
  def_default_handler HASH_RNG_IRQHandler
  def_default_handler FPU_IRQHandler
