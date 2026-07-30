/**
 * @file    bsp_knob_encoder.h
 * @brief   旋钮编码器驱动模块头文件，TIM2 Encoder Mode
 *
 * @details 编码器绑定在TIM2，四倍频计数模式（TI1 and TI2）。旋钮轴
 *          转一整圈的总计数值 = 7 PPR × 4倍频 × 100减速比 = 2800，
 *          该数值是电机实测/标称参数决定的，如后续实测怀疑有偏差，
 *          可清零计数器手动转整一圈、读最终计数值反推真实值替换
 *          本文件里的常量
 *
 * @version 1.0.0
 * @date    2026/7/30
 */
#ifndef BSP_KNOB_ENCODER_H
#define BSP_KNOB_ENCODER_H

#include "main.h"

// 旋钮轴转一整圈的总计数值：7 PPR x 4倍频 x 100减速比(理论值
#define KNOB_ENCODER_COUNTS_PER_REV 2800.0f

/**
 * @brief 初始化旋钮编码器，启动TIM2编码器接口并清零计数
 * @details 只有调用HAL_TIM_Encoder_Start之后，定时器才会真正开始
 *          根据A/B相边沿变化自动计数，CubeMX生成的MX_TIM2_Init()
 *          只完成寄存器配置，不会自动启动。清零计数是把上电时刻的
 *          物理位置定义为软件"0点"，注意这不代表旋钮真的转到了
 *          机械限位起点，回零/寻边是后续步骤要解决的问题
 * @param htim 已完成硬件初始化并配置好Encoder Mode的HAL定时器句柄指针
 */
void BSP_KnobEncoder_Init(TIM_HandleTypeDef *htim);

/**
 * @brief 获取旋钮当前原始计数值
 * @return 有符号计数值，已处理16位寄存器的正反方向还原
 */
int32_t BSP_KnobEncoder_GetRawCount(void);

/**
 * @brief 获取旋钮当前角度
 * @details 换算系数（每圈总计数）由本层维护，角度 = 计数值/2800×360°，
 *          上层App不需要知道2800这个数字从哪来
 * @return 角度值，单位：度
 */
float BSP_KnobEncoder_GetAngle(void);

#endif
