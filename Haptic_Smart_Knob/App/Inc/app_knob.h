/**
 * @file    app_knob.h
 * @brief   旋钮应用层 — 类型定义与对外接口
 * @details 包含所有模块间共享的数据结构、配置类型和对外 API。
 *          本文件是整个旋钮系统的入口头文件。
 * @version 3.0.0
 * @date    2026/8/3
 */
#ifndef APP_KNOB_H
#define APP_KNOB_H

#include <stdint.h>
#include "bsp_knob_motor.h"

/**
 * @brief 配置预设枚举
 * @details 每个预设对应一组经过调优的参数（卡位数、力度、死区等），
 *          用户只需选择预设，无需理解底层参数的含义。
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
 * @brief 运行时配置结构体
 * @details 只需 3 个参数即可调整整个旋钮手感：
 *          - preset: 选择预设方案，决定卡位数和基础参数
 *          - detent_strength: 卡位爬坡阻力 (1-10)，数值越大越难翻过卡位
 *          - return_strength: 松手归中力 (1-10)，数值越大归中速度越快
 */
typedef struct
{
    KnobPreset_t preset;        ///< 预设方案
    uint8_t detent_strength;    ///< 卡位力度 (1-10)
    uint8_t return_strength;    ///< 归中力度 (1-10)
} KnobConfig_t;

/**
 * @brief 传感器数据包
 * @details 封装每个控制周期（1ms）的传感器读数。
 */
typedef struct
{
    float   angle;              ///< 当前角度 (°)，累积值可超过 360°
    float   velocity;           ///< 角速度 (°/ms)，1kHz 采样的角度差
    int32_t raw_count;          ///< 原始编码器计数（调试用）
} KnobSensorData_t;

/**
 * @brief 力输出指令
 */
typedef struct
{
    Knob_Motor_Dir_t direction; ///< 电机方向（FORWARD/REVERSE/BRAKE/STOP）
    uint8_t duty_pct;           ///< PWM 占空比 (0-100%)
} KnobForceOutput_t;

/**
 * @brief 旋钮工作状态枚举
 * @details FREE: 正常转动，有卡位爬坡阻力
 *          RETURNING: 松手后电机持续推向最近卡位
 *          LIMIT_BOUNCE: 越过限位边界，弹簧推回
 */
typedef enum
{
    KNOB_STATE_FREE = 0,        ///< 自由模式（正常卡位手感）
    KNOB_STATE_RETURNING,       ///< 归中模式（松手拉向卡位中心）
    KNOB_STATE_LIMIT_BOUNCE,    ///< 限位弹跳（边界弹簧反馈）
} KnobState_t;

// ================================= 对外 API  =================================

void App_Knob_Init(void);
void App_Knob_Control(void);
void App_Knob_Debug(void);
void App_Knob_SetConfig(const KnobConfig_t *cfg);
void App_Knob_GetConfig(KnobConfig_t *cfg);

/**
 * @brief 设置蜂鸣器开关（0=关, 非0=开），默认开启
 * @details 由上位机 SET_BUZZER 命令控制；关闭时卡位/限位蜂鸣均静音
 */
void App_Knob_SetBuzzerEnabled(int enabled);

/**
 * @brief 查询蜂鸣器开关状态
 * @return 1=开启, 0=关闭
 */
int App_Knob_IsBuzzerEnabled(void);

/**
 * @brief 限位模块回调 — 进入限位弹跳时调用
 * @details 由 KnobLimit_Update 在检测到越界时调用。
 *          触发蜂鸣器并切换状态机到 LIMIT_BOUNCE。
 */
void AppKnob_OnLimitEnter(void);

/**
 * @brief 限位模块回调 — 退出限位弹跳时调用
 * @details 由 KnobLimit_Update 在振荡稳定后调用。
 *          切换状态机回 FREE。
 */
void AppKnob_OnLimitExit(void);

// ============================== 调试用全局变量  ==============================
extern volatile int32_t raw_count;
extern volatile float   angle;

#endif
