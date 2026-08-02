/**
 * @file    app_isr.c
 * @brief   HAL 弱符号回调 — TIM3 1kHz 控制循环入口。
 * @details 覆盖 HAL 默认弱符号:
 *          - HAL_TIM_PeriodElapsedCallback: TIM3 → App_Knob_Control
 *          本文件是唯一引用 Core 层头文件的 App 模块。
 * @version 1.1.0
 * @date    2026/8/2
 */
#include "app_isr.h"
#include "app_knob_ctrl.h"
#include "tim.h"

void App_ISR_Init(void)
{
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        App_Knob_Control();
    }
}
