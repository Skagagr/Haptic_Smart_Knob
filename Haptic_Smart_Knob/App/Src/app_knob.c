/**
 * @file    app_knob.c
 * @brief   旋钮应用层 — 控制循环、状态机、事件处理
 * @details 1kHz 控制循环，协调各模块完成以下功能：
 *          1. 编码器角度读取
 *          2. 卡位爬坡力反馈（棘轮手感）
 *          3. 松手自动归中
 *          4. 角度限位弹簧保护
 *          5. 蜂鸣器卡位提示
 *
 *          控制流程：读传感器 → 更新限位 → 状态机 → 应用输出 → 事件处理
 *          限位优先级高于卡位（边界保护）。
 * @version 3.0.0
 * @date    2026/8/3
 */
#include "app_knob.h"
#include "app_knob_physics.h"
#include "app_knob_limit.h"
#include "bsp_knob_encoder.h"
#include "bsp_knob_motor.h"
#include "bsp_knob_buzzer.h"
#include <stdio.h>

// 定时器句柄声明（Core 层定义）
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

// ===== 状态转换阈值 =====
#define STILL_THRESHOLD         0.18f  ///< 静止判定阈值 (°/ms)，须大于编码器噪声
#define STILL_COUNT_NEEDED      20     ///< 松手判定延迟 (ms)，连续静止此时长后切换到归中
#define REGRAB_VEL              0.05f  ///< 反向拧动检测阈值 (°/ms)，用于退出归中状态

// ===== 模块内部状态 =====
static KnobConfig_t     s_config;           ///< 当前旋钮配置
static float            s_last_angle;       ///< 上一 tick 角度，用于计算速度
static KnobSensorData_t s_current_sensor;   ///< 当前传感器数据
static KnobForceOutput_t s_current_output;  ///< 当前力输出指令
static int              s_current_detent;   ///< 当前卡位编号（限位激活时不更新）

// 状态机
static KnobState_t      s_current_state;    ///< 当前状态
static int              s_still_count;      ///< 连续静止计数器（用于判断松手）

// 事件检测
static int              s_last_detent;      ///< 上次卡位编号，用于检测切换

// ===== 调试用全局变量 =====
volatile int32_t raw_count;
volatile float   angle;

// ===== 私有函数声明 =====
static void HandleFreeState(const KnobSensorData_t *sensor, KnobForceOutput_t *output);
static void HandleReturningState(const KnobSensorData_t *sensor, KnobForceOutput_t *output);
static void HandleLimitBounceState(const KnobSensorData_t *sensor, KnobForceOutput_t *output);

// ===== 对外函数 =====

/**
 * @brief 初始化旋钮系统
 * @details 初始化 BSP 层、加载默认配置、初始化各子模块、
 *          读取初始编码器角度、启动 TIM3 1kHz 控制循环。
 */
void App_Knob_Init(void)
{
    // 初始化 BSP 层
    BSP_KnobEncoder_Init(&htim2);
    BSP_KnobMotor_Init(&htim4);

    // 默认配置：48 卡位，中等力度
    s_config.preset = KNOB_PRESET_NORMAL_12;
    s_config.detent_strength = 7;
    s_config.return_strength = 8;

    // 初始化各模块
    KnobPhysics_Init(s_config.preset,
                     s_config.detent_strength,
                     s_config.return_strength);
    KnobLimit_Init();
    BSP_KnobBuzzer_Init();

    // 初始化状态机
    s_current_state = KNOB_STATE_FREE;
    s_still_count = 0;
    s_last_detent = 0;

    // 初始化传感器状态
    s_last_angle = BSP_KnobEncoder_GetAngle();
    angle = s_last_angle;
    raw_count = BSP_KnobEncoder_GetRawCount();

    // 启动 1kHz 控制循环
    HAL_TIM_Base_Start_IT(&htim3);
}

/**
 * @brief 1kHz 控制循环（由 TIM3 ISR 调用）
 * @details 7 步控制流程：
 *          1. 读取传感器数据
 *          2. 更新限位（可能触发状态切换）
 *          3. 状态机计算力输出
 *          4. 限位激活时覆盖力输出（限位优先级最高）
 *          5. 应用电机输出
 *          6. 蜂鸣器脉冲计时
 *          7. 卡位变化检测 + 蜂鸣器触发（限位激活时跳过）
 */
void App_Knob_Control(void)
{
    // 1. 读取传感器
    s_current_sensor.raw_count = BSP_KnobEncoder_GetRawCount();
    s_current_sensor.angle = BSP_KnobEncoder_GetAngle();
    s_current_sensor.velocity = s_current_sensor.angle - s_last_angle;
    s_last_angle = s_current_sensor.angle;

    raw_count = s_current_sensor.raw_count;
    angle = s_current_sensor.angle;

    // 2. 更新限位（可能触发状态切换）
    KnobForceOutput_t limit_force;
    KnobLimit_Update(&s_current_sensor, &limit_force);

    // 3. 状态机计算力输出
    switch (s_current_state)
    {
        case KNOB_STATE_FREE:
            HandleFreeState(&s_current_sensor, &s_current_output);
            break;

        case KNOB_STATE_RETURNING:
            HandleReturningState(&s_current_sensor, &s_current_output);
            break;

        case KNOB_STATE_LIMIT_BOUNCE:
            HandleLimitBounceState(&s_current_sensor, &s_current_output);
            break;

        default:
            s_current_state = KNOB_STATE_FREE;
            s_current_output.direction = KNOB_MOTOR_DIR_STOP;
            s_current_output.duty_pct = 0;
            break;
    }

    // 4. 限位优先级高于卡位
    if (KnobLimit_IsActive())
    {
        s_current_output = limit_force;
    }

    // 5. 应用力输出
    BSP_KnobMotor_SetOutput(s_current_output.direction, s_current_output.duty_pct);

    // 6. 蜂鸣器脉冲计时（1kHz）
    BSP_KnobBuzzer_Tick();

    // 7. 卡位检测 + 蜂鸣器触发（限位激活时禁用，防止边界误触发）
    if (!KnobLimit_IsActive())
    {
        s_current_detent = KnobPhysics_GetDetentIndex(s_current_sensor.angle);
        if (s_current_detent != s_last_detent)  // 使蜂鸣器在同一个卡位点不会持续响
        {
            s_last_detent = s_current_detent;
            BSP_KnobBuzzer_Click();
        }
    }
}

