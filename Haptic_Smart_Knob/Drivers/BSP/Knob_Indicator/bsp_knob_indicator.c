/**
 * @file    bsp_knob_indicator.c
 * @brief   指示灯 + 按键驱动实现 — LED 输出控制、按键扫描（消抖 + 边沿检测）
 * @details 按键扫描逻辑与参考项目 Key_Scan 一致：
 *          - 20ms 限流扫描，跳过机械抖动窗口
 *          - 边沿检测：仅在"电平从高变低"瞬间上报一次按下事件，
 *            按住不放不会重复触发，松手再按才再触发
 * @version 1.0.0
 * @date    2026/8/5
 */
#include "bsp_knob_indicator.h"
#include "main.h"

// ============================ 按键扫描参数  ============================
#define KNOB_BTN_SCAN_INTERVAL  20   // 两次扫描最小间隔 (ms)，避开机械抖动

// =============================== 模块内部状态  ===============================
static uint32_t s_last_tick;                 // 上次扫描时间戳
static GPIO_PinState s_last_back;            // PB12 上次电平（边沿检测）
static GPIO_PinState s_last_next;            // PB13 上次电平（边沿检测）

// ================================= 对外函数  =================================

void BSP_KnobIndicator_Init(void)
{
    // 熄灭所有 LED（低电平点亮，拉高熄灭；引脚已在 CubeMX 配置为输出）
    HAL_GPIO_WritePin(KNOB_LED_IDLE_PORT, KNOB_LED_IDLE_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KNOB_LED_VOLUME_PORT, KNOB_LED_VOLUME_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KNOB_LED_BRIGHTNESS_PORT, KNOB_LED_BRIGHTNESS_PIN, GPIO_PIN_SET);

    // 按键初始状态：假设开机时都未按下（上拉输入，未按下=高电平）
    s_last_tick = 0;
    s_last_back = GPIO_PIN_SET;
    s_last_next = GPIO_PIN_SET;
}

void BSP_KnobIndicator_SetMode(KnobStateMode_t mode)
{
    // 先全部熄灭（拉高），再点亮当前模式对应的 LED（拉低）
    HAL_GPIO_WritePin(KNOB_LED_IDLE_PORT, KNOB_LED_IDLE_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KNOB_LED_VOLUME_PORT, KNOB_LED_VOLUME_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KNOB_LED_BRIGHTNESS_PORT, KNOB_LED_BRIGHTNESS_PIN, GPIO_PIN_SET);

    switch (mode)
    {
        case KNOB_STATE_MODE_IDLE:
            HAL_GPIO_WritePin(KNOB_LED_IDLE_PORT, KNOB_LED_IDLE_PIN, GPIO_PIN_RESET);
            break;
        case KNOB_STATE_MODE_VOLUME:
            HAL_GPIO_WritePin(KNOB_LED_VOLUME_PORT, KNOB_LED_VOLUME_PIN, GPIO_PIN_RESET);
            break;
        case KNOB_STATE_MODE_BRIGHTNESS:
            HAL_GPIO_WritePin(KNOB_LED_BRIGHTNESS_PORT, KNOB_LED_BRIGHTNESS_PIN, GPIO_PIN_RESET);
            break;
        default:
            break;
    }
}

/**
 * @brief 按键边沿检测：电平从"未按下(SET)"变为"按下(RESET)"时上报一次
 */
static KnobBtnEvent_t CheckButtonEdge(GPIO_TypeDef *port, uint16_t pin,
                                      GPIO_PinState *last_state, KnobBtnEvent_t event)
{
    GPIO_PinState cur = HAL_GPIO_ReadPin(port, pin);

    // 先更新"上一次状态"，再判断是否触发（保证按住不重复触发）
    if (cur == GPIO_PIN_RESET && *last_state == GPIO_PIN_SET)
    {
        *last_state = cur;
        return event;
    }
    *last_state = cur;
    return KNOB_BTN_EVENT_NONE;
}

KnobBtnEvent_t BSP_KnobButton_Scan(void)
{
    uint32_t now_tick = HAL_GetTick();

    // 限流：没到 20ms 直接返回无事件（跳过抖动窗口）
    if (now_tick - s_last_tick < KNOB_BTN_SCAN_INTERVAL)
    {
        return KNOB_BTN_EVENT_NONE;
    }
    s_last_tick = now_tick;

    // 依次检查两个按键（回空闲 / 切下一状态）
    KnobBtnEvent_t ev = CheckButtonEdge(KNOB_BTN_BACK_PORT, KNOB_BTN_BACK_PIN,
                                        &s_last_back, KNOB_BTN_EVENT_BACK);
    if (ev != KNOB_BTN_EVENT_NONE)
    {
        return ev;
    }
    return CheckButtonEdge(KNOB_BTN_NEXT_PORT, KNOB_BTN_NEXT_PIN,
                           &s_last_next, KNOB_BTN_EVENT_NEXT);
}
