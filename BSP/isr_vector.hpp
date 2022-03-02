/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
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

#elif defined(BOOT_STM32F1)

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
__attribute__((weak, alias("Default_Handler"))) void CAN1_TX_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_RX0_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_RX1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void CAN1_SCE_IRQHandler(void);
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
__attribute__((weak, alias("Default_Handler"))) void RTC_Alarm_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void OTG_FS_WKUP_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void SPI3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void UART5_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM6_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void TIM7_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel1_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel2_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel3_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel4_IRQHandler(void);
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel5_IRQHandler(void);
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
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
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
    nullptr                     ,\
    nullptr                     ,\
    CAN2_TX_IRQHandler          ,\
    CAN2_RX0_IRQHandler         ,\
    CAN2_RX1_IRQHandler         ,\
    CAN2_SCE_IRQHandler         ,\
    OTG_FS_IRQHandler           ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\
    nullptr                     ,\

#elif defined(BOOT_STM32F7)

	__attribute__((weak, alias("Default_Handler"))) void WWDG_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void PVD_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TAMP_STAMP_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void RTC_WKUP_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void FLASH_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void RCC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI4_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream4_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream5_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream6_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void ADC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN1_TX_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN1_RX0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN1_RX1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN1_SCE_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI9_5_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM1_BRK_TIM9_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM1_UP_TIM10_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM1_TRG_COM_TIM11_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM1_CC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM4_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C1_EV_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C1_ER_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C2_EV_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C2_ER_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPI1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPI2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void USART1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void USART2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void USART3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void EXTI15_10_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void RTC_Alarm_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void OTG_FS_WKUP_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM8_BRK_TIM12_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM8_UP_TIM13_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM8_TRG_COM_TIM14_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM8_CC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream7_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void FMC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SDMMC1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM5_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPI3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void UART4_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void UART5_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM6_DAC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void TIM7_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream4_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void ETH_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void ETH_WKUP_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN2_TX_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN2_RX0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN2_RX1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN2_SCE_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void OTG_FS_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream5_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream6_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream7_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void USART6_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C3_EV_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C3_ER_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void OTG_HS_EP1_OUT_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void OTG_HS_EP1_IN_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void OTG_HS_WKUP_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void OTG_HS_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DCMI_IRQHandler();
	// __attribute__((weak, alias("Default_Handler"))) void CRYP_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void RNG_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void FPU_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void UART7_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void UART8_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPI4_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPI5_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPI6_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SAI1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void LTDC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void LTDC_ER_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DMA2D_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SAI2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void QUADSPI_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void LPTIM1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CEC_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C4_EV_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void I2C4_ER_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SPDIF_RX_IRQHandler();
	// __attribute__((weak, alias("Default_Handler"))) void DSIHOST_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DFSDM1_FLT0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DFSDM1_FLT1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DFSDM1_FLT2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void DFSDM1_FLT3_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void SDMMC2_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN3_TX_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN3_RX0_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN3_RX1_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void CAN3_SCE_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void JPEG_IRQHandler();
	__attribute__((weak, alias("Default_Handler"))) void MDIOS_IRQHandler();

