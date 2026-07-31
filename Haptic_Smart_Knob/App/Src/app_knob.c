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

void App_Knob_Init(void)
{
    BSP_KnobEncoder_Init(&htim2);
    BSP_KnobMotor_Init(&htim4);
}

void App_Knob_Test(void)
{
    int32_t raw_count = BSP_KnobEncoder_GetRawCount();
    float angle = BSP_KnobEncoder_GetAngle();

    if (angle > 360)
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_REVERSE, 50);
    else if (angle < 0)
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_FORWARD, 50);
    else
        BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_STOP, 50);

    // printf("Forward 30%%\r\n");
    // BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_FORWARD, 30);
    // HAL_Delay(2000);
    //
    // printf("Brake\r\n");
    // BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_BRAKE, 0);
    // HAL_Delay(1000);
    //
    // printf("Reverse 30%%\r\n");
    // BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_REVERSE, 30);
    // HAL_Delay(2000);
    //
    // printf("Brake\r\n");
    // BSP_KnobMotor_SetOutput(KNOB_MOTOR_DIR_BRAKE, 0);
    // HAL_Delay(1000);
}