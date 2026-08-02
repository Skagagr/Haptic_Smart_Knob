/**
 * @file    app_knob_ctrl.c
 * @brief   旋钮控制协调层实现
 * @details 简化的控制循环，清晰的模块协调。
 *          控制流程：读传感器 → 更新限位 → 状态机 → 应用输出 → 事件处理
 *
 *          关键设计决策：
 *          - 限位优先级高于卡位（边界保护）
 *          - 限位激活时 Det# 和蜂鸣器同步冻结（避免超出边界时误触发）
 *          - 所有模块通过结构体传递数据（解耦）
 * @version 2.0.0
 * @date    2026/8/2
 */
#include "app_knob_ctrl.h"
#include "app_knob_physics.h"
#include "app_knob_state.h"
#include "app_knob_limit.h"
#include "app_knob_event.h"
#include "bsp_knob_encoder.h"
#include "bsp_knob_motor.h"
#include <stdio.h>

// 定时器句柄声明
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

// ===== 模块内部状态（只在 Init 时写入，运行时由 Control 更新） =====
static KnobConfig_t s_config;               ///< 当前旋钮配置
static float s_last_angle;                  ///< 上一 tick 角度，用于计算速度
static KnobSensorData_t s_current_sensor;   ///< 当前传感器数据（每 tick 更新）
static KnobForceOutput_t s_current_output;  ///< 当前力输出指令
static int s_current_detent;                ///< 当前卡位编号（限位激活时不更新）

// ===== 调试用全局变量（保持与原版兼容） =====
volatile int32_t raw_count;                 ///< 编码器原始计数值
volatile float   angle;                     ///< 当前角度

// ===== 限位事件回调声明 =====
static void OnLimitEnter(void);
static void OnLimitExit(void);

void App_Knob_Init(void)
{
    // 初始化 BSP 层
    BSP_KnobEncoder_Init(&htim2);
    BSP_KnobMotor_Init(&htim4);

    // 默认配置：48 卡位，中等力度
    s_config.preset = KNOB_PRESET_DENSE_48;
    s_config.detent_strength = 7;
    s_config.return_strength = 8;

    // 初始化各模块
    KnobPhysics_Init(s_config.preset,
                     s_config.detent_strength,
                     s_config.return_strength);
    KnobState_Init();
    KnobLimit_Init(OnLimitEnter, OnLimitExit);
    KnobEvent_Init();

    // 初始化传感器状态
    s_last_angle = BSP_KnobEncoder_GetAngle();
    angle = s_last_angle;
    raw_count = BSP_KnobEncoder_GetRawCount();

    // 启动控制循环
    HAL_TIM_Base_Start_IT(&htim3);
}

void App_Knob_Control(void)
{
    // 1. 读取传感器
    s_current_sensor.raw_count = BSP_KnobEncoder_GetRawCount();
    s_current_sensor.angle = BSP_KnobEncoder_GetAngle();
    s_current_sensor.velocity = s_current_sensor.angle - s_last_angle;
    s_last_angle = s_current_sensor.angle;

    // 更新调试变量
    raw_count = s_current_sensor.raw_count;
    angle = s_current_sensor.angle;

    // 2. 更新限位（可能触发状态切换）
    KnobForceOutput_t limit_force;
    KnobLimit_Update(&s_current_sensor, &limit_force);

    // 3. 状态机计算力输出
    KnobState_Update(&s_current_sensor, &s_current_output);

    // 4. 限位优先级高于卡位
    if (KnobLimit_IsActive())
    {
        s_current_output = limit_force;
    }

    // 5. 应用力输出
    BSP_KnobMotor_SetOutput(s_current_output.direction, s_current_output.duty_pct);

    // 6. 事件系统更新（蜂鸣器脉冲计时）
    KnobEvent_Tick();

    // 7. 卡位检测（限位激活时禁用，Det# 和蜂鸣器共用此限制逻辑）
    if (!KnobLimit_IsActive())
    {
        s_current_detent = KnobPhysics_GetDetentIndex(s_current_sensor.angle);
        KnobEvent_UpdateDetent(s_current_detent);
    }
}

void App_Knob_Debug(void)
{
    float target = KnobPhysics_FindNearestDetent(s_current_sensor.angle);
    float error = target - s_current_sensor.angle;

    printf("Angle: %8.2f  Target: %8.2f  Err: %+7.2f  "
           "Out: %+6.2f  Det#: %4d  State: %d\r\n",
           (double)s_current_sensor.angle,
           (double)target,
           (double)error,
           (double)s_current_output.duty_pct,
           s_current_detent,
           (int)KnobState_GetCurrent());
}

void App_Knob_SetConfig(const KnobConfig_t *cfg)
{
    s_config = *cfg;
    KnobPhysics_Init(s_config.preset,
                     s_config.detent_strength,
                     s_config.return_strength);
}

void App_Knob_GetConfig(KnobConfig_t *cfg)
{
    *cfg = s_config;
}

// ===== 限位事件回调 =====

static void OnLimitEnter(void)
{
    // 限位触发，切换状态机到弹跳模式
    KnobState_ForceSwitch(KNOB_STATE_LIMIT_BOUNCE);
    KnobEvent_Trigger(KNOB_EVENT_LIMIT_HIT);
}

static void OnLimitExit(void)
{
    // 限位稳定，切换回自由模式
    KnobState_ForceSwitch(KNOB_STATE_FREE);
}
