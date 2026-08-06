/**
 * @file    bsp_knob_motor.h
 * @brief   旋钮电机驱动模块头文件，N20+TB6612，TIM4 CH1 PWM控制
 *
 * @details 通过TIM4 PWM占空比控制力矩/转速大小，AIN1/AIN2（PB7/PB8）
 *          控制转向，STBY（PB9）使能TB6612驱动板输出。TIM4配置为
 *          Prescaler=0 ARR=999，占空比=CCR/1000。占空比 0~100 映射到
 *          CCR 0~999，由 App 层控制循环（卡位/限位/归中）计算输出。
 *
 * @version 1.0.0
 * @date    2026/7/31
 */
#ifndef BSP_KNOB_MOTOR_H
#define BSP_KNOB_MOTOR_H

#include  "main.h"

// TIM4 ARR配置值，对应CCR满量程，占空比=CCR/该值
#define KNOB_MOTOR_PWM_MAX_CCR  999
// 电机所用PWM定时器通道，对应PB6
#define KNOB_MOTOR_TIM_CHANNEL  TIM_CHANNEL_1
// STBY引脚，拉高使能TB6612驱动输出，对应PB9
#define KNOB_MOTOR_STBY_PORT    GPIOB
#define KNOB_MOTOR_STBY_PIN     GPIO_PIN_9
// AIN1/AIN2方向控制引脚，对应PB7/PB8
#define KNOB_MOTOR_AIN1_PORT    GPIOB
#define KNOB_MOTOR_AIN1_PIN     GPIO_PIN_7
#define KNOB_MOTOR_AIN2_PORT    GPIOB
#define KNOB_MOTOR_AIN2_PIN     GPIO_PIN_8

/**
 * @brief 旋钮电机输出方向
 */
typedef enum
{
    KNOB_MOTOR_DIR_FORWARD = 0,     // AIN1=1，AIN2=0，前进
    KNOB_MOTOR_DIR_REVERSE,         // AIN1=0，AIN2=1，后退
    KNOB_MOTOR_DIR_BRAKE,           // AIN1=1，AIN2=1，刹车
    KNOB_MOTOR_DIR_STOP             // AIN1=0，AIN2=0，停止，不进行任何操作，使人手可以很轻松的转动
} Knob_Motor_Dir_t;

/**
 * @brief 初始化旋钮电机，启动PWM输出并使能TB6612驱动板
 * @details 只有调用HAL_TIM_PWM_Start之后，定时器才会真正开始往对应
 *          引脚(PB6)输出PWM波形，CubeMX生成的MX_TIM4_Init()只完成
 *          寄存器配置，不会自动启动输出，必须显式调用。STBY若未拉高，
 *          即使PWM和方向引脚都正确，驱动板也完全不会输出，这是最
 *          容易漏掉导致"电机不转"的一步
 * @param htim 已完成硬件初始化并配置好PWM的HAL定时器句柄指针
 */
void BSP_KnobMotor_Init(TIM_HandleTypeDef *htim);

/**
 * @brief 设置电机输出方向与占空比
 * @details 占空比先做0~100限幅，防止上层传入异常值导致CCR溢出。
 *          刹车状态（AIN1/AIN2同时置高）用于测试阶段主动停止电机，
 *          比单纯占空比归零更快让电机停下
 * @param dir 方向（正转/反转/刹车）
 * @param duty_pct 占空比百分比，范围0~100，超出范围会被截断到边界值
 */
void BSP_KnobMotor_SetOutput(Knob_Motor_Dir_t dir, uint8_t duty_pct);

#endif
