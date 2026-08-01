/**
 * @file    app_knob.c
 * @brief   力反馈旋钮 — EC11 棘轮式虚拟卡位实现。
 * @details 基于中点位能峰的虚拟卡位:
 *          - 卡位之间: 电机自由 (AIN1=AIN2=0)，拧动手感如同未通电
 *          - 接近中点:  逆向阻力线性爬升，模拟翻越棘轮齿峰
 *          - 翻过中点:  阻力释放，自由滑入下一个卡位
 *          - 松手检测:  连续静止后触发归中力拉向最近卡位
 *          所有角度阈值按半间距比例自动缩放，限位由 app_limit 模块处理。
 * @version 1.0.0
 * @date    2026/7/31
 */
#include "app_knob.h"
#include <stdio.h>
#include "app_limit.h"
#include "bsp_knob_encoder.h"
#include "bsp_knob_motor.h"
#include "bsp_knob_buzzer.h"
#include "tim.h"

/**
 * ============================== 可调参数说明 ==============================
 *
 * 只需修改 KNOB_DEFAULT_NUM_DETENTS 一个宏。所有派生参数（力峰值、死区、
 * 爬坡起点）在 Init 中自动根据半间距缩放，无需手动对照调整。
 *
 * --- 卡位 ---
 *
 * KNOB_DEFAULT_NUM_DETENTS   每圈卡位数 (2 ~ 90)，0 = 禁用卡位
 *
 * KNOB_REF_BUMP_MAX_PCT      爬坡阻力基准值 (对应 12 卡位/半间距 15°)
 *                            实际值 = 基准 × (半间距/15°)，14% 地板
 *
 * KNOB_REF_RETURN_FORCE_PCT  归中力基准值 (对应 12 卡位/半间距 15°)
 *                            实际值 = 基准 × (半间距/15°)，12% 地板
 *
 * KNOB_VEL_THRESHOLD         转动判定阈值 (°/ms)，须 > 编码器噪声 0.13
 * KNOB_STILL_THRESHOLD       静止判定阈值 (°/ms)，须 > 编码器噪声
 * KNOB_STILL_COUNT_NEEDED    松手判定延迟 (ms)
 *
 * KNOB_DEAD_ZONE_RATIO       死区比例 + 最小 0.6° 地板 (5 个编码器 count)
 * KNOB_BUMP_START_RATIO      爬坡起点比例 + 最小 0.8° 爬坡宽度
 * KNOB_RETURN_FORCE_FLOOR    归中力地板 (% 占空比)
 * KNOB_REGRAB_VEL            反向拧动检测阈值 (°/ms)
 *
 * --- 自动缩放示例 ---
 *
 *   NUM_DETENTS | 半间距 | bump_max | return | 死区  | 爬坡起点
 *   ----------- | ------ | -------- | ------ | ----- | --------
 *          6    |  30°   |   20%    |  22%   | 3.9°  | 21.0°
 *         12    |  15°   |   20%    |  22%   | 2.0°  | 10.5°
 *         24    | 7.5°   |   15%    |  16%   | 1.0°  |  5.3°
 *         48    | 3.75°  |   12%    |  14%   | 0.6°  |  3.0°
 *
 * --- 限位 ---
 *
 * KNOB_LIMIT_DEFAULT_MODE    OFF / SINGLE / DUAL
 * KNOB_LIMIT_DEFAULT_MIN     下界 (°)，仅 DUAL 生效
 * KNOB_LIMIT_DEFAULT_MAX     上界 (°)，SINGLE/DUAL 生效
 * KNOB_LIMIT_SPRING_KP       限位弹簧刚度 (%/°)
 * KNOB_LIMIT_SPRING_KD       限位阻尼系数
 * KNOB_LIMIT_MAX_FORCE_PCT   限位力上限 (% 占空比)
 *
 * --- app_limit.c 额外参数 ---
 *
 * LIMIT_FORCE_FLOOR / LIMIT_SETTLE_ANGLE / LIMIT_SETTLE_VEL / LIMIT_SETTLE_MS
 *
 * ======================================================================
 */

