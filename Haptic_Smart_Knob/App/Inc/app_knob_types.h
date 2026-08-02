/**
 * @file    app_knob_types.h
 * @brief   旋钮系统公共类型定义
 * @details 定义所有模块间共享的数据结构，简化配置系统。
 *          本文件是整个旋钮系统的类型基础，被所有模块引用。
 * @version 2.0.0
 * @date    2026/8/2
 */
#ifndef APP_KNOB_TYPES_H
#define APP_KNOB_TYPES_H

#include <stdint.h>
#include "bsp_knob_motor.h"

/**
 * @brief 配置预设枚举（简化参数系统）
 * @details 每个预设对应一组经过调优的参数（卡位数、力度、死区等），
 *          用户只需选择预设，无需理解底层 22 个参数的含义。
 */
typedef enum
{
    KNOB_PRESET_COARSE_6 = 0,   ///< 6 卡位/圈，粗糙手感，大角度间距 (60°)
    KNOB_PRESET_NORMAL_12,      ///< 12 卡位/圈，标准手感，中等间距 (30°)
    KNOB_PRESET_FINE_24,        ///< 24 卡位/圈，精细手感，小角度间距 (15°)
    KNOB_PRESET_DENSE_48,       ///< 48 卡位/圈，密集手感，微小间距 (7.5°)
    KNOB_PRESET_SMOOTH,         ///< 完全平滑，无卡位，自由旋转
} KnobPreset_t;

/**
 * @brief 运行时配置结构体（对象化配置）
 * @details 新版配置系统只需 3 个参数，替代旧版 22 个宏定义。
 *          - preset: 选择预设方案，决定卡位数和基础参数
 *          - detent_strength: 卡位爬坡阻力，数值越大越难翻过卡位
 *          - return_strength: 松手归中力，数值越大归中速度越快
 */
typedef struct
{
    KnobPreset_t preset;        ///< 预设方案（决定卡位数和基础手感）
    uint8_t detent_strength;    ///< 卡位力度 (1-10)，1=很轻，10=很重
    uint8_t return_strength;    ///< 归中力度 (1-10)，1=很弱，10=很强
} KnobConfig_t;

/**
 * @brief 传感器数据包
 * @details 封装每个控制周期（1ms）的传感器读数，便于模块间传递。
 *          velocity 由控制层计算（当前角度 - 上一角度）。
 */
typedef struct
{
    float   angle;              ///< 当前角度 (°)，累积值可超过 360°
    float   velocity;           ///< 角速度 (°/ms)，1kHz 采样的角度差
    int32_t raw_count;          ///< 原始编码器计数（调试用）
} KnobSensorData_t;

/**
 * @brief 力输出指令
 * @details 模块计算出的电机控制指令，由控制层统一应用到硬件。
 *          支持 4 种方向：正转、反转、刹车、停止（自由）。
 */
typedef struct
{
    Knob_Motor_Dir_t direction; ///< 电机方向（FORWARD/REVERSE/BRAKE/STOP）
    uint8_t duty_pct;           ///< PWM 占空比 (0-100%)
} KnobForceOutput_t;

/**
 * @brief 旋钮工作状态枚举
 * @details 状态机的三种显式状态：
 *          - FREE: 正常转动，有卡位爬坡阻力
 *          - RETURNING: 松手后电机持续推向最近卡位
 *          - LIMIT_BOUNCE: 越过限位边界，弹簧推回
 */
typedef enum
{
    KNOB_STATE_FREE = 0,        ///< 自由模式（正常卡位手感）
    KNOB_STATE_RETURNING,       ///< 归中模式（松手拉向卡位中心）
    KNOB_STATE_LIMIT_BOUNCE,    ///< 限位弹跳（边界弹簧反馈）
} KnobState_t;

#endif
