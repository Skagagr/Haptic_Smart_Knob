/**
 * @file    app_knob_physics.c
 * @brief   卡位物理模型实现
 * @details 预设方案硬编码，避免复杂的自动缩放算法。
 *          所有参数在 Init 时一次性计算，运行时只做查表和简单计算。
 * @version 2.0.0
 * @date    2026/8/2
 */
#include "app_knob_physics.h"

// ===== 速度阈值（与原版保持一致） =====
#define VEL_THRESHOLD       0.25f  ///< 转动判定阈值 (°/ms)
#define REGRAB_VEL          0.05f  ///< 反向拧动检测阈值 (°/ms)

/**
 * @brief 预设方案参数结构体
 * @details 封装一组完整的卡位参数，每个预设对应一个实例
 */
typedef struct
{
    uint8_t num_detents;        ///< 每圈卡位数
    float   detent_angle;       ///< 卡位间距 (°)
    float   half_spacing;       ///< 半间距 (°) = detent_angle / 2
    float   dead_zone;          ///< 死区 (°)，卡位中心附近不输出力
    float   bump_start;         ///< 爬坡起点 (°)，从此处开始施加阻力
    float   bump_max_base;      ///< 基础爬坡力 (%)，实际值 = base × strength
    float   return_force_base;  ///< 基础归中力 (%)，实际值 = base × strength
} PresetParams_t;

/**
 * @brief 预设方案参数表（硬编码，经过调优）
 * @details 每个预设的参数都经过实际测试调优，确保手感一致性。
 *          参数设计原则：
 *          - 卡位越少，半间距越大，力度越强（防止过度平滑）
 *          - 卡位越多，半间距越小，力度越弱（防止卡顿）
 *          - 死区约为半间距的 13%，最小 0.6°（约 5 个编码器 count）
 *          - 爬坡起点约为半间距的 70%，保证爬坡区宽度至少 0.8°
 */
static const PresetParams_t s_preset_table[] =
{
    // KNOB_PRESET_COARSE_6: 6 卡位/圈，粗糙手感
    {6,  60.0f, 30.0f, 3.9f, 21.0f, 22.0f, 24.0f},
    // KNOB_PRESET_NORMAL_12: 12 卡位/圈，标准手感
    {12, 30.0f, 15.0f, 2.0f, 10.5f, 20.0f, 22.0f},
    // KNOB_PRESET_FINE_24: 24 卡位/圈，精细手感
    {24, 15.0f, 7.5f,  1.0f, 5.3f,  16.0f, 18.0f},
    // KNOB_PRESET_DENSE_48: 48 卡位/圈，密集手感
    {48, 7.5f,  3.75f, 0.6f, 2.6f,  12.0f, 14.0f},
    // KNOB_PRESET_SMOOTH: 无卡位，完全平滑
    {0,  0.0f,  0.0f,  0.0f, 0.0f,  0.0f,  0.0f},
};

// ===== 模块内部状态（只在 Init 时写入，运行时只读） =====
static PresetParams_t s_params;             ///< 当前预设参数
static float s_bump_max_pct;                ///< 实际爬坡力 = base × mult
static float s_return_force_pct;            ///< 实际归中力 = base × mult
static float s_return_force_floor;          ///< 归中力地板（确保能推动齿轮箱）

void KnobPhysics_Init(KnobPreset_t preset,
                      uint8_t detent_strength,
                      uint8_t return_strength)
{
    // 参数限幅（防御性编程）
    if (preset > KNOB_PRESET_SMOOTH)
        preset = KNOB_PRESET_DENSE_48;
    if (detent_strength < 1) detent_strength = 1;
    if (detent_strength > 10) detent_strength = 10;
    if (return_strength < 1) return_strength = 1;
    if (return_strength > 10) return_strength = 10;

    // 加载预设参数
    s_params = s_preset_table[preset];

    // 计算力度乘数 (1-10 映射到 0.1-1.0)，仅 Init 期间使用
    float detent_mult = (float)detent_strength / 10.0f;
    float return_mult = (float)return_strength / 10.0f;

    // 计算实际力参数
    s_bump_max_pct = s_params.bump_max_base * detent_mult;
    s_return_force_pct = s_params.return_force_base * return_mult;

    // 归中力地板 = 基础值的 70%，确保能推动齿轮箱
    s_return_force_floor = s_params.return_force_base * 0.7f;
    if (s_return_force_floor < 12.0f)
        s_return_force_floor = 12.0f;
}