/**
 * @brief 串口调试输出（由 main 循环每 1000ms 调用）
 */
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
           (int)s_current_state);
}

/**
 * @brief 运行时配置旋钮参数
 * @details 更新预设方案和力度参数，并重新初始化物理模型。
 * @param cfg 配置指针
 */
void App_Knob_SetConfig(const KnobConfig_t *cfg)
{
    s_config = *cfg;
    KnobPhysics_Init(s_config.preset,
                     s_config.detent_strength,
                     s_config.return_strength);
}

/**
 * @brief 读取当前配置
 * @param cfg 输出配置指针
 */
void App_Knob_GetConfig(KnobConfig_t *cfg)
{
    *cfg = s_config;
}

// ===== 限位回调（供 limit.c 调用） =====

/**
 * @brief 进入限位弹跳 — 触发蜂鸣并切换状态机
 */
void AppKnob_OnLimitEnter(void)
{
    s_current_state = KNOB_STATE_LIMIT_BOUNCE;
    s_still_count = 0;
    BSP_KnobBuzzer_Click();
}

/**
 * @brief 退出限位弹跳 — 切换回自由模式
 */
void AppKnob_OnLimitExit(void)
{
    s_current_state = KNOB_STATE_FREE;
    s_still_count = 0;
}

// ===== 状态处理函数 =====

/**
 * @brief 自由模式 — 正常转动时有卡位爬坡阻力
 * @details 1. 死区内不输出力（旋钮"卡"在卡位上）
 *          2. 连续静止 20ms → 切换到归中模式
 *          3. 未到爬坡区或已过中点 → 自由滑行
 *          4. 在爬坡区且接近中点 → 施加逆向阻力
 */
static void HandleFreeState(const KnobSensorData_t *sensor, KnobForceOutput_t *output)
{
    float abs_vel = (sensor->velocity >= 0.0f) ? sensor->velocity : -sensor->velocity;

    // 死区内 → 不输出力
    if (KnobPhysics_IsInDeadZone(sensor->angle))
    {
        s_still_count = 0;
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 静止检测 → 连续静止 STILL_COUNT_NEEDED ms 后切换到归中
    if (abs_vel < STILL_THRESHOLD)
    {
        s_still_count++;
        if (s_still_count >= STILL_COUNT_NEEDED)
        {
            s_current_state = KNOB_STATE_RETURNING;
            s_still_count = 0;
        }
    }
    else
    {
        s_still_count = 0;
    }

    // 未到爬坡区 → 自由滑行
    if (!KnobPhysics_IsInBumpZone(sensor->angle))
    {
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 已过中点（正滑向下一卡位）→ 自由
    if (!KnobPhysics_IsApproaching(sensor->angle, sensor->velocity))
    {
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 在爬坡区且接近中点 → 施加逆向阻力
    *output = KnobPhysics_CalcBumpForce(sensor->angle, sensor->velocity);
}

/**
 * @brief 归中模式 — 松手后电机推向最近卡位中心
 * @details 1. 进入死区 → 归中完成，切换回自由模式
 *          2. 检测反向拧动 → 人手接管，切换回自由模式
 *          3. 持续归中 → 电机推向卡位中心
 */
static void HandleReturningState(const KnobSensorData_t *sensor, KnobForceOutput_t *output)
{
    // 进入死区 → 归中完成
    if (KnobPhysics_IsInDeadZone(sensor->angle))
    {
        s_current_state = KNOB_STATE_FREE;
        s_still_count = 0;
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 反向拧动 → 人手接管
    float target = KnobPhysics_FindNearestDetent(sensor->angle);
    float error = target - sensor->angle;
    int moving_away = (error > 0.0f && sensor->velocity < -REGRAB_VEL)
                   || (error < 0.0f && sensor->velocity >  REGRAB_VEL);
    if (moving_away)
    {
        s_current_state = KNOB_STATE_FREE;
        s_still_count = 0;
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 持续归中
    *output = KnobPhysics_CalcReturnForce(sensor->angle, sensor->velocity);
}

/**
 * @brief 限位弹跳 — 完全由限位模块控制，状态机不输出力
 * @details 限位模块会在振荡稳定后调用 AppKnob_OnLimitExit 切换回 FREE。
 */
static void HandleLimitBounceState(const KnobSensorData_t *sensor, KnobForceOutput_t *output)
{
    output->direction = KNOB_MOTOR_DIR_STOP;
    output->duty_pct = 0;
}

// ===== ISR 回调 =====

/**
 * @brief HAL TIM 周期中断回调
 * @details TIM3 每 1ms 触发一次，驱动主控制循环。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        App_Knob_Control();
    }
}
