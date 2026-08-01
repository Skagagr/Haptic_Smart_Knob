/**
 * @file    bsp_knob_buzzer.h
 * @brief   蜂鸣器驱动 — 有源蜂鸣器，GPIO 高低电平控制。
 * @details PB0 输出高低电平。有源蜂鸣器自带振荡电路，通电即响。
 *          Click() 触发约 15ms 脉冲，模拟机械卡位的喀嗒声。
 *          Tick() 需在 ISR 中以 1kHz 调用以管理脉冲计时。
 * @version 1.0.0
 * @date    2026/7/31
 */
#ifndef BSP_KNOB_BUZZER_H
#define BSP_KNOB_BUZZER_H

// 蜂鸣器引脚定义 (PB0)
#define KNOB_BUZZER_PORT    GPIOB
#define KNOB_BUZZER_PIN     GPIO_PIN_0

/**
 * @brief 初始化蜂鸣器 — PB0 推挽输出，默认低电平
 */
void BSP_KnobBuzzer_Init(void);

/**
 * @brief 触发一次短促蜂鸣 (~15ms)
 */
void BSP_KnobBuzzer_Click(void);

/**
 * @brief 脉冲计时管理，需在 1kHz ISR 中调用
 */
void BSP_KnobBuzzer_Tick(void);

#endif
