/**
 * @file    app_knob_limit.h
 * @brief   角度限位 — 双向弹簧 + 阻尼，越界时推回边界内
 * @details 独立于主控制循环的限位保护模块。
 *          支持三种模式：关闭 / 单边 / 双边。
 *          检测到越界时直接调用 AppKnob_OnLimitEnter/Exit 通知主模块。
 * @version 3.0.0
 * @date    2026/8/3
 */
#ifndef APP_KNOB_LIMIT_H
#define APP_KNOB_LIMIT_H

#include <stdint.h>
#include "app_knob.h"

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
    KNOB_LIMIT_MODE_OFF = 0,    ///< 关闭限位
    KNOB_LIMIT_MODE_SINGLE,     ///< 单边限位（仅上限）
    KNOB_LIMIT_MODE_DUAL,       ///< 双边限位（上下限均有效）
} Knob_LimitMode_t;

/**
 * @brief 限位弹簧配置
 */
typedef struct
{
    Knob_LimitMode_t mode;          ///< 限位模式
    float   limit_min_deg;          ///< 下限角度 (°)
    float   limit_max_deg;          ///< 上限角度 (°)
    float   spring_kp;              ///< 弹簧刚度系数
    float   spring_kd;              ///< 阻尼系数
    uint8_t max_force_pct;          ///< 最大力百分比 (0-100)
} Knob_LimitConfig_t;

/**
 * @brief 初始化限位模块
 * @details 加载默认配置，复位内部状态。
 *          在主控制循环初始化时调用一次。
 */
void KnobLimit_Init(void);

/**
 * @brief 检查并更新限位状态（每 tick 调用）
 * @details 检测越界 → 进入弹跳 → 弹簧推回 → 稳定后退出。
 *          进入/退出时直接调用 AppKnob_OnLimitEnter/Exit 通知主模块。
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
