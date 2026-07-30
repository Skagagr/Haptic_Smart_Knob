/**
 * @file    bsp_knob_encoder.c
 * @brief   旋钮编码器驱动模块实现
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/30
 */
#include "bsp_knob_encoder.h"

// 保存Init时传入的定时器句柄，后续GetRawCount复用，
// 避免每次调用都要求外部重复传入句柄
static TIM_HandleTypeDef *s_htim;

void BSP_KnobEncoder_Init(TIM_HandleTypeDef *htim)
{
    s_htim = htim;
    HAL_TIM_Encoder_Start(s_htim, TIM_CHANNEL_ALL);

    // 上电清零，把当前物理位置定义为软件0点
    __HAL_TIM_SET_COUNTER(s_htim, 0);
}

int32_t BSP_KnobEncoder_GetRawCount(void)
{
    // TIM->CNT是16位无符号寄存器，但编码器双向计数时，反转会导致
    // 寄存器从0向下"借位"变成65535附近的大数。将寄存器值强转为
    // int16_t可以让CPU按补码规则自动还原成负数，从而在±32767范围内
    // 正确识别正反方向，这是STM32编码器接口读数的标准处理方式
    int16_t raw = (int16_t)__HAL_TIM_GET_COUNTER(s_htim);
    return (int32_t)raw;
}

float BSP_KnobEncoder_GetAngle(void)
{
    int32_t count = BSP_KnobEncoder_GetRawCount();
    return ((float)count / KNOB_ENCODER_COUNTS_PER_REV) * 360.0f;
}
