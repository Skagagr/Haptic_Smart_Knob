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
#include "tim.h"

/**
 * ============================== 可调参数说明 ==============================
 *
 * 只需修改本区块的 #define 即可调整手感，无需改动逻辑代码。
 * 所有角度阈值（死区、爬坡起点）按半间距比例自动缩放，改卡位数无需
 * 逐一手动调整其他值。
 *
 * --- 卡位 ---
 *
 * KNOB_DEFAULT_NUM_DETENTS   每圈卡位数 (2 ~ 90)
 *                            值越大卡位越密。设为 0 禁用卡位。
 *                            示例: 12 = 每30°一个, 24 = 每15°一个
 *
 * KNOB_DEAD_ZONE_RATIO       死区占半间距比例 (0.0 ~ 0.5)
 *                            死区内电机不输出，旋钮可自由停靠。
 *                            值越大卡位中心越"宽松"，越小越"紧"。
 *                            示例: 12 卡位时 0.13 → 死区 ±2.0°
 *
 * KNOB_BUMP_START_RATIO      爬坡起点占半间距比例 (0.0 ~ 1.0)
 *                            旋钮到达此比例位置后开始感受到逆向阻力，
 *                            之前完全自由。值越大自由区越宽、爬坡越陡。
 *                            示例: 12 卡位时 0.70 → 前 10.5° 自由
 *
 * KNOB_BUMP_MAX_PCT          爬坡阻力最大值，0 ~ 100 (% 占空比)
 *                            越接近中点阻力越大，在中点达到该峰值。
 *                            值过大则拧动费力，过小则齿感不明显。
 *
 * KNOB_VEL_THRESHOLD         速度阈值 (°/ms)，高于此值视为人手正在转动。
 *                            必须大于编码器噪声 (约 0.13°/ms)。
 *                            值越大需要拧得越快才触发 bump。
 *
 * KNOB_STILL_THRESHOLD       静止阈值 (°/ms)，低于此值视为已松手。
 *                            必须大于编码器噪声。值越大越容易触发归中。
 *
 * KNOB_STILL_COUNT_NEEDED    连续静止多少毫秒后触发归中力。
 *                            值越小松手响应越快，但可能误触发。
 *
 * KNOB_RETURN_FORCE_PCT      归中力最大值，0 ~ 100 (% 占空比)
 *                            实际出力按偏离距离比例缩放，14% 地板。
 *                            值越大归中越快，但可能过冲振荡。
 *
 * --- 限位 ---
 *
 * KNOB_LIMIT_DEFAULT_MODE    限位模式:
 *                              KNOB_LIMIT_MODE_OFF    = 关闭 (无限旋转)
 *                              KNOB_LIMIT_MODE_SINGLE = 单边上限
 *                              KNOB_LIMIT_MODE_DUAL   = 双边上下限
 *
 * KNOB_LIMIT_DEFAULT_MIN     下界角度 (°)，仅 DUAL 模式生效
 * KNOB_LIMIT_DEFAULT_MAX     上界角度 (°)，SINGLE 和 DUAL 均生效
 *
 *                             示例:
 *                               DUAL + [-180, 180] = ±180° 范围
 *                               DUAL + [-360, 360] = ±360° 范围
 *                               DUAL + [0, 360]    = 0~360° 范围
 *                               SINGLE + max=360   = 不能超过 360°
 *                               SINGLE + max=0     = 不能超过 0° (只能负向)
 *                               OFF                = 无限旋转
 *
 * KNOB_LIMIT_SPRING_KP       限位弹簧刚度 (% 占空比每度偏差)
 *                            值越大限位越"硬"，回弹越快。
 *
 * KNOB_LIMIT_SPRING_KD       限位阻尼系数
 *                            抑制弹簧振荡过冲，值越大振荡衰减越快。
 *
 * KNOB_LIMIT_MAX_FORCE_PCT   限位弹簧最大力，0 ~ 100 (% 占空比)
 *
 * --- app_limit.c 中的额外参数 ---
 *
 * LIMIT_FORCE_FLOOR          限位地板力 (默认 20%)
 * LIMIT_SETTLE_ANGLE         稳定判定角度 (默认 3°)
 * LIMIT_SETTLE_VEL           稳定判定速度 (默认 0.3°/ms)
 * LIMIT_SETTLE_MS            连续稳定 ms 后退出弹跳 (默认 50)
 *
 * --- 其他 ---
 *
 * KNOB_RETURN_FORCE_FLOOR    归中力地板 (% 占空比)，确保克服齿轮箱静摩擦
 * KNOB_REGRAB_VEL            反向拧动检测阈值 (°/ms)
 *
 * ======================================================================
 */