#define IRQ_HANDLERS \
		WWDG_IRQHandler,                   /* 0x0040 Window WatchDog                                                            */                   \
		PVD_IRQHandler,                    /* 0x0044 PVD through EXTI Line detection											*/				   \
		TAMP_STAMP_IRQHandler,             /* 0x0048 Tamper and TimeStamps through the EXTI line								*/				   \
		RTC_WKUP_IRQHandler,               /* 0x004C RTC Wakeup through the EXTI line											*/				   \
		FLASH_IRQHandler,                  /* 0x0050 FLASH																		*/				   \
		RCC_IRQHandler,                    /* 0x0054 RCC																		*/				   \
		EXTI0_IRQHandler,                  /* 0x0058 EXTI Line0																	*/				   \
		EXTI1_IRQHandler,                  /* 0x005C EXTI Line1																	*/				   \
		EXTI2_IRQHandler,                  /* 0x0060 EXTI Line2																	*/				   \
		EXTI3_IRQHandler,                  /* 0x0064 EXTI Line3																	*/				   \
		EXTI4_IRQHandler,                  /* 0x0068 EXTI Line4																	*/				   \
		DMA1_Stream0_IRQHandler,           /* 0x006C DMA1 Stream 0																*/				   \
		DMA1_Stream1_IRQHandler,           /* 0x0070 DMA1 Stream 1																*/				   \
		DMA1_Stream2_IRQHandler,           /* 0x0074 DMA1 Stream 2																*/				   \
		DMA1_Stream3_IRQHandler,           /* 0x0078 DMA1 Stream 3																*/				   \
		DMA1_Stream4_IRQHandler,           /* 0x007C DMA1 Stream 4																*/				   \
		DMA1_Stream5_IRQHandler,           /* 0x0080 DMA1 Stream 5																*/				   \
		DMA1_Stream6_IRQHandler,           /* 0x0084 DMA1 Stream 6																*/				   \
		ADC_IRQHandler,                    /* 0x0088 ADC1, ADC2 and ADC3s														*/				   \
		CAN1_TX_IRQHandler,                /* 0x008C CAN1 TX																	*/				   \
		CAN1_RX0_IRQHandler,               /* 0x0090 CAN1 RX0																	*/				   \
		CAN1_RX1_IRQHandler,               /* 0x0094 CAN1 RX1																	*/				   \
		CAN1_SCE_IRQHandler,               /* 0x0098 CAN1 SCE																	*/				   \
		EXTI9_5_IRQHandler,                /* 0x009C External Line[9:5]s														*/				   \
		TIM1_BRK_TIM9_IRQHandler,          /* 0x00A0 TIM1 Break and TIM9														*/				   \
		TIM1_UP_TIM10_IRQHandler,          /* 0x00A4 TIM1 Update and TIM10														*/				   \
		TIM1_TRG_COM_TIM11_IRQHandler,     /* 0x00A8 TIM1 Trigger and Commutation and TIM11										*/				   \
		TIM1_CC_IRQHandler,                /* 0x00AC TIM1 Capture Compare														*/				   \
		TIM2_IRQHandler,                   /* 0x00B0 TIM2																		*/				   \
		TIM3_IRQHandler,                   /* 0x00B4 TIM3																		*/				   \
		TIM4_IRQHandler,                   /* 0x00B8 TIM4																		*/				   \
		I2C1_EV_IRQHandler,                /* 0x00BC I2C1 Event																	*/				   \
		I2C1_ER_IRQHandler,                /* 0x00C0 I2C1 Error																	*/				   \
		I2C2_EV_IRQHandler,                /* 0x00C4 I2C2 Event																	*/				   \
		I2C2_ER_IRQHandler,                /* 0x00C8 I2C2 Error																	*/				   \
		SPI1_IRQHandler,                   /* 0x00CC SPI1																		*/				   \
		SPI2_IRQHandler,                   /* 0x00D0 SPI2																		*/				   \
		USART1_IRQHandler,                 /* 0x00D4 USART1																		*/				   \
		USART2_IRQHandler,                 /* 0x00D8 USART2																		*/				   \
		USART3_IRQHandler,                 /* 0x00DC USART3																		*/				   \
		EXTI15_10_IRQHandler,              /* 0x00E0 External Line[15:10]s														*/				   \
		RTC_Alarm_IRQHandler,              /* 0x00E4 RTC Alarm (A and B) through EXTI Line										*/				   \
		OTG_FS_WKUP_IRQHandler,            /* 0x00E8 USB OTG FS Wakeup through EXTI line										*/				   \
		TIM8_BRK_TIM12_IRQHandler,         /* 0x00EC TIM8 Break and TIM12														*/				   \
		TIM8_UP_TIM13_IRQHandler,          /* 0x00F0 TIM8 Update and TIM13														*/				   \
		TIM8_TRG_COM_TIM14_IRQHandler,     /* 0x00F4 TIM8 Trigger and Commutation and TIM14										*/				   \
		TIM8_CC_IRQHandler,                /* 0x00F8 TIM8 Capture Compare														*/				   \
		DMA1_Stream7_IRQHandler,           /* 0x00FC DMA1 Stream7																*/				   \
		FMC_IRQHandler,                    /* 0x0100 FMC																		*/				   \
		SDMMC1_IRQHandler,                 /* 0x0104 SDMMC1																		*/				   \
		TIM5_IRQHandler,                   /* 0x0108 TIM5																		*/				   \
		SPI3_IRQHandler,                   /* 0x010C SPI3																		*/				   \
		UART4_IRQHandler,                  /* 0x0110 UART4																		*/				   \
		UART5_IRQHandler,                  /* 0x0114 UART5																		*/				   \
		TIM6_DAC_IRQHandler,               /* 0x0118 TIM6 and DAC1&2 underrun errors											*/				   \
		TIM7_IRQHandler,                   /* 0x011C TIM7																		*/				   \
		DMA2_Stream0_IRQHandler,           /* 0x0120 DMA2 Stream 0																*/				   \
		DMA2_Stream1_IRQHandler,           /* 0x0124 DMA2 Stream 1																*/				   \
		DMA2_Stream2_IRQHandler,           /* 0x0128 DMA2 Stream 2																*/				   \
		DMA2_Stream3_IRQHandler,           /* 0x012C DMA2 Stream 3																*/				   \
		DMA2_Stream4_IRQHandler,           /* 0x0130 DMA2 Stream 4																*/				   \
		ETH_IRQHandler,                    /* 0x0134 Ethernet																	*/				   \
		ETH_WKUP_IRQHandler,               /* 0x0138 Ethernet Wakeup through EXTI line											*/				   \
		CAN2_TX_IRQHandler,                /* 0x013C CAN2 TX																	*/				   \
		CAN2_RX0_IRQHandler,               /* 0x0140 CAN2 RX0																	*/				   \
		CAN2_RX1_IRQHandler,               /* 0x0144 CAN2 RX1																	*/				   \
		CAN2_SCE_IRQHandler,               /* 0x0148 CAN2 SCE																	*/				   \
		OTG_FS_IRQHandler,                 /* 0x014C USB OTG FS																	*/				   \
		DMA2_Stream5_IRQHandler,           /* 0x0150 DMA2 Stream 5																*/				   \
		DMA2_Stream6_IRQHandler,           /* 0x0154 DMA2 Stream 6																*/				   \
		DMA2_Stream7_IRQHandler,           /* 0x0158 DMA2 Stream 7																*/				   \
		USART6_IRQHandler,                 /* 0x015C USART6																		*/				   \
		I2C3_EV_IRQHandler,                /* 0x0160 I2C3 event																	*/				   \
		I2C3_ER_IRQHandler,                /* 0x0164 I2C3 error																	*/				   \
		OTG_HS_EP1_OUT_IRQHandler,         /* 0x0168 USB OTG HS End Point 1 Out													*/				   \
		OTG_HS_EP1_IN_IRQHandler,          /* 0x016C USB OTG HS End Point 1 In													*/				   \
		OTG_HS_WKUP_IRQHandler,            /* 0x0170 USB OTG HS Wakeup through EXTI												*/				   \
		OTG_HS_IRQHandler,                 /* 0x0174 USB OTG HS																	*/				   \
		DCMI_IRQHandler,                   /* 0x0178 DCMI																		*/				   \
		nullptr,                           /* 0x017C CRYP (CRYP_IRQHandler) - not in 767										*/			   \
		RNG_IRQHandler,                    /* 0x0180 Rng																		*/				   \
		FPU_IRQHandler,                    /* 0x0184 FPU																		*/				   \
		UART7_IRQHandler,                  /* 0x0188 UART7																		*/				   \
		UART8_IRQHandler,                  /* 0x018C UART8																		*/				   \
		SPI4_IRQHandler,                   /* 0x0190 SPI4																		*/				   \
		SPI5_IRQHandler,                   /* 0x0194 SPI5																		*/				   \
		SPI6_IRQHandler,                   /* 0x0198 SPI6																		*/				   \
		SAI1_IRQHandler,                   /* 0x019C SAI1																		*/				   \
		LTDC_IRQHandler,                   /* 0x01A0 LTDC																		*/				   \
		LTDC_ER_IRQHandler,                /* 0x01A4 LTDC error																	*/				   \
		DMA2D_IRQHandler,                  /* 0x01A8 DMA2D																		*/				   \
		SAI2_IRQHandler,                   /* 0x01AC SAI2																		*/				   \
		QUADSPI_IRQHandler,                /* 0x01B0 QUADSPI																	*/				   \
		LPTIM1_IRQHandler,                 /* 0x01B4 LPTIM1																		*/				   \
		CEC_IRQHandler,                    /* 0x01B8 HDMI_CEC																	*/				   \
		I2C4_EV_IRQHandler,                /* 0x01BC I2C4 Event																	*/				   \
		I2C4_ER_IRQHandler,                /* 0x01C0 I2C4 Error																	*/				   \
		SPDIF_RX_IRQHandler,               /* 0x01C4 SPDIF_RX																	*/				   \
		nullptr,                           /* 0x01C8 DSI host global interrupt (DSIHOST_IRQHandler) - no on 767			*/				   \
		DFSDM1_FLT0_IRQHandler,            /* 0x01CC DFSDM1 Filter 0 global interrupt											*/				   \
		DFSDM1_FLT1_IRQHandler,            /* 0x01D0 DFSDM1 Filter 1 global interrupt											*/				   \
		DFSDM1_FLT2_IRQHandler,            /* 0x01D4 DFSDM1 Filter 2 global interrupt											*/				   \
		DFSDM1_FLT3_IRQHandler,            /* 0x01D8 DFSDM1 Filter 3 global interrupt											*/				   \
		SDMMC2_IRQHandler,                 /* 0x01DC SDMMC2 global interrupt													*/				   \
		CAN3_TX_IRQHandler,                /* 0x01E0 CAN3 TX interrupt															*/				   \
		CAN3_RX0_IRQHandler,               /* 0x01E4 CAN3 RX0 interrupt															*/				   \
		CAN3_RX1_IRQHandler,               /* 0x01E8 CAN3 RX1 interrupt															*/				   \
		CAN3_SCE_IRQHandler,               /* 0x01EC CAN3 SCE interrupt															*/				   \
		JPEG_IRQHandler,                   /* 0x01F0 JPEG global interrupt														*/				   \
		MDIOS_IRQHandler                  /* 0x01F4 MDIO slave global interrupt												*/				   \

