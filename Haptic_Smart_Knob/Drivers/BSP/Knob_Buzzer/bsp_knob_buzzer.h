/**
 * @file    bsp_knob_buzzer.h
 * @brief   蜂鸣器驱动 — 有源蜂鸣器，低电平触发。
 * @details PB0 在 CubeMX 已配置 Output PP。Init() 仅拉高静音。
 *          Click() 触发 5ms 脉冲 + 50ms 冷却防连响。
 *          Tick() 需在 1kHz ISR 中调用以管理脉冲计时。
 * @version 1.0.0
 * @date    2026/7/31
 */
#ifndef BSP_KNOB_BUZZER_H
#define BSP_KNOB_BUZZER_H

// 蜂鸣器引脚定义 (PB0)
#define KNOB_BUZZER_PORT    GPIOB
#define KNOB_BUZZER_PIN     GPIO_PIN_0

/**
 * @brief 初始化蜂鸣器 — 拉高 PB0 静音，清零脉冲/冷却计数
 */
void BSP_KnobBuzzer_Init(void);

/**
 * @brief 触发一次短促蜂鸣 (5ms) + 50ms 冷却防连响
 */
void BSP_KnobBuzzer_Click(void);

/**
 * @brief 脉冲计时管理，需在 1kHz ISR 中调用
 */
void BSP_KnobBuzzer_Tick(void);

#endif