// ===== 卡位可调参数 =====
#define KNOB_DEFAULT_NUM_DETENTS   24     // 每圈卡位数 (2~90)，0 = 禁用卡位。建议最高48，再高就感觉不出来了

#define KNOB_BUMP_MAX_PCT          25     // 爬坡阻力峰值 (% 占空比)，越大齿感越重
#define KNOB_RETURN_FORCE_PCT      44     // 归中力峰值 (% 占空比)，越大归中越快
#define KNOB_VEL_THRESHOLD         0.25f  // 转动判定阈值 (°/ms)，须 > 编码器噪声 0.13
#define KNOB_STILL_THRESHOLD       0.18f  // 静止判定阈值 (°/ms)，须 > 编码器噪声
#define KNOB_STILL_COUNT_NEEDED    20     // 松手判定延迟 (ms)，越小响应越快

#define KNOB_DEAD_ZONE_RATIO       0.13f  // 死区比例 (0~0.5)，越大卡位中心越宽松
#define KNOB_BUMP_START_RATIO      0.70f  // 爬坡起点比例 (0~1)，越大自由区越宽
#define KNOB_RETURN_FORCE_FLOOR    14.0f  // 归中力地板 (% 占空比)，确保克服齿轮箱静摩擦
#define KNOB_REGRAB_VEL            0.05f  // 反向拧动检测阈值 (°/ms)，小于编码器噪声一半

// ===== 限位默认值 =====
#define KNOB_LIMIT_DEFAULT_MODE    KNOB_LIMIT_MODE_DUAL  // OFF / SINGLE / DUAL
#define KNOB_LIMIT_DEFAULT_MIN     0.0f  // 下界 (°)，仅 DUAL 生效
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
static float  s_last_angle;              // 上一 tick 角度，用于计算速度
static float  s_current_angle;           // 当前角度
static float  s_target_angle;            // 目标角度 (最近卡位中心)
static float  s_pid_output;              // 当前输出 (调试用，历史命名保留)

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
 * @brief 初始化旋钮 — 编码器、电机、卡位、限位，启动 TIM3 1kHz 中断
 */
void App_Knob_Init(void)
{
    BSP_KnobEncoder_Init(&htim2);
    BSP_KnobMotor_Init(&htim4);

    // 卡位默认参数
    s_detent_cfg.num_detents    = KNOB_DEFAULT_NUM_DETENTS;
    s_detent_cfg.detent_angle   = 360.0f / (float)KNOB_DEFAULT_NUM_DETENTS;
    s_detent_cfg.window_deg     = 15.0f;
    s_detent_cfg.max_torque_pct = 50;

    // 派生阈值: 全部基于半间距自动缩放
    s_half_spacing = s_detent_cfg.detent_angle * 0.5f;
    s_detent_cfg.dead_zone_deg = s_half_spacing * KNOB_DEAD_ZONE_RATIO;
    s_dead_zone  = s_detent_cfg.dead_zone_deg;
    s_bump_start = s_half_spacing * KNOB_BUMP_START_RATIO;

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
    s_state       = STATE_FREE;
    s_still_count = 0;
    s_pid_output  = 0.0f;

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
    if (App_Limit_Check(s_current_angle, velocity))
        return;

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
            s_pid_output = (float)KNOB_RETURN_FORCE_PCT * ratio;
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

    // 11. 爬坡: 逆向阻力从 0 线性爬升至 KNOB_BUMP_MAX_PCT
    float ramp_range = s_half_spacing - s_bump_start;
    float ramp = (abs_err - s_bump_start) / ramp_range;
    if (ramp > 1.0f) ramp = 1.0f;
    s_pid_output = (float)KNOB_BUMP_MAX_PCT * ramp;

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
    s_half_spacing = s_detent_cfg.detent_angle * 0.5f;
    s_detent_cfg.dead_zone_deg = s_half_spacing * KNOB_DEAD_ZONE_RATIO;
    s_dead_zone  = s_detent_cfg.dead_zone_deg;
    s_bump_start = s_half_spacing * KNOB_BUMP_START_RATIO;
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
