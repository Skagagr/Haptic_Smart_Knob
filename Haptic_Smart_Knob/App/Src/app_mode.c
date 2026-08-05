/**
 * @file    app_mode.c
 * @brief   控制模式管理实现 — 模式状态 + 按钮业务逻辑
 * @details 模块职责：
 *          - 维护当前控制模式（默认空闲）
 *          - 处理按钮事件（回空闲 / 切下一状态）
 *          - 模式变化时通过 BSP_KnobIndicator 点亮对应 LED
 *          与 app_knob（力反馈）完全解耦，互不依赖。
 * @version 1.0.0
 * @date    2026/8/5
 */
#include "app_mode.h"
#include "bsp_knob_indicator.h"

// =============================== 模块内部状态  ===============================
static KnobStateMode_t s_mode;   ///< 当前控制模式

// ================================= 对外函数  =================================

void AppMode_Init(void)
{
    BSP_KnobIndicator_Init();
    AppMode_SetMode(KNOB_STATE_MODE_IDLE);
}

KnobStateMode_t AppMode_GetMode(void)
{
    return s_mode;
}

void AppMode_SetMode(KnobStateMode_t mode)
{
    // 钳制非法值（协议/上位机可能传越界数据）
    if (mode > KNOB_STATE_MODE_BRIGHTNESS)
    {
        mode = KNOB_STATE_MODE_IDLE;
    }
    s_mode = mode;
    BSP_KnobIndicator_SetMode(s_mode);
}

void AppMode_NextMode(void)
{
    switch (s_mode)
    {
        case KNOB_STATE_MODE_IDLE:
            AppMode_SetMode(KNOB_STATE_MODE_VOLUME);
            break;
        case KNOB_STATE_MODE_VOLUME:
            AppMode_SetMode(KNOB_STATE_MODE_BRIGHTNESS);
            break;
        case KNOB_STATE_MODE_BRIGHTNESS:
        default:
            AppMode_SetMode(KNOB_STATE_MODE_IDLE);
            break;
    }
}

void AppMode_ScanButtons(void)
{
    KnobBtnEvent_t ev = BSP_KnobButton_Scan();
    switch (ev)
    {
        case KNOB_BTN_EVENT_BACK:
            AppMode_SetMode(KNOB_STATE_MODE_IDLE);   // PB12：回空闲
            break;
        case KNOB_BTN_EVENT_NEXT:
            AppMode_NextMode();                      // PB13：切下一状态
            break;
        default:
            break;
    }
}
