/**
 * @file    app_knob_event.h
 * @brief   事件通知层 — 蜂鸣器、调试输出
 * @details 统一管理卡位切换检测和事件触发
 * @version 2.0.0
 * @date    2026/8/2
 */
#ifndef APP_KNOB_EVENT_H
#define APP_KNOB_EVENT_H

#include <stdint.h>

/**
 * @brief 事件类型
 */
typedef enum
{
    KNOB_EVENT_DETENT_CROSS = 0,  // 跨过卡位
    KNOB_EVENT_LIMIT_HIT,         // 撞击限位
    KNOB_EVENT_STATE_CHANGE,      // 状态切换
} KnobEventType_t;

/**
 * @brief 初始化事件层
 */
void KnobEvent_Init(void);

/**
 * @brief 触发事件
 * @param type 事件类型
 */
void KnobEvent_Trigger(KnobEventType_t type);

/**
 * @brief 定时更新（用于蜂鸣器脉冲计时）
 */
void KnobEvent_Tick(void);

/**
 * @brief 更新卡位检测（每 tick 调用）
 * @param detent_index 当前卡位编号
 */
void KnobEvent_UpdateDetent(int detent_index);

#endif
