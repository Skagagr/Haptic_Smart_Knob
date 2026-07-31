/**
 * @file    app_knob.c
 * @brief
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/31
 */
#include "app_knob.h"
#include <stdio.h>
#include "bsp_knob_encoder.h"
#include "bsp_knob_motor.h"
#include "tim.h"

volatile int32_t raw_count = 0;
volatile float angle = 0;

void App_Knob_Init(void)
{
    BSP_KnobEncoder_Init(&htim2);
    BSP_KnobMotor_Init(&htim4);
    HAL_TIM_Base_Start_IT(&htim3);
}

void App_Knob_Test(void)
{
    raw_count = BSP_KnobEncoder_GetRawCount();
    angle = BSP_KnobEncoder_GetAngle();

    if (angle > 360)
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_REVERSE, 50);
    else if (angle < 0)
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_FORWARD, 50);
    else
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_STOP, 50);
}

void App_Knob_Debug(void)
{
    printf("Raw Count: %ld\r\n", raw_count);
    printf("Angle: %f\r\n", angle);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        App_Knob_Test();
    }
}