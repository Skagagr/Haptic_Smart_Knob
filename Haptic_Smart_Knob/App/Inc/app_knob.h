/**
 * @file    app_knob.h
 * @brief   力反馈旋钮 — EC11 棘轮式虚拟卡位。
 * @details TIM3 1kHz 中断驱动。可选的物理限位由 app_limit 模块处理
 *          （通过 app_limit.h 间接引入）。num_detents = 0 禁用卡位。
 * @version 1.1.0
 * @date    2026/7/31
 */
#ifndef APP_KNOB_H
#define APP_KNOB_H

#include <stdint.h>
#include "app_limit.h"

/**
 * @brief 虚拟卡位配置
 */
typedef struct {
    uint8_t num_detents;          // 每圈卡位数，0 = 禁用卡位
    float   detent_angle;         // 卡位间距角度 (360.0f / num_detents)
    float   dead_zone_deg;        // 死区 ±°，卡位中心附近电机不输出
    float   window_deg;           // 保留字段
    uint8_t max_torque_pct;       // 保留字段
} Knob_DetentConfig_t;

/**
 * @brief 初始化旋钮（编码器、电机、卡位、限位、TIM3 中断）
 */
void App_Knob_Init(void);

/**
 * @brief 控制循环，由 TIM3 ISR 以 1kHz 频率调用
 */
void App_Knob_Control(void);

/**
 * @brief 串口调试输出（角度、目标、误差、输出、卡位号）
 */
void App_Knob_Debug(void);

/**
 * @brief 运行时设置卡位配置
 * @param cfg 卡位配置指针
 */
void App_Knob_SetDetentConfig(const Knob_DetentConfig_t *cfg);

/**
 * @brief 读取当前卡位配置
 * @param cfg 输出配置指针
 */
void App_Knob_GetDetentConfig(Knob_DetentConfig_t *cfg);

#endif
