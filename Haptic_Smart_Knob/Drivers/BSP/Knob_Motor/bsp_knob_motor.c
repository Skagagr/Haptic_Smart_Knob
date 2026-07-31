/**
 * @file    bsp_knob_motor.c
 * @brief   旋钮编码器驱动模块实现
 *
 * @version 1.0.0
 * @date    2026/7/31
 */
#include "bsp_knob_motor.h"

// 保存Init时传送的定时器句柄，后续SetOutput复用
// 避免每次调用都要求外部重复传入句柄
static TIM_HandleTypeDef *s_htim;

void BSP_KnobMotor_Init(TIM_HandleTypeDef *htim)
{
    s_htim = htim;
    HAL_TIM_PWM_Start(s_htim, KNOB_MOTOR_TIM_CHANNEL);
    __HAL_TIM_SET_COMPARE(s_htim, KNOB_MOTOR_TIM_CHANNEL, 0);

    // 上电默认停止状态，方向引脚都拉低
    HAL_GPIO_WritePin(KNOB_MOTOR_AIN1_PORT, KNOB_MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(KNOB_MOTOR_AIN2_PORT, KNOB_MOTOR_AIN2_PIN, GPIO_PIN_RESET);

    // STBY拉高，使能TB6612驱动输出
    HAL_GPIO_WritePin(KNOB_MOTOR_STBY_PORT, KNOB_MOTOR_STBY_PIN, GPIO_PIN_SET);
}

void BSP_KnobMotor_SetOutput(Knob_Motor_Dir_t dir, uint8_t duty_pct)
{
    if (duty_pct > 100) duty_pct = 100;

    switch (dir)
    {
        case KNOB_MOTOR_DIR_FORWARD:
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN1_PORT, KNOB_MOTOR_AIN1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN2_PORT, KNOB_MOTOR_AIN2_PIN, GPIO_PIN_RESET);
            break;
        case KNOB_MOTOR_DIR_REVERSE:
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN1_PORT, KNOB_MOTOR_AIN1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN2_PORT, KNOB_MOTOR_AIN2_PIN, GPIO_PIN_SET);
            break;
        case KNOB_MOTOR_DIR_BRAKE:
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN1_PORT, KNOB_MOTOR_AIN1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN2_PORT, KNOB_MOTOR_AIN2_PIN, GPIO_PIN_SET);
            break;
        case KNOB_MOTOR_DIR_WAIT:
        default:
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN1_PORT, KNOB_MOTOR_AIN1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(KNOB_MOTOR_AIN2_PORT, KNOB_MOTOR_AIN2_PIN, GPIO_PIN_RESET);
            break;
    }

    uint32_t ccr = (uint32_t)duty_pct * KNOB_MOTOR_PWM_MAX_CCR / 100;
    __HAL_TIM_SET_COMPARE(s_htim, KNOB_MOTOR_TIM_CHANNEL, ccr);
}