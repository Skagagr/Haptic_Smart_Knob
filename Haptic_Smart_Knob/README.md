# 力反馈智能旋钮（Haptic Smart Knob）STM32
基于STM32F103C8T6

## 已完成

1.编码器读取和角度换算
2.电机的正反转、刹车、停止控制

# 目前用到东西

## STM32外设与协议配置

### USART1 调试串口
- 目前只单向发送数据给PC

| 配置项 | 值                                         |
|---|-------------------------------------------|
| Mode | Asynchronous                              |
| Baud Rate | 115200                                    |
| Word Length | 8 Bits                                    |
| Parity | None                                      |
| Stop Bits | 1                                         |
| 引脚 | TX=PA9,RX=PA10(RX可不接,但引脚保留默认不用管)          |
| NVIC | 不需要开中断(用阻塞发送);后续若改成非阻塞DMA发送,再回来加DMA TX请求  |


### TIM2 编码器输入

| 配置项 | 值 |
|---|---|
| Mode | Combined Channels → Encoder Mode |
| Channel1 | Encoder Mode |
| Channel2 | Encoder Mode |
| Encoder Mode | TI1 and TI2(常用默认选项,四倍频计数) |
| Polarity (IC1/IC2) | 默认Rising Edge,后面测方向不对再改 |
| Counter Period (ARR) | 65535(用满16位范围) |
| Prescaler (PSC) | 0(编码器模式一般不需要额外分频) |

### TIM4 电机PWM(TB6612的PWMA)

| 配置项 | 值 |
|---|---|
| Channel1 | PWM Generation CH1 |
| Prescaler (PSC) | 0 |
| Counter Period (ARR) | 999(对应PWM频率≈72kHz;若后续实测电机啸叫明显,可改PSC=71、ARR=999降到1kHz) |
| Pulse (CCR初始值) | 0 |
| Counter Mode | Up |
| PWM Mode | PWM Mode 1(默认) |
| CH Polarity | High(默认) |
| 引脚 | PB6 |


## 外部电路连接

### N20电机与TB6612

- N20编码器电机

| 电机引出线颜色 | 功能 | 接到哪里                    |
|---|---|-------------------------|
| 红 | 电机电源+ | TB6612  **AO2**         |
| 白 | 电机电源- | TB6612  **AO1**         |
| 黑 | 编码器供电+ (VCC) | STM32 **3.3V**          |
| 蓝 | 编码器供电- (GND) | STM32 **GND**           |
| 绿 | 编码器A相 (C1) | STM32 **PA1** (TIM2_CH1) |
| 黄 | 编码器B相 (C2) | STM32 **PA0** (TIM2_CH2) |

- TB6612

### 左排针

| 引脚名 | 接到哪里 |
|---|---|
| VM | 电机电源正极 **6V** |
| VCC | STM32 **3.3V** |
| GND | 接系统共地 |
| AO1 | 电机**红线**(电机电源+) |
| AO2 | 电机**白线**(电机电源-) |
| BO2 | 空置不接 |
| BO1 | 空置不接 |
| GND | 接系统共地 

### 右排针

| 引脚名 | 接到哪里 |
|---|---|
| PWMA | STM32 **PB6** (TIM4_CH1) |
| AIN2 | STM32 **PB8** |
| AIN1 | STM32 **PB7** |
| STBY | STM32 **PB9** |
| BIN1 | 接**GND**(B通道不用) |
| BIN2 | 接**GND**(B通道不用) |
| PWMB | 空置不接 |
| GND | 接系统共地 |
