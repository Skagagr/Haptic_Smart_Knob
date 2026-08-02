/**
 * @file    app_knob_limit.h
 * @brief   角度限位 — 事件驱动，独立状态机
 * @details 双向弹簧 + 阻尼，通过回调通知状态切换
 * @version 2.0.0
 * @date    2026/8/2
 */
#ifndef APP_KNOB_LIMIT_H
#define APP_KNOB_LIMIT_H

#include <stdint.h>
#include "app_knob_types.h"

// ===== 限位默认值 =====
#define KNOB_LIMIT_DEFAULT_MODE    KNOB_LIMIT_MODE_DUAL
#define KNOB_LIMIT_DEFAULT_MIN     -180.0f
#define KNOB_LIMIT_DEFAULT_MAX     360.0f
#define KNOB_LIMIT_SPRING_KP       4.0f
#define KNOB_LIMIT_SPRING_KD       1.5f
#define KNOB_LIMIT_MAX_FORCE_PCT   55

/**
 * @brief 限位模式
 */
typedef enum
{
    KNOB_LIMIT_MODE_OFF = 0,
    KNOB_LIMIT_MODE_SINGLE,
    KNOB_LIMIT_MODE_DUAL,
} Knob_LimitMode_t;

/**
 * @brief 限位弹簧配置
 */
typedef struct
{
    Knob_LimitMode_t mode;
    float   limit_min_deg;
    float   limit_max_deg;
    float   spring_kp;
    float   spring_kd;
    uint8_t max_force_pct;
} Knob_LimitConfig_t;

/**
 * @brief 事件回调类型
 */
typedef void (*LimitEventCallback_t)(void);

/**
 * @brief 初始化限位模块（注册回调）
 * @param on_enter 进入限位弹跳时的回调
 * @param on_exit 退出限位弹跳时的回调
 */
void KnobLimit_Init(LimitEventCallback_t on_enter,
                    LimitEventCallback_t on_exit);

/**
 * @brief 检查并更新限位状态（每 tick 调用）
 * @param sensor 传感器数据
 * @param output 输出力指令（限位激活时填充）
 */
void KnobLimit_Update(const KnobSensorData_t *sensor,
                      KnobForceOutput_t *output);

/**
 * @brief 查询限位是否激活
 * @return 1 = 激活中，0 = 未激活
 */
int KnobLimit_IsActive(void);

/**
 * @brief 运行时更新限位配置
 * @param cfg 限位配置指针
 */
void KnobLimit_SetConfig(const Knob_LimitConfig_t *cfg);

/**
 * @brief 读取当前限位配置
 * @param cfg 输出配置指针
 */
void KnobLimit_GetConfig(Knob_LimitConfig_t *cfg);

#endif