// ===== 卡位可调参数 =====
// 以下为 12 卡位（半间距 15°）的基准值，Init 中会根据实际半间距自动缩放。
// 改 KNOB_DEFAULT_NUM_DETENTS 即可，力参数和角度阈值自动适配。

#define KNOB_DEFAULT_NUM_DETENTS   48     // 每圈卡位数 (2~90)，0 = 禁用卡位

#define KNOB_REF_BUMP_MAX_PCT      20     // 爬坡阻力基准值 (% 占空比)
#define KNOB_REF_RETURN_FORCE_PCT  22     // 归中力基准值 (% 占空比)
#define KNOB_VEL_THRESHOLD         0.25f  // 转动判定阈值 (°/ms)，须 > 编码器噪声 0.13
#define KNOB_STILL_THRESHOLD       0.18f  // 静止判定阈值 (°/ms)，须 > 编码器噪声
#define KNOB_STILL_COUNT_NEEDED    20     // 松手判定延迟 (ms)

#define KNOB_DEAD_ZONE_RATIO       0.13f  // 死区比例，自动缩放 + 最小 0.6° 地板
#define KNOB_BUMP_START_RATIO      0.70f  // 爬坡起点比例，自动缩放 + 最小 0.8° 爬坡宽度
#define KNOB_RETURN_FORCE_FLOOR    14.0f  // 归中力地板 (% 占空比)
#define KNOB_REGRAB_VEL            0.05f  // 反向拧动检测阈值 (°/ms)

// ===== 限位默认值 =====
#define KNOB_LIMIT_DEFAULT_MODE    KNOB_LIMIT_MODE_DUAL  // OFF / SINGLE / DUAL
#define KNOB_LIMIT_DEFAULT_MIN     -180.0f  // 下界 (°)，仅 DUAL 生效
#define KNOB_LIMIT_DEFAULT_MAX     360.0f  // 上界 (°)，SINGLE/DUAL 生效
#define KNOB_LIMIT_SPRING_KP       4.0f    // 限位弹簧刚度 (%/°)，越大回弹越猛
#define KNOB_LIMIT_SPRING_KD       1.5f    // 限位阻尼，越大振荡衰减越快
#define KNOB_LIMIT_MAX_FORCE_PCT   55      // 限位力上限 (% 占空比)

/**
 * @brief 旋钮行为状态
 */
typedef enum {
    STATE_FREE,       // 自由模式: 人手转动 → 自由滑行 + 中点爬坡
    STATE_RETURNING,  // 归中模式: 松手 → 电机持续推向最近卡位
} KnobState_t;

// ===== 模块内部状态 =====
static Knob_DetentConfig_t s_detent_cfg; // 卡位配置
static KnobState_t s_state;              // 当前状态
static int    s_still_count;             // 连续静止计数器
static float  s_half_spacing;            // 半间距 = detent_angle / 2
static float  s_dead_zone;               // 死区角度 (自动缩放)
static float  s_bump_start;              // 爬坡起始角度 (自动缩放)
static float  s_bump_max_pct;            // 爬坡阻力峰值 (自动缩放)
static float  s_return_force_pct;        // 归中力峰值 (自动缩放)
static float  s_last_angle;              // 上一 tick 角度，用于计算速度
static float  s_current_angle;           // 当前角度
static float  s_target_angle;            // 目标角度 (最近卡位中心)
static float  s_pid_output;              // 当前输出 (调试用，历史命名保留)
static int    s_last_detent_idx;          // 上一 tick 的卡位编号，用于检测切换

// ===== 调试用全局变量 =====
volatile int32_t raw_count;  // 编码器原始计数值
volatile float   angle;      // 当前角度

/**
 * @brief 找到距离当前角度最近的卡位中心
 * @param a 当前角度 (°)，可累积超过 360°
 * @return 最近卡位中心的角度
 */
