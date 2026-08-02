/**
 * @file    app_knob_physics.h
 * @brief   卡位物理模型 — 纯函数计算，无状态
 * @details 根据预设计算卡位参数，提供力计算和位置查询接口。
 *          本模块是无状态的纯函数库，所有参数在 Init 时一次性计算。
 * @version 2.0.0
 * @date    2026/8/2
 */
#ifndef APP_KNOB_PHYSICS_H
#define APP_KNOB_PHYSICS_H

#include "app_knob_types.h"

/**
 * @brief 初始化物理模型参数（根据预设计算）
 * @details 加载预设方案的基础参数（卡位数、半间距、死区、爬坡起点等），
 *          并根据 detent_strength 和 return_strength 计算实际力输出。
 *          所有派生参数在此函数中一次性计算，存储在模块内部静态变量。
 *
 * @param preset 预设方案（决定卡位数和基础手感）
 * @param detent_strength 卡位力度 (1-10)，影响爬坡阻力大小
 * @param return_strength 归中力度 (1-10)，影响松手归中速度
 *
 * @note 参数会自动限幅：preset 不合法时默认 DENSE_48，力度限制在 1-10 范围
 * @note 调用此函数后，所有查询和计算函数才能正常工作
 */
void KnobPhysics_Init(KnobPreset_t preset,
                      uint8_t detent_strength,
                      uint8_t return_strength);

/**
 * @brief 计算最近卡位中心
 * @details 根据当前角度，计算距离最近的卡位中心位置（四舍五入）。
 *          卡位中心是 detent_angle 的整数倍。
 *
 * @param angle 当前角度 (°)，可累积超过 360° 或为负数
 * @return 最近卡位中心的角度 (°)
 *
 * @note 无卡位模式（SMOOTH）下返回原始角度
 * @example angle=47° 时，12 卡位（间距 30°）返回 30°（第 1 个卡位）
 */
float KnobPhysics_FindNearestDetent(float angle);

/**
 * @brief 计算卡位爬坡力（接近中点时的逆向阻力）
 * @details 当旋钮接近卡位中点（翻过齿峰）时，施加逆向阻力模拟棘轮手感。
 *          阻力从 bump_start 位置开始线性爬升至 half_spacing。
 *
 * @param angle 当前角度 (°)
 * @param velocity 角速度 (°/ms)，用于判断转动方向
 * @return 力输出指令（方向与转动相反，占空比线性爬升）
 *
 * @note 只有在爬坡区且正在接近中点时才输出力
 * @note 已过中点或在死区/爬坡区外时返回 STOP + 0%
 */
KnobForceOutput_t KnobPhysics_CalcBumpForce(float angle, float velocity);

/**
 * @brief 计算归中力（松手后的拉向卡位力）
 * @details 当状态机进入 RETURNING 状态后，持续输出力推向最近卡位中心。
 *          力的大小与距中心的误差成正比，距离越远推力越大。
 *
 * @param angle 当前角度 (°)
 * @param velocity 角速度 (°/ms)，当前未使用，保留用于未来优化
 * @return 力输出指令（方向指向卡位中心，占空比与误差成正比）
 *
 * @note 占空比有地板值（12%），确保能推动齿轮箱
 * @note 误差超过半间距时按 100% 计算比例
 */
KnobForceOutput_t KnobPhysics_CalcReturnForce(float angle, float velocity);

/**
 * @brief 判断是否在死区内
 * @details 死区是卡位中心附近的小范围，在此范围内电机不输出任何力，
 *          让旋钮感觉"卡住"在卡位上，提供稳定的定位感。
 *
 * @param angle 当前角度 (°)
 * @return 1 = 在死区内，0 = 不在死区
 *
 * @note 死区大小由预设自动计算（通常为半间距的 13% 或最小 0.6°）
 * @note 无卡位模式下始终返回 1（相当于全局死区）
 */
int KnobPhysics_IsInDeadZone(float angle);

/**
 * @brief 获取当前卡位编号
 * @details 根据最近卡位中心计算卡位序号，用于蜂鸣器触发和调试输出。
 *
 * @param angle 当前角度 (°)
 * @return 卡位编号（整数，0 为初始位置，正数为正向，负数为反向）
 *
 * @note 无卡位模式下始终返回 0
 * @example 12 卡位模式，angle=90° 时返回 3（第 3 个卡位）
 */
int KnobPhysics_GetDetentIndex(float angle);

/**
 * @brief 判断是否在爬坡区
 * @details 爬坡区是从 bump_start 到 half_spacing 的区域，
 *          在此区域内如果正在接近中点，则施加逆向阻力。
 *
 * @param angle 当前角度 (°)
 * @return 1 = 在爬坡区，0 = 不在
 *
 * @note 只判断距离，不判断方向（方向由 IsApproaching 判断）
 */
int KnobPhysics_IsInBumpZone(float angle);

/**
 * @brief 判断是否正在接近中点（用于判断爬坡方向）
 * @details 通过比较误差方向和速度方向，判断是否正在"翻越齿峰"。
 *          只有正在接近中点时才施加爬坡阻力，已过中点时立即释放。
 *
 * @param angle 当前角度 (°)
 * @param velocity 角速度 (°/ms)
 * @return 1 = 正在接近，0 = 已过中点或远离
 *
 * @note error>0 表示目标在正前方，velocity>0 表示正向转动
 * @note 只有转动方向与误差方向一致时才算"接近"
 */
int KnobPhysics_IsApproaching(float angle, float velocity);

#endif