#elif defined BOOT_STM32F2

__attribute__((weak, alias("Default_Handler"))) void WWDG_IRQHandler(void); /* Window WatchDog              */
__attribute__((weak, alias("Default_Handler"))) void PVD_IRQHandler(void); /* PVD through EXTI Line detection */
__attribute__((weak, alias("Default_Handler"))) void TAMP_STAMP_IRQHandler(void); /* Tamper and TimeStamps through the EXTI line */
__attribute__((weak, alias("Default_Handler"))) void RTC_WKUP_IRQHandler(void); /* RTC Wakeup through the EXTI line */
__attribute__((weak, alias("Default_Handler"))) void FLASH_IRQHandler(void); /* FLASH                        */
__attribute__((weak, alias("Default_Handler"))) void RCC_IRQHandler(void); /* RCC                          */
__attribute__((weak, alias("Default_Handler"))) void EXTI0_IRQHandler(void); /* EXTI Line0                   */
__attribute__((weak, alias("Default_Handler"))) void EXTI1_IRQHandler(void); /* EXTI Line1                   */
__attribute__((weak, alias("Default_Handler"))) void EXTI2_IRQHandler(void); /* EXTI Line2                   */
__attribute__((weak, alias("Default_Handler"))) void EXTI3_IRQHandler(void); /* EXTI Line3                   */
__attribute__((weak, alias("Default_Handler"))) void EXTI4_IRQHandler(void); /* EXTI Line4                   */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream0_IRQHandler(void); /* DMA1 Stream 0                */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream1_IRQHandler(void); /* DMA1 Stream 1                */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream2_IRQHandler(void); /* DMA1 Stream 2                */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream3_IRQHandler(void); /* DMA1 Stream 3                */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream4_IRQHandler(void); /* DMA1 Stream 4                */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream5_IRQHandler(void); /* DMA1 Stream 5                */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream6_IRQHandler(void); /* DMA1 Stream 6                */
__attribute__((weak, alias("Default_Handler"))) void ADC_IRQHandler(void); /* ADC1, ADC2 and ADC3s         */
__attribute__((weak, alias("Default_Handler"))) void CAN1_TX_IRQHandler(void); /* CAN1 TX                      */
__attribute__((weak, alias("Default_Handler"))) void CAN1_RX0_IRQHandler(void); /* CAN1 RX0                     */
__attribute__((weak, alias("Default_Handler"))) void CAN1_RX1_IRQHandler(void); /* CAN1 RX1                     */
__attribute__((weak, alias("Default_Handler"))) void CAN1_SCE_IRQHandler(void); /* CAN1 SCE                     */
__attribute__((weak, alias("Default_Handler"))) void EXTI9_5_IRQHandler(void); /* External Line[9:5]s          */
__attribute__((weak, alias("Default_Handler"))) void TIM1_BRK_TIM9_IRQHandler(void); /* TIM1 Break and TIM9          */
__attribute__((weak, alias("Default_Handler"))) void TIM1_UP_TIM10_IRQHandler(void); /* TIM1 Update and TIM10        */
__attribute__((weak, alias("Default_Handler"))) void TIM1_TRG_COM_TIM11_IRQHandler(void); /* TIM1 Trigger and Commutation and TIM11 */
__attribute__((weak, alias("Default_Handler"))) void TIM1_CC_IRQHandler(void); /* TIM1 Capture Compare         */
__attribute__((weak, alias("Default_Handler"))) void TIM2_IRQHandler(void); /* TIM2                         */
__attribute__((weak, alias("Default_Handler"))) void TIM3_IRQHandler(void); /* TIM3                         */
__attribute__((weak, alias("Default_Handler"))) void TIM4_IRQHandler(void); /* TIM4                         */
__attribute__((weak, alias("Default_Handler"))) void I2C1_EV_IRQHandler(void); /* I2C1 Event                   */
__attribute__((weak, alias("Default_Handler"))) void I2C1_ER_IRQHandler(void); /* I2C1 Error                   */
__attribute__((weak, alias("Default_Handler"))) void I2C2_EV_IRQHandler(void); /* I2C2 Event                   */
__attribute__((weak, alias("Default_Handler"))) void I2C2_ER_IRQHandler(void); /* I2C2 Error                   */
__attribute__((weak, alias("Default_Handler"))) void SPI1_IRQHandler(void); /* SPI1                         */
__attribute__((weak, alias("Default_Handler"))) void SPI2_IRQHandler(void); /* SPI2                         */
__attribute__((weak, alias("Default_Handler"))) void USART1_IRQHandler(void); /* USART1                       */
__attribute__((weak, alias("Default_Handler"))) void USART2_IRQHandler(void); /* USART2                       */
__attribute__((weak, alias("Default_Handler"))) void USART3_IRQHandler(void); /* USART3                       */
__attribute__((weak, alias("Default_Handler"))) void EXTI15_10_IRQHandler(void); /* External Line[15:10]s        */
__attribute__((weak, alias("Default_Handler"))) void RTC_Alarm_IRQHandler(void); /* RTC Alarm (A and B) through EXTI Line */
__attribute__((weak, alias("Default_Handler"))) void OTG_FS_WKUP_IRQHandler(void); /* USB OTG FS Wakeup through EXTI line */
__attribute__((weak, alias("Default_Handler"))) void TIM8_BRK_TIM12_IRQHandler(void); /* TIM8 Break and TIM12         */
__attribute__((weak, alias("Default_Handler"))) void TIM8_UP_TIM13_IRQHandler(void); /* TIM8 Update and TIM13        */
__attribute__((weak, alias("Default_Handler"))) void TIM8_TRG_COM_TIM14_IRQHandler(void); /* TIM8 Trigger and Commutation and TIM14 */
__attribute__((weak, alias("Default_Handler"))) void TIM8_CC_IRQHandler(void); /* TIM8 Capture Compare         */
__attribute__((weak, alias("Default_Handler"))) void DMA1_Stream7_IRQHandler(void); /* DMA1 Stream7                 */
__attribute__((weak, alias("Default_Handler"))) void FSMC_IRQHandler(void); /* FSMC                         */
__attribute__((weak, alias("Default_Handler"))) void SDIO_IRQHandler(void); /* SDIO                         */
__attribute__((weak, alias("Default_Handler"))) void TIM5_IRQHandler(void); /* TIM5                         */
__attribute__((weak, alias("Default_Handler"))) void SPI3_IRQHandler(void); /* SPI3                         */
__attribute__((weak, alias("Default_Handler"))) void UART4_IRQHandler(void); /* UART4                        */
__attribute__((weak, alias("Default_Handler"))) void UART5_IRQHandler(void); /* UART5                        */
__attribute__((weak, alias("Default_Handler"))) void TIM6_DAC_IRQHandler(void); /* TIM6 and DAC1&2 underrun errors */
__attribute__((weak, alias("Default_Handler"))) void TIM7_IRQHandler(void); /* TIM7                         */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream0_IRQHandler(void); /* DMA2 Stream 0                */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream1_IRQHandler(void); /* DMA2 Stream 1                */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream2_IRQHandler(void); /* DMA2 Stream 2                */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream3_IRQHandler(void); /* DMA2 Stream 3                */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream4_IRQHandler(void); /* DMA2 Stream 4                */
__attribute__((weak, alias("Default_Handler"))) void CAN2_TX_IRQHandler(void); /* CAN2 TX                      */
__attribute__((weak, alias("Default_Handler"))) void CAN2_RX0_IRQHandler(void); /* CAN2 RX0                     */
__attribute__((weak, alias("Default_Handler"))) void CAN2_RX1_IRQHandler(void); /* CAN2 RX1                     */
__attribute__((weak, alias("Default_Handler"))) void CAN2_SCE_IRQHandler(void); /* CAN2 SCE                     */
__attribute__((weak, alias("Default_Handler"))) void OTG_FS_IRQHandler(void); /* USB OTG FS                   */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream5_IRQHandler(void); /* DMA2 Stream 5                */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream6_IRQHandler(void); /* DMA2 Stream 6                */
__attribute__((weak, alias("Default_Handler"))) void DMA2_Stream7_IRQHandler(void); /* DMA2 Stream 7                */
__attribute__((weak, alias("Default_Handler"))) void USART6_IRQHandler(void); /* USART6                       */
__attribute__((weak, alias("Default_Handler"))) void I2C3_EV_IRQHandler(void); /* I2C3 event                   */
__attribute__((weak, alias("Default_Handler"))) void I2C3_ER_IRQHandler(void); /* I2C3 error                   */
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_EP1_OUT_IRQHandler(void); /* USB OTG HS End Point 1 Out   */
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_EP1_IN_IRQHandler(void); /* USB OTG HS End Point 1 In    */
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_WKUP_IRQHandler(void); /* USB OTG HS Wakeup through EXTI */
__attribute__((weak, alias("Default_Handler"))) void OTG_HS_IRQHandler(void); /* USB OTG HS                   */
__attribute__((weak, alias("Default_Handler"))) void HASH_RNG_IRQHandler(void); /* Hash and Rng                 */


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
	 DMA1_Stream0_IRQHandler, \
	 DMA1_Stream1_IRQHandler, \
	 DMA1_Stream2_IRQHandler, \
	 DMA1_Stream3_IRQHandler, \
	 DMA1_Stream4_IRQHandler, \
	 DMA1_Stream5_IRQHandler, \
	 DMA1_Stream6_IRQHandler, \
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
	 RTC_Alarm_IRQHandler, \
	 OTG_FS_WKUP_IRQHandler, \
	 TIM8_BRK_TIM12_IRQHandler, \
	 TIM8_UP_TIM13_IRQHandler, \
	 TIM8_TRG_COM_TIM14_IRQHandler, \
	 TIM8_CC_IRQHandler, \
	 DMA1_Stream7_IRQHandler, \
	 FSMC_IRQHandler, \
	 SDIO_IRQHandler, \
	 TIM5_IRQHandler, \
	 SPI3_IRQHandler, \
	 UART4_IRQHandler, \
	 UART5_IRQHandler, \
	 TIM6_DAC_IRQHandler, \
	 TIM7_IRQHandler, \
	 DMA2_Stream0_IRQHandler, \
	 DMA2_Stream1_IRQHandler, \
	 DMA2_Stream2_IRQHandler, \
	 DMA2_Stream3_IRQHandler, \
	 DMA2_Stream4_IRQHandler, \
	 nullptr, \
	 nullptr, \
	 CAN2_TX_IRQHandler, \
	 CAN2_RX0_IRQHandler, \
	 CAN2_RX1_IRQHandler, \
	 CAN2_SCE_IRQHandler, \
	 OTG_FS_IRQHandler, \
	 DMA2_Stream5_IRQHandler, \
	 DMA2_Stream6_IRQHandler, \
	 DMA2_Stream7_IRQHandler, \
	 USART6_IRQHandler, \
	 I2C3_EV_IRQHandler, \
	 I2C3_ER_IRQHandler, \
	 OTG_HS_EP1_OUT_IRQHandler, \
	 OTG_HS_EP1_IN_IRQHandler, \
	 OTG_HS_WKUP_IRQHandler, \
	 OTG_HS_IRQHandler, \
	 nullptr, \
	 nullptr, \
	 HASH_RNG_IRQHandler, \


