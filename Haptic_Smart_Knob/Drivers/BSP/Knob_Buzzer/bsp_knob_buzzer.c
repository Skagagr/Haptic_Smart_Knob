/**
 * @file    bsp_knob_buzzer.c
 * @brief   蜂鸣器驱动实现。
 * @details 有源蜂鸣器低电平触发。Click() 拉低 5ms 后拉高。
 *          冷却间隔 50ms 防止密集卡位切换时听起来像长鸣。
 *          Tick() 需在 1kHz ISR 中调用以管理脉冲计时。
 * @version 1.1.0
 * @date    2026/7/31
 */
#include "bsp_knob_buzzer.h"
#include "main.h"

#define BUZZER_ON_MS     5    // 蜂鸣脉冲宽度 (ms)
#define BUZZER_COOLDOWN  50   // 两次触发最小间隔 (ms)

static int s_pulse_remaining;  // 剩余发声时间 (ms)
static int s_cooldown;         // 剩余冷却时间 (ms)

void BSP_KnobBuzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin   = KNOB_BUZZER_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KNOB_BUZZER_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(KNOB_BUZZER_PORT, KNOB_BUZZER_PIN, GPIO_PIN_SET);  // 低电平触发, 默认高=静音
    s_pulse_remaining = 0;
    s_cooldown        = 0;
}

void BSP_KnobBuzzer_Click(void)
{
    // 正在发声 或 冷却未结束 → 跳过
    if (s_pulse_remaining > 0 || s_cooldown > 0) return;
    HAL_GPIO_WritePin(KNOB_BUZZER_PORT, KNOB_BUZZER_PIN, GPIO_PIN_RESET);  // 拉低→发声
    s_pulse_remaining = BUZZER_ON_MS;
    s_cooldown        = BUZZER_COOLDOWN;
}

void BSP_KnobBuzzer_Tick(void)
{
    if (s_pulse_remaining > 0) {
        s_pulse_remaining--;
        if (s_pulse_remaining == 0) {
            HAL_GPIO_WritePin(KNOB_BUZZER_PORT, KNOB_BUZZER_PIN, GPIO_PIN_SET);  // 拉高→静音
        }
    }
    if (s_cooldown > 0) {
        s_cooldown--;
    }
}
