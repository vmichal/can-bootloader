/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */
#pragma once

using interrupt_service_routine = void (*)();

extern "C" {

void Default_Handler();
void HardFault_Handler();
void Reset_Handler();
__attribute__((weak, alias("Default_Handler"))) void NMI_Handler();
__attribute__((weak, alias("Default_Handler"))) void MemManage_Handler();
__attribute__((weak, alias("Default_Handler"))) void BusFault_Handler();
__attribute__((weak, alias("Default_Handler"))) void UsageFault_Handler();
__attribute__((weak, alias("Default_Handler"))) void SVC_Handler();
__attribute__((weak, alias("Default_Handler"))) void DebugMon_Handler();
__attribute__((weak, alias("Default_Handler"))) void PendSV_Handler();
__attribute__((weak, alias("Default_Handler"))) void SysTick_Handler();

#ifdef BOOT_STM32F4

__attribute__((weak, alias("Default_Handler"))) void WWDG_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void PVD_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TAMP_STAMP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void RTC_WKUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void FLASH_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void RCC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ADC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_TX_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_RX0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_RX1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_SCE_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI9_5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_BRK_TIM9_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_UP_TIM10_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_TRG_COM_TIM11_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_CC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C1_EV_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C1_ER_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C2_EV_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C2_ER_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI15_10_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void RTC_ALARM_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USB_FS_WKUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_BRK_TIM12_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_UP_TIM13_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_TRG_COM_TIM14_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_CC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_STREAM7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void FSMC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SDIO_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM6_DAC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ETH_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ETH_WKUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_TX_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_RX0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_RX1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_SCE_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_FS_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_STREAM7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C3_EV_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C3_ER_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_EP1_OUT_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_EP1_IN_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_WKUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DCMI_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CRYP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void HASH_RNG_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void FPU_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART8_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SAI1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void LCD_TFT_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void LCD_TFT_ERR_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2D_IRQHandler(void);

#define IRQ_HANDLERS \
    WWDG_IRQHandler, \
    PVD_IRQHandler, \
    TAMP_STAMP_IRQHandler, \
    RTC_WKUP_IRQHandler, \
    FLASH_IRQHandler, \
    RCC_IRQHandler, \
    EXTI0_IRQHandler, \
    EXTI1_IRQHandler, \
    EXTI2_IRQHandler, \
    EXTI3_IRQHandler, \
    EXTI4_IRQHandler, \
    DMA1_STREAM0_IRQHandler, \
    DMA1_STREAM1_IRQHandler, \
    DMA1_STREAM2_IRQHandler, \
    DMA1_STREAM3_IRQHandler, \
    DMA1_STREAM4_IRQHandler, \
    DMA1_STREAM5_IRQHandler, \
    DMA1_STREAM6_IRQHandler, \
    ADC_IRQHandler, \
    CAN1_TX_IRQHandler, \
    CAN1_RX0_IRQHandler, \
    CAN1_RX1_IRQHandler, \
    CAN1_SCE_IRQHandler, \
    EXTI9_5_IRQHandler, \
    TIM1_BRK_TIM9_IRQHandler, \
    TIM1_UP_TIM10_IRQHandler, \
    TIM1_TRG_COM_TIM11_IRQHandler, \
    TIM1_CC_IRQHandler, \
    TIM2_IRQHandler, \
    TIM3_IRQHandler, \
    TIM4_IRQHandler, \
    I2C1_EV_IRQHandler, \
    I2C1_ER_IRQHandler, \
    I2C2_EV_IRQHandler, \
    I2C2_ER_IRQHandler, \
    SPI1_IRQHandler, \
    SPI2_IRQHandler, \
    USART1_IRQHandler, \
    USART2_IRQHandler, \
    USART3_IRQHandler, \
    EXTI15_10_IRQHandler, \
    RTC_ALARM_IRQHandler, \
    USB_FS_WKUP_IRQHandler, \
    TIM8_BRK_TIM12_IRQHandler, \
    TIM8_UP_TIM13_IRQHandler, \
    TIM8_TRG_COM_TIM14_IRQHandler, \
    TIM8_CC_IRQHandler, \
    DMA1_STREAM7_IRQHandler, \
    FSMC_IRQHandler, \
    SDIO_IRQHandler, \
    TIM5_IRQHandler, \
    SPI3_IRQHandler, \
    UART4_IRQHandler, \
    UART5_IRQHandler, \
    TIM6_DAC_IRQHandler, \
    TIM7_IRQHandler, \
    DMA2_STREAM0_IRQHandler, \
    DMA2_STREAM1_IRQHandler, \
    DMA2_STREAM2_IRQHandler, \
    DMA2_STREAM3_IRQHandler, \
    DMA2_STREAM4_IRQHandler, \
    ETH_IRQHandler, \
    ETH_WKUP_IRQHandler, \
    CAN2_TX_IRQHandler, \
    CAN2_RX0_IRQHandler, \
    CAN2_RX1_IRQHandler, \
    CAN2_SCE_IRQHandler, \
    OTG_FS_IRQHandler, \
    DMA2_STREAM5_IRQHandler, \
    DMA2_STREAM6_IRQHandler, \
    DMA2_STREAM7_IRQHandler, \
    USART6_IRQHandler, \
    I2C3_EV_IRQHandler, \
    I2C3_ER_IRQHandler, \
    OTG_HS_EP1_OUT_IRQHandler, \
    OTG_HS_EP1_IN_IRQHandler, \
    OTG_HS_WKUP_IRQHandler, \
    OTG_HS_IRQHandler, \
    DCMI_IRQHandler, \
    CRYP_IRQHandler, \
    HASH_RNG_IRQHandler, \
    FPU_IRQHandler, \
    UART7_IRQHandler, \
    UART8_IRQHandler, \
    SPI4_IRQHandler, \
    SPI5_IRQHandler, \
    SPI6_IRQHandler, \
    SAI1_IRQHandler, \
    LCD_TFT_IRQHandler, \
    LCD_TFT_ERR_IRQHandler, \
    DMA2D_IRQHandler

#else
#ifdef BOOT_STM32F1

__attribute__((weak, alias("Default_Handler"))) void WWDG_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void PVD_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TAMPER_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void RTC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void FLASH_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void RCC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ADC1_2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USB_HP_CAN_TX_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USB_LP_CAN_RX0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN_RX1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN_SCE_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI9_5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_BRK_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_UP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_TRG_COM_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM1_CC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C1_EV_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C1_ER_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C2_EV_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void I2C2_ER_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USART3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void EXTI15_10_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void RTC_ALARM_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void USB_WAKEUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_BRK_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_UP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_TRG_COM_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM8_CC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ADC3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void FSMC_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SDIO_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel4_5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ETH_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void ETH_WKUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_TX_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_RX0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_RX1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN2_SCE_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_FS_IRQHandler(void);

#define IRQ_HANDLERS \
    WWDG_IRQHandler             ,\
    PVD_IRQHandler              ,\
    TAMPER_IRQHandler           ,\
    RTC_IRQHandler              ,\
    FLASH_IRQHandler            ,\
    RCC_IRQHandler              ,\
    EXTI0_IRQHandler            ,\
    EXTI1_IRQHandler            ,\
    EXTI2_IRQHandler            ,\
    EXTI3_IRQHandler            ,\
    EXTI4_IRQHandler            ,\
    DMA1_Channel1_IRQHandler    ,\
    DMA1_Channel2_IRQHandler    ,\
    DMA1_Channel3_IRQHandler    ,\
    DMA1_Channel4_IRQHandler    ,\
    DMA1_Channel5_IRQHandler    ,\
    DMA1_Channel6_IRQHandler    ,\
    DMA1_Channel7_IRQHandler    ,\
    ADC1_2_IRQHandler           ,\
    CAN1_TX_IRQHandler          ,\
    CAN1_RX0_IRQHandler         ,\
    CAN1_RX1_IRQHandler         ,\
    CAN1_SCE_IRQHandler         ,\
    EXTI9_5_IRQHandler          ,\
    TIM1_BRK_IRQHandler         ,\
    TIM1_UP_IRQHandler          ,\
    TIM1_TRG_COM_IRQHandler     ,\
    TIM1_CC_IRQHandler          ,\
    TIM2_IRQHandler             ,\
    TIM3_IRQHandler             ,\
    TIM4_IRQHandler             ,\
    I2C1_EV_IRQHandler          ,\
    I2C1_ER_IRQHandler          ,\
    I2C2_EV_IRQHandler          ,\
    I2C2_ER_IRQHandler          ,\
    SPI1_IRQHandler             ,\
    SPI2_IRQHandler             ,\
    USART1_IRQHandler           ,\
    USART2_IRQHandler           ,\
    USART3_IRQHandler           ,\
    EXTI15_10_IRQHandler        ,\
    RTC_Alarm_IRQHandler        ,\
    OTG_FS_WKUP_IRQHandler      ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    TIM5_IRQHandler             ,\
    SPI3_IRQHandler             ,\
    UART4_IRQHandler            ,\
    UART5_IRQHandler            ,\
    TIM6_IRQHandler             ,\
    TIM7_IRQHandler             ,\
    DMA2_Channel1_IRQHandler    ,\
    DMA2_Channel2_IRQHandler    ,\
    DMA2_Channel3_IRQHandler    ,\
    DMA2_Channel4_IRQHandler    ,\
    DMA2_Channel5_IRQHandler    ,\
    0                           ,\
    0                           ,\
    CAN2_TX_IRQHandler          ,\
    CAN2_RX0_IRQHandler         ,\
    CAN2_RX1_IRQHandler         ,\
    CAN2_SCE_IRQHandler         ,\
    OTG_FS_IRQHandler           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\
    0                           ,\

#else
#error MCU NOT SUPPORTED
#endif
#endif
}