static float FindNearestDetent(float a)
{
    int32_t idx;
    if (a >= 0.0f) {
        idx = (int32_t)(a / s_detent_cfg.detent_angle + 0.5f);
    } else {
        idx = (int32_t)(a / s_detent_cfg.detent_angle - 0.5f);
    }
    return (float)idx * s_detent_cfg.detent_angle;
}

/**
 * @brief 根据当前半间距重新计算所有派生参数，NUM_DETENTS 改变时自动适配。
 * @details 力参数按半间距比例缩放（相对于 12 卡位 15° 基准），
 *          角度参数保证最小绝对值防止死区/爬坡区过窄。
 */
static void RecalcDetentParams(void)
{
    s_half_spacing = s_detent_cfg.detent_angle * 0.5f;

    // 力参数: 与半间距成比例缩放，上限为基准值
    float scale = s_half_spacing / 15.0f;  // 以 12 卡位为基准
    if (scale > 1.0f) scale = 1.0f;

    s_bump_max_pct = KNOB_REF_BUMP_MAX_PCT * scale;
    if (s_bump_max_pct < 12.0f) s_bump_max_pct = 12.0f;

    s_return_force_pct = KNOB_REF_RETURN_FORCE_PCT * scale;
    if (s_return_force_pct < 14.0f) s_return_force_pct = 14.0f;

    // 死区: 比例缩放 + 最小 0.6° (约 5 个编码器 count)
    s_dead_zone = s_half_spacing * KNOB_DEAD_ZONE_RATIO;
    if (s_dead_zone < 0.6f) s_dead_zone = 0.6f;
    s_detent_cfg.dead_zone_deg = s_dead_zone;

    // 爬坡起点: 保证爬坡区宽度至少 0.8° (约 6 个 count)
    float bump_width = s_half_spacing * (1.0f - KNOB_BUMP_START_RATIO);
    if (bump_width < 0.8f) {
        s_bump_start = s_half_spacing - 0.8f;
        if (s_bump_start < s_dead_zone + 0.3f)
            s_bump_start = s_dead_zone + 0.3f;
    } else {
        s_bump_start = s_half_spacing * KNOB_BUMP_START_RATIO;
    }
}

/**
 * @brief 初始化旋钮 — 编码器、电机、卡位、限位，启动 TIM3 1kHz 中断
 */
void App_Knob_Init(void)
{
    BSP_KnobEncoder_Init(&htim2);
    BSP_KnobMotor_Init(&htim4);
    BSP_KnobBuzzer_Init();

    // 卡位默认参数
    s_detent_cfg.num_detents    = KNOB_DEFAULT_NUM_DETENTS;
    s_detent_cfg.detent_angle   = 360.0f / (float)KNOB_DEFAULT_NUM_DETENTS;
    s_detent_cfg.window_deg     = 15.0f;
    s_detent_cfg.max_torque_pct = 50;

    // 派生阈值: 全部基于半间距自动缩放
    RecalcDetentParams();

    // 限位模块初始化
    Knob_LimitConfig_t lim = {
        .mode          = KNOB_LIMIT_DEFAULT_MODE,
        .limit_min_deg = KNOB_LIMIT_DEFAULT_MIN,
        .limit_max_deg = KNOB_LIMIT_DEFAULT_MAX,
        .spring_kp     = KNOB_LIMIT_SPRING_KP,
        .spring_kd     = KNOB_LIMIT_SPRING_KD,
        .max_force_pct = KNOB_LIMIT_MAX_FORCE_PCT,
    };
    App_Limit_Init(&lim);

    s_last_angle  = BSP_KnobEncoder_GetAngle();
    angle         = s_last_angle;
    s_state           = STATE_FREE;
    s_still_count     = 0;
    s_pid_output      = 0.0f;
    s_last_detent_idx = 0;

    HAL_TIM_Base_Start_IT(&htim3);
}

