/**
 * @file    app_mode.h
 * @brief   控制模式管理 — 模式状态 + 按钮业务逻辑
 * @details 维护当前控制模式（空闲/音量/亮度），响应按钮切换，
 *          模式变化时点亮对应 LED（通过 BSP_KnobIndicator）。
 *          App 层决定"按钮对应什么业务动作"，BSP 层只管硬件。
 *          模式状态供协议层读取上报、供上位机设置同步。
 * @version 1.0.0
 * @date    2026/8/5
 */
#ifndef APP_MODE_H
#define APP_MODE_H

#include <stdint.h>

/**
 * @brief 控制模式枚举
 * @details 与上位机 KnobControlMode 对齐（IDLE=0/VOLUME=1/BRIGHTNESS=2）
 */
typedef enum
{
    KNOB_STATE_MODE_IDLE = 0,       ///< 空闲（不控制音量/亮度）
    KNOB_STATE_MODE_VOLUME,         ///< 控制音量
    KNOB_STATE_MODE_BRIGHTNESS,     ///< 控制亮度
} KnobStateMode_t;

/**
 * @brief 初始化控制模式模块（默认空闲，点亮对应 LED）
 */
void AppMode_Init(void);

/**
 * @brief 获取当前控制模式
 * @return 当前模式
 */
KnobStateMode_t AppMode_GetMode(void);

/**
 * @brief 设置控制模式（更新模式状态 + 点亮对应 LED）
 * @param mode 目标模式
 */
void AppMode_SetMode(KnobStateMode_t mode);

/**
 * @brief 切换到下一状态（空闲→音量→亮度→空闲 循环）
 */
void AppMode_NextMode(void);

/**
 * @brief 按键扫描（由 1kHz 控制循环调用）：消抖后处理按钮业务逻辑
 * @details 在 App_Knob_Control() 第 8 步调用，每 1ms 执行一次，
 *          实时响应按键，不受主循环 HAL_Delay 阻塞影响。
 *          PB12=回空闲；PB13=切下一状态
 */
void AppMode_ScanButtons(void);

#endif
