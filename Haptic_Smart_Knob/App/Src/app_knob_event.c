/**
 * @file    app_knob_event.c
 * @brief   事件通知层实现
 * @details 统一管理蜂鸣器触发和卡位检测
 * @version 2.0.0
 * @date    2026/8/2
 */
#include "app_knob_event.h"
#include "bsp_knob_buzzer.h"

// ===== 模块内部状态（只在 Init 时写入，运行时由 UpdateDetent 修改） =====
static int s_last_detent_index;             ///< 上次卡位编号，用于检测切换

void KnobEvent_Init(void)
{
    BSP_KnobBuzzer_Init();
    s_last_detent_index = 0;
}

void KnobEvent_Trigger(KnobEventType_t type)
{
    switch (type)
    {
        case KNOB_EVENT_DETENT_CROSS:
            BSP_KnobBuzzer_Click();
            break;

        case KNOB_EVENT_LIMIT_HIT:
            BSP_KnobBuzzer_Click();
            break;

        case KNOB_EVENT_STATE_CHANGE:
            // 状态切换暂不触发蜂鸣
            break;

        default:
            break;
    }
}

void KnobEvent_Tick(void)
{
    BSP_KnobBuzzer_Tick();
}

void KnobEvent_UpdateDetent(int detent_index)
{
    if (detent_index != s_last_detent_index)
    {
        s_last_detent_index = detent_index;
        KnobEvent_Trigger(KNOB_EVENT_DETENT_CROSS);
    }
}
