/**
 * @file    app_knob_state.h
 * @brief   旋钮状态机 — 显式状态转换
 * @details 管理 FREE/RETURNING/LIMIT_BOUNCE 三种状态的转换。
 *          相比旧版隐式 if 分支，新版使用显式 switch 分支，易于维护和扩展。
 * @version 2.0.0
 * @date    2026/8/2
 */
#ifndef APP_KNOB_STATE_H
#define APP_KNOB_STATE_H

#include "app_knob_types.h"

/**
 * @brief 初始化状态机
 * @details 设置初始状态为 FREE，清零静止计数器。
 *          在系统启动时由 App_Knob_Init 调用一次。
 */
void KnobState_Init(void);

/**
 * @brief 状态机更新（每 tick 调用一次）
 * @details 根据当前状态和传感器数据，执行相应的状态处理逻辑，
 *          并填充输出力指令。状态转换条件：
 *          - FREE → RETURNING: 连续静止 20ms
 *          - FREE → LIMIT_BOUNCE: 限位模块通过 ForceSwitch 触发
 *          - RETURNING → FREE: 进入死区 或 反向拧动
 *          - LIMIT_BOUNCE → FREE: 限位模块通过 ForceSwitch 触发
 *
 * @param sensor 传感器数据（角度、速度、原始计数）
 * @param output 输出力指令（由状态机填充）
 * @return 当前状态（用于调试和日志）
 *
 * @note 在 1kHz ISR 中调用，执行时间需尽量短
 * @note 限位优先级高于状态机，控制层会覆盖状态机输出
 */
KnobState_t KnobState_Update(const KnobSensorData_t *sensor,
                              KnobForceOutput_t *output);

/**
 * @brief 强制切换状态（用于限位模块触发）
 * @details 限位模块通过事件回调调用此函数，强制切换到/退出弹跳模式。
 *          这是限位模块与状态机通信的唯一接口（事件驱动）。
 *
 * @param new_state 新状态（通常为 LIMIT_BOUNCE 或 FREE）
 *
 * @note 切换时会清零静止计数器，防止状态抖动
 */
void KnobState_ForceSwitch(KnobState_t new_state);

/**
 * @brief 获取当前状态
 * @details 用于调试输出和外部查询。
 *
 * @return 当前状态枚举值
 */
KnobState_t KnobState_GetCurrent(void);

#endif