/**
 * @brief 控制循环 — TIM3 ISR 中以 1kHz 频率调用
 * @details 执行顺序:
 *          1. 读取编码器角度
 *          2. 找到最近卡位中心 (setpoint)
 *          3. 计算角速度
 *          4. 限位检查 (委托 app_limit，触发则跳过后续)
 *          5. 卡位禁用 → 自由模式
 *          6. 死区内 → 电机停止
 *          7. 归中状态 → 持续推向卡位中心
 *          8. 静止检测 → 达标后切换归中
 *          9. 自由区 → 电机停止 (大部分行程)
 *         10. 已过中点 → 自由滑入下一卡位
 *         11. 爬坡 → 逆向阻力线性增长
 */
void App_Knob_Control(void)
{
    // 1. 读取传感器
    raw_count = BSP_KnobEncoder_GetRawCount();
    s_current_angle = BSP_KnobEncoder_GetAngle();
    angle = s_current_angle;

    // 2. 确定最近卡位中心。error > 0 表示目标在正前方（角度增大方向）
    s_target_angle = FindNearestDetent(s_current_angle);
    float error   = s_target_angle - s_current_angle;
    float abs_err = (error >= 0.0f) ? error : -error;

    // 3. 计算角速度 (°/ms，相邻 1kHz tick 的角度差)
    float velocity = s_current_angle - s_last_angle;
    s_last_angle   = s_current_angle;
    float abs_vel  = (velocity >= 0.0f) ? velocity : -velocity;

    // 4. 限位检查（委托 app_limit 模块，触发则跳过卡位逻辑）
    if (App_Limit_Check(s_current_angle, velocity)) {
        // 限位弹跳中 — 用限位边界截断的卡位号检测切换，避免边界振荡误触
        float clamped = s_current_angle;
        Knob_LimitConfig_t lim;
        App_Limit_GetConfig(&lim);
        int check_upper = (lim.mode == KNOB_LIMIT_MODE_SINGLE || lim.mode == KNOB_LIMIT_MODE_DUAL);
        int check_lower = (lim.mode == KNOB_LIMIT_MODE_DUAL);
        if (check_upper && clamped > lim.limit_max_deg) clamped = lim.limit_max_deg;
        if (check_lower && clamped < lim.limit_min_deg) clamped = lim.limit_min_deg;
        int detent_idx = (int)(FindNearestDetent(clamped) / s_detent_cfg.detent_angle);

        BSP_KnobBuzzer_Tick();
        if (detent_idx != s_last_detent_idx) {
            s_last_detent_idx = detent_idx;
            BSP_KnobBuzzer_Click();
        }
        return;
    }

    // 蜂鸣器脉冲计时 + 卡位切换检测（正常路径）
    BSP_KnobBuzzer_Tick();
    {
        int det_idx = (int)(s_target_angle / s_detent_cfg.detent_angle);
        if (det_idx != s_last_detent_idx) {
            s_last_detent_idx = det_idx;
            BSP_KnobBuzzer_Click();
        }
    }

    // 5. 卡位已禁用 → 完全自由
    if (s_detent_cfg.num_detents == 0) {
        s_pid_output = 0.0f;
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_STOP, 0);
        return;
    }

    // 6. 位于死区 → 电机不输出，自由
    if (abs_err < s_dead_zone) {
        s_state = STATE_FREE;
        s_still_count = 0;
        s_pid_output = 0.0f;
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_STOP, 0);
        return;
    }

    // 7. 归中状态: 电机持续推向卡位中心
    if (s_state == STATE_RETURNING) {
        // 人手反向拧动 → 退出归中
        int moving_away = (error > 0.0f && velocity < -KNOB_REGRAB_VEL)
                       || (error < 0.0f && velocity >  KNOB_REGRAB_VEL);
        if (moving_away) {
            s_state = STATE_FREE;
            s_still_count = 0;
        } else {
            // 归中力 = 距中心比例 × 最大归中力，14% 地板防推不动齿轮箱
            float ratio = abs_err / s_half_spacing;
            if (ratio > 1.0f) ratio = 1.0f;
            s_pid_output = s_return_force_pct * ratio;
            if (s_pid_output < KNOB_RETURN_FORCE_FLOOR) s_pid_output = KNOB_RETURN_FORCE_FLOOR;
            if (error > 0.0f) {
                BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_FORWARD, (uint8_t)s_pid_output);
            } else {
                BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_REVERSE, (uint8_t)s_pid_output);
            }
            return;
        }
    }

    // 8. 静止检测: 连续静止 N ms → 切换为归中状态
    if (abs_vel > KNOB_VEL_THRESHOLD) {
        s_state = STATE_FREE;
        s_still_count = 0;
    } else if (abs_vel < KNOB_STILL_THRESHOLD) {
        s_still_count++;
    } else {
        s_still_count = 0;
    }
    if (s_still_count >= KNOB_STILL_COUNT_NEEDED)
        s_state = STATE_RETURNING;

    // 9. 自由模式 + 未到爬坡区 → 完全自由
    if (abs_err < s_bump_start) {
        s_pid_output = 0.0f;
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_STOP, 0);
        return;
    }

    // 10. 自由模式 + 已过中点（正滑向下一卡位）→ 自由
    int approaching = (velocity > 0.0f) ? (error < 0.0f) : (error > 0.0f);
    if (!approaching) {
        s_pid_output = 0.0f;
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_STOP, 0);
        return;
    }

    // 11. 爬坡: 逆向阻力从 0 线性爬升至 s_bump_max_pct
    float ramp_range = s_half_spacing - s_bump_start;
    float ramp = (abs_err - s_bump_start) / ramp_range;
    if (ramp > 1.0f) ramp = 1.0f;
    s_pid_output = s_bump_max_pct * ramp;

    // 阻力方向与转动方向相反
    if (velocity > 0.0f) {
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_REVERSE, (uint8_t)s_pid_output);
    } else {
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_FORWARD, (uint8_t)s_pid_output);
    }
}

