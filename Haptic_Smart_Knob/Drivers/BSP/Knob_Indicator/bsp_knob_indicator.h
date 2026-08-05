/**
 * @file    bsp_knob_indicator.h
 * @brief   指示灯 + 按键驱动 — LED 输出控制、按键扫描（消抖 + 边沿检测）
 * @details 指示灯：PA2=空闲 / PA3=控制音量 / PA4=控制亮度（低电平点亮）
 *          按键：  PB12=回空闲 / PB13=切到下一状态（按下=低电平，上拉输入）
 *          BSP 层只关心"硬件是什么"，不关心按键对应什么业务逻辑——
 *          状态枚举与业务逻辑在 App 层（app_mode.h）定义。
 * @version 1.0.0
 * @date    2026/8/5
 */
#ifndef BSP_KNOB_INDICATOR_H
#define BSP_KNOB_INDICATOR_H

#include <stdint.h>
#include "app_mode.h"

// ============================ 指示灯引脚定义  ============================
#define KNOB_LED_IDLE_PORT      GPIOA
#define KNOB_LED_IDLE_PIN       GPIO_PIN_2
#define KNOB_LED_VOLUME_PORT    GPIOA
#define KNOB_LED_VOLUME_PIN     GPIO_PIN_3
#define KNOB_LED_BRIGHTNESS_PORT GPIOA
#define KNOB_LED_BRIGHTNESS_PIN GPIO_PIN_4

// ============================ 按键引脚定义  ============================
#define KNOB_BTN_BACK_PORT      GPIOB
#define KNOB_BTN_BACK_PIN       GPIO_PIN_12   // 回空闲
#define KNOB_BTN_NEXT_PORT      GPIOB
#define KNOB_BTN_NEXT_PIN       GPIO_PIN_13   // 切到下一状态

/**
 * @brief 按键事件类型
 * @details BSP 层只上报"哪个按键被按下"，不解释含义；
 *          由 App 层决定按键对应的业务动作。
 */
typedef enum
{
    KNOB_BTN_EVENT_NONE = 0,   // 无按键事件
    KNOB_BTN_EVENT_BACK,       // PB12 按下（回空闲）
    KNOB_BTN_EVENT_NEXT,       // PB13 按下（切下一状态）
} KnobBtnEvent_t;

/**
 * @brief 初始化指示灯 + 按键（引脚已在 CubeMX 配置）
 * @details 熄灭所有 LED；清零按键扫描状态
 */
void BSP_KnobIndicator_Init(void);

/**
 * @brief 点亮指定控制模式对应的 LED，熄灭其余
 * @param mode 控制模式（KNOB_STATE_MODE_IDLE/VOLUME/BRIGHTNESS）
 */
void BSP_KnobIndicator_SetMode(KnobStateMode_t mode);

/**
 * @brief 扫描按键，返回"刚刚被按下"的事件（内部消抖 + 边沿检测）
 * @note  非阻塞，需在主循环中每次循环调用
 * @return 检测到的事件；无按键动作返回 KNOB_BTN_EVENT_NONE
 */
KnobBtnEvent_t BSP_KnobButton_Scan(void);

#endif
