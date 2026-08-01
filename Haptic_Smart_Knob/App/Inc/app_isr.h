/**
 * @file    app_isr.h
 * @brief   HAL 弱符号回调入口。
 * @details 所有 HAL 回调（TIM、UART 等）的弱符号覆盖集中在此，
 *          作为 App 层与 HAL/Core 之间的薄适配层。
 *          仅本文件引用 Core 头文件 (tim.h, usart.h)。
 * @version 1.0.0
 * @date    2026/8/1
 */
#ifndef APP_ISR_H
#define APP_ISR_H

void App_ISR_Init(void);

#endif
