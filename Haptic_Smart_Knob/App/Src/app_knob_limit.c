/**
 * @file    app_knob_limit.c
 * @brief   角度限位实现 — 双向弹簧 + 阻尼，振荡稳定检测
 * @details 检测到越界后进入弹跳模式，通过弹簧力将旋钮推回边界内。
 *          振荡稳定后自动退出弹跳，恢复正常工作模式。
 *          限位优先级高于卡位力反馈。
 * @version 3.0.0
 * @date    2026/8/3
 */
#include "app_knob_limit.h"
#include "bsp_knob_motor.h"

// ================================= 可调参数  =================================
#define LIMIT_FORCE_FLOOR   20.0f  ///< 限位地板力（确保能推动齿轮箱）
#define LIMIT_SETTLE_ANGLE  3.0f   ///< 稳定判定角度 (°)
#define LIMIT_SETTLE_VEL    0.3f   ///< 稳定判定速度 (°/ms)
#define LIMIT_SETTLE_MS     50     ///< 连续稳定 ms 后退出弹跳
#define LIMIT_DEBOUNCE_MS   100    ///< 蜂鸣器去抖时间 (ms)，防止边界振荡时重复触发

// =============================== 模块内部状态  ===============================
static Knob_LimitConfig_t s_cfg;            ///< 当前限位配置
static int s_bounce_active;                 ///< 1 = 弹跳模式激活中
static float s_bounce_target;               ///< 弹簧锚点角度（限位边界值）
static int s_settle_count;                  ///< 连续稳定计数器
static int s_debounce_count;                ///< 蜂鸣器去抖计数器（防止重复触发）

void KnobLimit_Init(void)
{
    s_cfg.mode = KNOB_LIMIT_DEFAULT_MODE;
    s_cfg.limit_min_deg = KNOB_LIMIT_DEFAULT_MIN;
    s_cfg.limit_max_deg = KNOB_LIMIT_DEFAULT_MAX;
    s_cfg.spring_kp = KNOB_LIMIT_SPRING_KP;
    s_cfg.spring_kd = KNOB_LIMIT_SPRING_KD;
    s_cfg.max_force_pct = KNOB_LIMIT_MAX_FORCE_PCT;

    s_bounce_active = 0;
    s_settle_count = 0;
    s_debounce_count = 0;
}

void KnobLimit_SetConfig(const Knob_LimitConfig_t *cfg)
{
    s_cfg = *cfg;
    s_bounce_active = 0;
    s_settle_count = 0;
}

void KnobLimit_GetConfig(Knob_LimitConfig_t *cfg)
{
    *cfg = s_cfg;
}

int KnobLimit_IsActive(void)
{
    return s_bounce_active;
}

void KnobLimit_Update(const KnobSensorData_t *sensor,
                      KnobForceOutput_t *output)
{
    output->direction = KNOB_MOTOR_DIR_STOP;
    output->duty_pct = 0;

    // 限位关闭
    if (s_cfg.mode == KNOB_LIMIT_MODE_OFF)
    {
        if (s_bounce_active)
        {
            s_bounce_active = 0;
            s_debounce_count = 0;
            AppKnob_OnLimitExit();
        }
        return;
    }

    float abs_vel = (sensor->velocity >= 0.0f) ? sensor->velocity : -sensor->velocity;
    int check_upper = (s_cfg.mode == KNOB_LIMIT_MODE_SINGLE ||
                       s_cfg.mode == KNOB_LIMIT_MODE_DUAL);
    int check_lower = (s_cfg.mode == KNOB_LIMIT_MODE_DUAL);

    // 越界检测，进入弹跳模式
    if (!s_bounce_active)
    {
        if (check_upper && sensor->angle > s_cfg.limit_max_deg)
        {
            s_bounce_active = 1;
            s_bounce_target = s_cfg.limit_max_deg;
            s_settle_count = 0;
            if (s_debounce_count == 0)
            {
                AppKnob_OnLimitEnter();
                s_debounce_count = LIMIT_DEBOUNCE_MS;
            }
        }
        else if (check_lower && sensor->angle < s_cfg.limit_min_deg)
        {
            s_bounce_active = 1;
            s_bounce_target = s_cfg.limit_min_deg;
            s_settle_count = 0;
            if (s_debounce_count == 0)
            {
                AppKnob_OnLimitEnter();
                s_debounce_count = LIMIT_DEBOUNCE_MS;
            }
        }
    }

    // 去抖计数器递减（每 1ms 减 1）
    if (s_debounce_count > 0)
    {
        s_debounce_count--;
    }

    if (!s_bounce_active)
        return;

    // 双向弹簧 + 阻尼
    float err = s_bounce_target - sensor->angle;
    float abs_err = (err >= 0.0f) ? err : -err;
    float spring = abs_err * s_cfg.spring_kp;
    float damp = abs_vel * s_cfg.spring_kd;
    float force = spring - damp;

    if (force < LIMIT_FORCE_FLOOR)
        force = LIMIT_FORCE_FLOOR;
    if (force > (float)s_cfg.max_force_pct)
        force = (float)s_cfg.max_force_pct;

    output->duty_pct = (uint8_t)force;

    // 推力方向指向锚点
    if (err > 0.0f)
    {
        output->direction = KNOB_MOTOR_DIR_FORWARD;
    }
    else
    {
        output->direction = KNOB_MOTOR_DIR_REVERSE;
    }

    // 振荡稳定检测
    if (abs_err < LIMIT_SETTLE_ANGLE && abs_vel < LIMIT_SETTLE_VEL)
    {
        s_settle_count++;
        if (s_settle_count >= LIMIT_SETTLE_MS)
        {
            s_bounce_active = 0;
            s_settle_count = 0;
            AppKnob_OnLimitExit();
        }
    }
    else
    {
        s_settle_count = 0;
    }
}