#elif defined(BOOT_STM32G4)
__attribute__((weak, alias("Default_Handler"))) void WWDG_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void PVD_PVM_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void RTC_TAMP_LSECSS_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void RTC_WKUP_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FLASH_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void RCC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI0_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel5_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel6_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel7_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void ADC1_2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void USB_HP_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void USB_LP_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FDCAN1_IT0_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FDCAN1_IT1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI9_5_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM1_BRK_TIM15_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM1_UP_TIM16_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM1_TRG_COM_TIM17_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM1_CC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C1_EV_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C1_ER_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C2_EV_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C2_ER_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void SPI1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void SPI2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void USART1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void USART2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void USART3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void EXTI15_10_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void RTC_Alarm_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void USBWakeUp_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM8_BRK_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM8_UP_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM8_TRG_COM_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM8_CC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void ADC3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FMC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void LPTIM1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM5_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void SPI3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void UART4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void UART5_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM6_DAC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM7_DAC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel2_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel5_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void ADC4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void ADC5_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void UCPD1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void COMP1_2_3_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void COMP4_5_6_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void COMP7_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_Master_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_TIMA_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_TIMB_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_TIMC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_TIMD_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_TIME_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_FLT_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void HRTIM1_TIMF_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void CRS_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void SAI1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM20_BRK_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM20_UP_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM20_TRG_COM_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void TIM20_CC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FPU_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C4_EV_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C4_ER_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void SPI4_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FDCAN2_IT0_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FDCAN2_IT1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FDCAN3_IT0_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FDCAN3_IT1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void RNG_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void LPUART1_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C3_EV_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void I2C3_ER_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMAMUX_OVR_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void QUADSPI_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA1_Channel8_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel6_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel7_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void DMA2_Channel8_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void CORDIC_IRQHandler();
__attribute__((weak, alias("Default_Handler"))) void FMAC_IRQHandler();