float KnobPhysics_FindNearestDetent(float angle)
{
    // 无卡位模式，返回原始角度
    if (s_params.num_detents == 0)
        return angle;

    // 四舍五入到最近的卡位中心
    int32_t idx;
    if (angle >= 0.0f)
    {
        idx = (int32_t)(angle / s_params.detent_angle + 0.5f);
    }
    else
    {
        idx = (int32_t)(angle / s_params.detent_angle - 0.5f);
    }
    return (float)idx * s_params.detent_angle;
}

int KnobPhysics_IsInDeadZone(float angle)
{
    // 无卡位模式，始终在"死区"（全局自由）
    if (s_params.num_detents == 0)
        return 1;

    float target = KnobPhysics_FindNearestDetent(angle);
    float error = target - angle;
    float abs_err = (error >= 0.0f) ? error : -error;

    return (abs_err < s_params.dead_zone) ? 1 : 0;
}

int KnobPhysics_IsInBumpZone(float angle)
{
    if (s_params.num_detents == 0)
        return 0;

    float target = KnobPhysics_FindNearestDetent(angle);
    float error = target - angle;
    float abs_err = (error >= 0.0f) ? error : -error;

    return (abs_err >= s_params.bump_start) ? 1 : 0;
}

int KnobPhysics_IsApproaching(float angle, float velocity)
{
    if (s_params.num_detents == 0)
        return 0;

    float target = KnobPhysics_FindNearestDetent(angle);
    float error = target - angle;

    // error > 0 表示目标在正前方，velocity > 0 表示正向转动
    // 只有当转动方向与误差方向一致时，才算"接近中点"
    int approaching = (velocity > 0.0f) ? (error > 0.0f) : (error < 0.0f);
    return approaching;
}

KnobForceOutput_t KnobPhysics_CalcBumpForce(float angle, float velocity)
{
    KnobForceOutput_t output = {KNOB_MOTOR_DIR_STOP, 0};

    if (s_params.num_detents == 0)
        return output;

    float target = KnobPhysics_FindNearestDetent(angle);
    float error = target - angle;
    float abs_err = (error >= 0.0f) ? error : -error;

    // 爬坡力：从 bump_start 到 half_spacing 线性爬升
    // ramp = 0 在 bump_start 位置，ramp = 1 在 half_spacing 位置
    float ramp_range = s_params.half_spacing - s_params.bump_start;
    float ramp = (abs_err - s_params.bump_start) / ramp_range;
    if (ramp > 1.0f) ramp = 1.0f;
    if (ramp < 0.0f) ramp = 0.0f;

    output.duty_pct = (uint8_t)(s_bump_max_pct * ramp);

    // 阻力方向与转动方向相反（模拟翻越齿峰的阻力）
    if (velocity > 0.0f)
    {
        output.direction = KNOB_MOTOR_DIR_REVERSE;
    }
    else
    {
        output.direction = KNOB_MOTOR_DIR_FORWARD;
    }

    return output;
}

KnobForceOutput_t KnobPhysics_CalcReturnForce(float angle, float velocity)
{
    KnobForceOutput_t output = {KNOB_MOTOR_DIR_STOP, 0};

    if (s_params.num_detents == 0)
        return output;

    float target = KnobPhysics_FindNearestDetent(angle);
    float error = target - angle;
    float abs_err = (error >= 0.0f) ? error : -error;

    // 归中力 = 距中心比例 × 最大归中力
    // 距离越远，推力越大（线性关系）
    float ratio = abs_err / s_params.half_spacing;
    if (ratio > 1.0f) ratio = 1.0f;

    float force = s_return_force_pct * ratio;
    // 应用地板值，确保能推动齿轮箱
    if (force < s_return_force_floor)
        force = s_return_force_floor;

    output.duty_pct = (uint8_t)force;

    // 推力方向指向卡位中心
    if (error > 0.0f)
    {
        output.direction = KNOB_MOTOR_DIR_FORWARD;
    }
    else
    {
        output.direction = KNOB_MOTOR_DIR_REVERSE;
    }

    return output;
}

int KnobPhysics_GetDetentIndex(float angle)
{
    if (s_params.num_detents == 0)
        return 0;

    float target = KnobPhysics_FindNearestDetent(angle);
    return (int)(target / s_params.detent_angle);
}
