/**
 * @file    app_limit.c
 * @brief   角度限位实现 — 双向弹跳弹簧 + 速度阻尼。
 * @details 越过边界后以限位点为锚点，两侧均有拉力形成来回振荡。
 *          阻尼项削减高速运动时的推力，使振荡快速衰减。
 *          静止阈值内连续 50ms 后自动退出弹跳。
 * @version 1.0.0
 * @date    2026/7/31
 */
#include "app_limit.h"
#include "bsp_knob_motor.h"

// ===== 可调参数 =====
#define LIMIT_FORCE_FLOOR   20.0f  // 地板力，确保能推动齿轮箱
#define LIMIT_SETTLE_ANGLE  3.0f   // 进入此角度范围视为"已稳定"
#define LIMIT_SETTLE_VEL    0.3f   // 低于此速度视为"已静止"
#define LIMIT_SETTLE_MS     50     // 连续稳定 ms 数，达标后退出弹跳

// ===== 模块内部状态 =====
static Knob_LimitConfig_t s_cfg;        // 当前限位配置
static int    s_bounce_active;           // 1 = 弹跳模式激活中
static float  s_bounce_target;           // 弹簧锚点角度（限位边界值）
static int    s_settle_count;            // 连续稳定计数器

/**
 * @brief 初始化限位模块
 * @param cfg 限位配置指针
 */
void App_Limit_Init(const Knob_LimitConfig_t *cfg)
{
    s_cfg = *cfg;
    s_bounce_active = 0;
    s_settle_count  = 0;
}

/**
 * @brief 运行时更新限位配置（会重置弹跳状态）
 * @param cfg 限位配置指针
 */
void App_Limit_SetConfig(const Knob_LimitConfig_t *cfg)
{
    s_cfg = *cfg;
    s_bounce_active = 0;  // 配置变更时退出弹跳，避免锚点错乱
    s_settle_count  = 0;
}

/**
 * @brief 读取当前限位配置
 * @param cfg 输出配置指针
 */
void App_Limit_GetConfig(Knob_LimitConfig_t *cfg)
{
    *cfg = s_cfg;
}

/**
 * @brief 执行限位检查
 * @param angle    当前旋钮角度 (°)
 * @param velocity 角速度 (°/ms)
 * @return 1 = 限位弹簧占用本次 tick，0 = 角度在界内
 */
int App_Limit_Check(float angle, float velocity)
{
    // 限位关闭 → 不做任何处理
    if (s_cfg.mode == KNOB_LIMIT_MODE_OFF)
        return 0;

    float abs_vel = (velocity >= 0.0f) ? velocity : -velocity;
    int check_upper = (s_cfg.mode == KNOB_LIMIT_MODE_SINGLE ||
                       s_cfg.mode == KNOB_LIMIT_MODE_DUAL);
    int check_lower = (s_cfg.mode == KNOB_LIMIT_MODE_DUAL);

    // 越界时进入弹跳模式，记录锚点
    if (!s_bounce_active) {
        if (check_upper && angle > s_cfg.limit_max_deg) {
            s_bounce_active  = 1;
            s_bounce_target  = s_cfg.limit_max_deg;
            s_settle_count   = 0;
        } else if (check_lower && angle < s_cfg.limit_min_deg) {
            s_bounce_active  = 1;
            s_bounce_target  = s_cfg.limit_min_deg;
            s_settle_count   = 0;
        }
    }

    if (!s_bounce_active)
        return 0;

    // 双向弹簧 + 阻尼: force = |误差|*Kp - |速度|*Kd
    float err     = s_bounce_target - angle;
    float abs_err = (err >= 0.0f) ? err : -err;
    float spring  = abs_err * s_cfg.spring_kp;
    float damp    = abs_vel * s_cfg.spring_kd;
    float force   = spring - damp;  // 阻尼始终与运动方向相反，抑制过冲
    if (force < LIMIT_FORCE_FLOOR) force = LIMIT_FORCE_FLOOR;
    if (force > (float)s_cfg.max_force_pct) force = (float)s_cfg.max_force_pct;

    // 推力方向总是指向锚点
    if (err > 0.0f) {
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_FORWARD, (uint8_t)force);
    } else {
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_REVERSE, (uint8_t)force);
    }

    // 检测振荡是否已稳定，连续达标后退出弹跳
    if (abs_err < LIMIT_SETTLE_ANGLE && abs_vel < LIMIT_SETTLE_VEL) {
        s_settle_count++;
        if (s_settle_count >= LIMIT_SETTLE_MS) {
            s_bounce_active = 0;
        }
    } else {
        s_settle_count = 0;
    }

    return 1;
}