/**
 * @brief 串口调试输出，每秒一次。限位开启时 Det# 截断到限位边界内
 */
void App_Knob_Debug(void)
{
    float display_angle  = s_current_angle;
    float display_target = s_target_angle;

    // 若限位已开启，将显示角度截断到限位范围，防止 Det# 溢出
    Knob_LimitConfig_t lim;
    App_Limit_GetConfig(&lim);
    if (lim.mode != KNOB_LIMIT_MODE_OFF) {
        int check_upper = (lim.mode == KNOB_LIMIT_MODE_SINGLE ||
                           lim.mode == KNOB_LIMIT_MODE_DUAL);
        int check_lower = (lim.mode == KNOB_LIMIT_MODE_DUAL);
        if (check_upper && display_angle > lim.limit_max_deg)
            display_angle = lim.limit_max_deg;
        if (check_lower && display_angle < lim.limit_min_deg)
            display_angle = lim.limit_min_deg;
        display_target = FindNearestDetent(display_angle);
    }

    printf("Angle: %8.2f  Target: %8.2f  Err: %+7.2f  "
           "Out: %+6.2f  Det#: %4.0f\r\n",
           s_current_angle,
           display_target,
           display_target - s_current_angle,
           s_pid_output,
           display_target / s_detent_cfg.detent_angle);
}

/**
 * @brief 运行时设置卡位配置
 * @details 自动从 num_detents 重算 detent_angle，无需调用者手动维护
 * @param cfg 卡位配置指针
 */
void App_Knob_SetDetentConfig(const Knob_DetentConfig_t *cfg)
{
    s_detent_cfg = *cfg;
    if (s_detent_cfg.num_detents > 0)
        s_detent_cfg.detent_angle = 360.0f / (float)s_detent_cfg.num_detents;
    RecalcDetentParams();
}

/**
 * @brief 读取当前卡位配置
 * @param cfg 输出配置指针
 */
void App_Knob_GetDetentConfig(Knob_DetentConfig_t *cfg)
{
    *cfg = s_detent_cfg;
}

/**
 * @brief TIM3 周期中断回调，1kHz。HAL 弱符号覆盖
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        App_Knob_Control();
    }
}
