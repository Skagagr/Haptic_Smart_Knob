# 力反馈智能旋钮（Haptic Smart Knob）STM32

# 目前用到东西

## 配置

### TIM2 —— 编码器输入

| 配置项 | 值 |
|---|---|
| Mode | Combined Channels → Encoder Mode |
| Channel1 | Encoder Mode |
| Channel2 | Encoder Mode |
| Encoder Mode | TI1 and TI2(常用默认选项,四倍频计数) |
| Polarity (IC1/IC2) | 默认Rising Edge,后面测方向不对再改 |
| Counter Period (ARR) | 65535(用满16位范围) |
| Prescaler (PSC) | 0(编码器模式一般不需要额外分频) |


## 外部电路连接

### N20电机与TB6612

- N20编码器电机

| 电机引出线颜色 | 功能 | 接到哪里 |
|---|---|---|
| 红 | 电机电源+ | TB6612  **AO1** |
| 白 | 电机电源- | TB6612  **AO2** |
| 黑 | 编码器供电+ (VCC) | STM32 **3.3V** |
| 蓝 | 编码器供电- (GND) | STM32 **GND** |
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
