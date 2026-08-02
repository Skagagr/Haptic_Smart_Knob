/**
 * @file    app_knob_state.c
 * @brief   旋钮状态机实现
 * @details 显式状态转换，每个状态对应独立处理函数。
 *          状态机职责：根据传感器数据判断状态转换，调用物理模型计算力输出。
 * @version 2.0.0
 * @date    2026/8/2
 */
#include "app_knob_state.h"
#include "app_knob_physics.h"

// ===== 状态转换阈值（与原版保持一致） =====
#define STILL_THRESHOLD         0.18f  ///< 静止判定阈值 (°/ms)，须大于编码器噪声
#define STILL_COUNT_NEEDED      20     ///< 松手判定延迟 (ms)，连续静止此时长后切换到归中
#define REGRAB_VEL              0.05f  ///< 反向拧动检测阈值 (°/ms)，用于退出归中状态

// ===== 模块内部状态（只在 Init 时写入，运行时只读） =====
static KnobState_t s_current_state;         ///< 当前状态
static int s_still_count;                   ///< 连续静止计数器（用于判断松手）

// ===== 私有函数声明 =====
/**
 * @brief 处理自由模式状态
 * @details 自由模式下的逻辑：
 *          1. 位于死区 → 不输出力
 *          2. 检测静止 → 累计静止计数，达标后切换到归中
 *          3. 未到爬坡区 或 已过中点 → 自由滑行
 *          4. 在爬坡区且接近中点 → 施加逆向阻力
 * @param sensor 传感器数据
 * @param output 输出力指令
 */
static void HandleFreeState(const KnobSensorData_t *sensor, KnobForceOutput_t *output);

/**
 * @brief 处理归中模式状态
 * @details 归中模式下的逻辑：
 *          1. 进入死区 → 切换回自由模式
 *          2. 检测反向拧动 → 切换回自由模式
 *          3. 其他情况 → 持续推向卡位中心
 * @param sensor 传感器数据
 * @param output 输出力指令
 */
static void HandleReturningState(const KnobSensorData_t *sensor, KnobForceOutput_t *output);

/**
 * @brief 处理限位弹跳状态
 * @details 限位弹跳时，状态机不输出任何力，完全由限位模块控制。
 *          限位模块会在振荡稳定后通过 ForceSwitch 切换回 FREE。
 * @param sensor 传感器数据
 * @param output 输出力指令（固定为 STOP + 0%）
 */
static void HandleLimitBounceState(const KnobSensorData_t *sensor, KnobForceOutput_t *output);

void KnobState_Init(void)
{
    s_current_state = KNOB_STATE_FREE;
    s_still_count = 0;
}

KnobState_t KnobState_Update(const KnobSensorData_t *sensor,
                              KnobForceOutput_t *output)
{
    // 显式状态分支（相比旧版隐式 if 更清晰）
    switch (s_current_state)
    {
        case KNOB_STATE_FREE:
            HandleFreeState(sensor, output);
            break;

        case KNOB_STATE_RETURNING:
            HandleReturningState(sensor, output);
            break;

        case KNOB_STATE_LIMIT_BOUNCE:
            HandleLimitBounceState(sensor, output);
            break;

        default:
            // 异常状态，恢复到自由模式
            s_current_state = KNOB_STATE_FREE;
            output->direction = KNOB_MOTOR_DIR_STOP;
            output->duty_pct = 0;
            break;
    }

    return s_current_state;
}

void KnobState_ForceSwitch(KnobState_t new_state)
{
    s_current_state = new_state;
    s_still_count = 0;  // 清零静止计数，防止状态抖动
}

KnobState_t KnobState_GetCurrent(void)
{
    return s_current_state;
}

// ===== 私有函数实现 =====

static void HandleFreeState(const KnobSensorData_t *sensor, KnobForceOutput_t *output)
{
    float abs_vel = (sensor->velocity >= 0.0f) ? sensor->velocity : -sensor->velocity;

    // 1. 位于死区 → 电机不输出，旋钮"卡"在卡位上
    if (KnobPhysics_IsInDeadZone(sensor->angle))
    {
        s_still_count = 0;
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 2. 静止检测 → 连续静止 STILL_COUNT_NEEDED ms 后切换到归中
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
        s_still_count = 0;  // 有转动，清零计数
    }

    // 3. 未到爬坡区 → 自由滑行
    if (!KnobPhysics_IsInBumpZone(sensor->angle))
    {
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 4. 已过中点（正滑向下一卡位）→ 自由
    if (!KnobPhysics_IsApproaching(sensor->angle, sensor->velocity))
    {
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 5. 在爬坡区且接近中点 → 施加逆向阻力
    *output = KnobPhysics_CalcBumpForce(sensor->angle, sensor->velocity);
}

static void HandleReturningState(const KnobSensorData_t *sensor, KnobForceOutput_t *output)
{
    // 1. 进入死区 → 归中完成，切换回自由模式
    if (KnobPhysics_IsInDeadZone(sensor->angle))
    {
        s_current_state = KNOB_STATE_FREE;
        s_still_count = 0;
        output->direction = KNOB_MOTOR_DIR_STOP;
        output->duty_pct = 0;
        return;
    }

    // 2. 检测反向拧动 → 人手接管，切换回自由模式
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

    // 3. 持续归中 → 电机推向卡位中心
    *output = KnobPhysics_CalcReturnForce(sensor->angle, sensor->velocity);
}

static void HandleLimitBounceState(const KnobSensorData_t *sensor, KnobForceOutput_t *output)
{
    // 限位弹跳状态由限位模块完全控制
    // 状态机在此状态下不输出任何力
    // 限位模块会在振荡稳定后通过 KnobState_ForceSwitch 切换回 FREE
    output->direction = KNOB_MOTOR_DIR_STOP;
    output->duty_pct = 0;
}