#define IRQ_HANDLERS \
	WWDG_IRQHandler, \
	PVD_PVM_IRQHandler, \
	RTC_TAMP_LSECSS_IRQHandler, \
	RTC_WKUP_IRQHandler, \
	FLASH_IRQHandler, \
	RCC_IRQHandler, \
	EXTI0_IRQHandler, \
	EXTI1_IRQHandler, \
	EXTI2_IRQHandler, \
	EXTI3_IRQHandler, \
	EXTI4_IRQHandler, \
	DMA1_Channel1_IRQHandler, \
	DMA1_Channel2_IRQHandler, \
	DMA1_Channel3_IRQHandler, \
	DMA1_Channel4_IRQHandler, \
	DMA1_Channel5_IRQHandler, \
	DMA1_Channel6_IRQHandler, \
	DMA1_Channel7_IRQHandler, \
	ADC1_2_IRQHandler, \
	USB_HP_IRQHandler, \
	USB_LP_IRQHandler, \
	FDCAN1_IT0_IRQHandler, \
	FDCAN1_IT1_IRQHandler, \
	EXTI9_5_IRQHandler, \
	TIM1_BRK_TIM15_IRQHandler, \
	TIM1_UP_TIM16_IRQHandler, \
	TIM1_TRG_COM_TIM17_IRQHandler, \
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
	RTC_Alarm_IRQHandler, \
	USBWakeUp_IRQHandler, \
	TIM8_BRK_IRQHandler, \
	TIM8_UP_IRQHandler, \
	TIM8_TRG_COM_IRQHandler, \
	TIM8_CC_IRQHandler, \
	ADC3_IRQHandler, \
	FMC_IRQHandler, \
	LPTIM1_IRQHandler, \
	TIM5_IRQHandler, \
	SPI3_IRQHandler, \
	UART4_IRQHandler, \
	UART5_IRQHandler, \
	TIM6_DAC_IRQHandler, \
	TIM7_DAC_IRQHandler, \
	DMA2_Channel1_IRQHandler, \
	DMA2_Channel2_IRQHandler, \
	DMA2_Channel3_IRQHandler, \
	DMA2_Channel4_IRQHandler, \
	DMA2_Channel5_IRQHandler, \
	ADC4_IRQHandler, \
	ADC5_IRQHandler, \
	UCPD1_IRQHandler, \
	COMP1_2_3_IRQHandler, \
	COMP4_5_6_IRQHandler, \
	COMP7_IRQHandler, \
	HRTIM1_Master_IRQHandler, \
	HRTIM1_TIMA_IRQHandler, \
	HRTIM1_TIMB_IRQHandler, \
	HRTIM1_TIMC_IRQHandler, \
	HRTIM1_TIMD_IRQHandler, \
	HRTIM1_TIME_IRQHandler, \
	HRTIM1_FLT_IRQHandler, \
	HRTIM1_TIMF_IRQHandler, \
	CRS_IRQHandler, \
	SAI1_IRQHandler, \
	TIM20_BRK_IRQHandler, \
	TIM20_UP_IRQHandler, \
	TIM20_TRG_COM_IRQHandler, \
	TIM20_CC_IRQHandler, \
	FPU_IRQHandler, \
	I2C4_EV_IRQHandler, \
	I2C4_ER_IRQHandler, \
	SPI4_IRQHandler, \
	nullptr, \
	FDCAN2_IT0_IRQHandler, \
	FDCAN2_IT1_IRQHandler, \
	FDCAN3_IT0_IRQHandler, \
	FDCAN3_IT1_IRQHandler, \
	RNG_IRQHandler, \
	LPUART1_IRQHandler, \
	I2C3_EV_IRQHandler, \
	I2C3_ER_IRQHandler, \
	DMAMUX_OVR_IRQHandler, \
	QUADSPI_IRQHandler, \
	DMA1_Channel8_IRQHandler, \
	DMA2_Channel6_IRQHandler, \
	DMA2_Channel7_IRQHandler, \
	DMA2_Channel8_IRQHandler, \
	CORDIC_IRQHandler, \
	FMAC_IRQHandler

#else
#error MCU NOT SUPPORTED
#endif
}

