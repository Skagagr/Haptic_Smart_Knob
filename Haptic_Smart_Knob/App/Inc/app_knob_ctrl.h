/**
 * @file    app_knob_ctrl.h
 * @brief   旋钮控制协调层 — 顶层入口
 * @details 简化的控制循环，协调所有子模块
 * @version 2.0.0
 * @date    2026/8/2
 */
#ifndef APP_KNOB_CTRL_H
#define APP_KNOB_CTRL_H

#include "app_knob_types.h"

/**
 * @brief 初始化旋钮系统
 */
void App_Knob_Init(void);

/**
 * @brief 1kHz 控制循环（由 TIM3 ISR 调用）
 */
void App_Knob_Control(void);

/**
 * @brief 串口调试输出（由 main 循环调用）
 */
void App_Knob_Debug(void);

/**
 * @brief 运行时配置接口
 * @param cfg 配置指针
 */
void App_Knob_SetConfig(const KnobConfig_t *cfg);

/**
 * @brief 读取当前配置
 * @param cfg 输出配置指针
 */
void App_Knob_GetConfig(KnobConfig_t *cfg);

#endif
