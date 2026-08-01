/**
 * @file    app_limit.h
 * @brief   角度限位 — 双向弹簧 + 阻尼，模拟真实物理限位。
 * @details 旋钮越过限位边界后进入弹跳模式：以限位位置为锚点的双向弹簧，
 *          配合速度阻尼使振荡快速衰减。三种模式: OFF / SINGLE / DUAL。
 * @version 1.0.0
 * @date    2026/7/31
 */
#ifndef APP_LIMIT_H
#define APP_LIMIT_H

#include <stdint.h>

/**
 * @brief 限位模式
 */
typedef enum {
    KNOB_LIMIT_MODE_OFF = 0,    // 关闭限位
    KNOB_LIMIT_MODE_SINGLE,     // 单边限位（仅检查上限 limit_max_deg）
    KNOB_LIMIT_MODE_DUAL,       // 双边限位（上下界均检查）
} Knob_LimitMode_t;

/**
 * @brief 限位弹簧配置
 */
typedef struct {
    Knob_LimitMode_t mode;          // 限位模式
    float   limit_min_deg;          // 下界角度 (SINGLE 忽略，DUAL 生效)
    float   limit_max_deg;          // 上界角度 (SINGLE/DUAL 均生效)
    float   spring_kp;              // 弹簧刚度 (% 占空比 / °)
    float   spring_kd;              // 阻尼系数 (% 占空比 / (°/ms))
    uint8_t max_force_pct;          // 弹簧最大输出力 (% 占空比)
} Knob_LimitConfig_t;

/**
 * @brief 初始化限位模块
 * @param cfg 限位配置指针
 */
void App_Limit_Init(const Knob_LimitConfig_t *cfg);

/**
 * @brief 运行时更新限位配置（会重置弹跳状态）
 * @param cfg 限位配置指针
 */
void App_Limit_SetConfig(const Knob_LimitConfig_t *cfg);

/**
 * @brief 读取当前限位配置
 * @param cfg 输出配置指针
 */
void App_Limit_GetConfig(Knob_LimitConfig_t *cfg);

/**
 * @brief 执行限位检查，应在 ISR 中卡位逻辑之前调用
 * @param angle    当前旋钮角度 (°)
 * @param velocity 角速度 (°/ms，每 1ms 采样的角度差)
 * @return 1 = 限位弹簧占用本次 tick（跳过卡位逻辑），
 *         0 = 角度在限位范围内（正常执行卡位逻辑）
 */
int App_Limit_Check(float angle, float velocity);

#endif
