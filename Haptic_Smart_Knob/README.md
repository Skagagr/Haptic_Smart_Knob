# 力反馈智能旋钮（Haptic Smart Knob）STM32

基于 STM32F103C8T6，参考 [scottbez1/smartknob](https://github.com/scottbez1/smartknob) 设计。

## 已实现功能

### 虚拟卡位（EC11 棘轮手感）
- 中点位能峰 bump 模型：卡位之间电机自由（如同未通电），接近中点时逆向阻力线性爬升，翻过中点后阻力释放滑入下一卡位
- 松手检测：连续静止后自动归中到最近卡位
- 每圈卡位数可调（2~90），死区/爬坡起点按比例自动缩放
- 卡位可独立关闭（设为 0 即自由旋转）

### 角度限位
- 三种模式：关闭 / 单边上限 / 双边上下限
- 双向弹簧 + 速度阻尼，越过边界后产生真实弹簧式来回弹跳振荡
- 限位弹簧刚度、阻尼、最大力均可调
- 限位可与卡位共存，也可独立使用

### 1kHz 闭环控制
- TIM3 1kHz 中断驱动控制循环
- 先检查限位，再执行卡位逻辑

## 硬件

| 组件 | 型号 |
|------|------|
| MCU | STM32F103C8T6 (Cortex-M3, 72MHz) |
| 电机 | N20 直流减速电机 (100:1) |
| 电机驱动 | TB6612 (H 桥) |
| 编码器 | N20 自带增量式 (7 PPR × 4 倍频 × 100 减速比 = 2800 CPR) |

### N20 电机接线

| 电机引出线颜色 | 功能 | 接到哪里 |
|---|---|---|
| 红 | 电机电源+ | TB6612 **AO2** |
| 白 | 电机电源- | TB6612 **AO1** |
| 黑 | 编码器供电+ (VCC) | STM32 **3.3V** |
| 蓝 | 编码器供电- (GND) | STM32 **GND** |
| 绿 | 编码器A相 (C1) | STM32 **PA1** (TIM2_CH1) |
| 黄 | 编码器B相 (C2) | STM32 **PA0** (TIM2_CH2) |

### TB6612 接线

**左排针:**

| 引脚名 | 接到哪里 |
|---|---|
| VM | 电机电源正极 **6V** |
| VCC | STM32 **3.3V** |
| GND | 接系统共地 |
| AO1 | 电机**红线** (电机电源+) |
| AO2 | 电机**白线** (电机电源-) |
| BO2 | 空置不接 |
| BO1 | 空置不接 |
| GND | 接系统共地 |

**右排针:**

| 引脚名 | 接到哪里 |
|---|---|
| PWMA | STM32 **PB6** (TIM4_CH1) |
| AIN2 | STM32 **PB8** |
| AIN1 | STM32 **PB7** |
| STBY | STM32 **PB9** |
| BIN1 | 接 **GND** (B通道不用) |
| BIN2 | 接 **GND** (B通道不用) |
| PWMB | 空置不接 |
| GND | 接系统共地 |

### STM32 外设分配与配置

| 外设 | 功能 | 关键配置 |
|------|------|----------|
| TIM2 | 编码器接口 (A/B 相 PA0/PA1) | Encoder Mode TI1+TI2, PSC=0, ARR=65535 |
| TIM3 | 1kHz 控制循环中断 | PSC=71, ARR=999 → 72MHz/72/1000=1kHz |
| TIM4 CH1 | 电机 PWM (PB6) | PWM Mode 1, PSC=0, ARR=999, 占空比=CCR/1000 |
| PB7/PB8 | TB6612 AIN1/AIN2 方向控制 | GPIO Output |
| PB9 | TB6612 STBY 使能 | GPIO Output, 拉高使能 |
| USART1 | 调试串口 | 115200, 8N1, TX=PA9, RX=PA10(可不接) |

## 软件架构

```
App/
  Inc/
    app_knob.h      卡位配置 + API
    app_limit.h     限位模式/配置 + API
  Src/
    app_knob.c      卡位 bump 控制 + 归中状态机 (~380 行)
    app_limit.c     限位双向弹簧 + 阻尼 (~115 行)

Drivers/BSP/
  Knob_Motor/       N20 + TB6612 电机驱动
  Knob_Encoder/     TIM2 编码器读取 + 角度换算
```

## 参数调优

只需改 `KNOB_DEFAULT_NUM_DETENTS` 一个宏。所有力参数和角度阈值自动按半间距缩放。

### 卡位参数（自动缩放）

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `KNOB_DEFAULT_NUM_DETENTS` | 48 | 每圈卡位数 (2~90, 0=禁用)，改这一个即可 |
| `KNOB_REF_BUMP_MAX_PCT` | 20 | 爬坡阻力基准值 (12 卡位时的值) |
| `KNOB_REF_RETURN_FORCE_PCT` | 22 | 归中力基准值 (12 卡位时的值) |
| `KNOB_DEAD_ZONE_RATIO` | 0.13 | 死区比例 + 最小 0.6° 地板 |
| `KNOB_BUMP_START_RATIO` | 0.70 | 爬坡起点比例 + 最小 0.8° 爬坡宽度 |
| `KNOB_RETURN_FORCE_FLOOR` | 14.0 | 归中力地板 (% 占空比) |
| `KNOB_VEL_THRESHOLD` | 0.25 | 转动判定阈值 (°/ms) |
| `KNOB_STILL_THRESHOLD` | 0.18 | 静止判定阈值 (°/ms) |
| `KNOB_STILL_COUNT_NEEDED` | 20 | 松手判定延迟 (ms) |

**自动缩放对照表：**

| NUM_DETENTS | 半间距 | bump力 | 归中力 | 死区 | 爬坡起点 |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 6 | 30° | 20% | 22% | 3.9° | 21.0° |
| 12 | 15° | 20% | 22% | 2.0° | 10.5° |
| 24 | 7.5° | 15% | 16% | 1.0° | 5.3° |
| 48 | 3.75° | 12% | 14% | 0.6° | 3.0° |

### 限位参数

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `KNOB_LIMIT_DEFAULT_MODE` | DUAL | OFF / SINGLE / DUAL |
| `KNOB_LIMIT_DEFAULT_MIN` | -180 | 下界 (°) |
| `KNOB_LIMIT_DEFAULT_MAX` | 360 | 上界 (°) |
| `KNOB_LIMIT_SPRING_KP` | 4.0 | 弹簧刚度 (%/°) |
| `KNOB_LIMIT_SPRING_KD` | 1.5 | 阻尼系数 |
| `KNOB_LIMIT_MAX_FORCE_PCT` | 55 | 力上限 (% 占空比) |

### 限位使用示例

```c
// 关闭限位，仅卡位
#define KNOB_LIMIT_DEFAULT_MODE  KNOB_LIMIT_MODE_OFF

// 单边上限：不能超过 360°
#define KNOB_LIMIT_DEFAULT_MODE  KNOB_LIMIT_MODE_SINGLE
#define KNOB_LIMIT_DEFAULT_MAX   360.0f

// 双边限位：-180° ~ 360°
#define KNOB_LIMIT_DEFAULT_MODE  KNOB_LIMIT_MODE_DUAL
#define KNOB_LIMIT_DEFAULT_MIN   -180.0f
#define KNOB_LIMIT_DEFAULT_MAX   360.0f

// 仅限位，无卡位
#define KNOB_LIMIT_DEFAULT_MODE  KNOB_LIMIT_MODE_DUAL
#define KNOB_DEFAULT_NUM_DETENTS 0
```

## 运行时 API

App 层提供以下运行时配置接口（无需重编译即可调整）：

```c
// ---- 卡位 ----
App_Knob_SetDetentConfig(&cfg);  // 运行时修改卡位参数
App_Knob_GetDetentConfig(&cfg);  // 读取当前卡位参数

// ---- 限位 ----
App_Limit_SetConfig(&cfg);       // 运行时修改限位参数
App_Limit_GetConfig(&cfg);       // 读取当前限位参数
```

示例 — 从 main.c 运行时开启限位：

```c
Knob_LimitConfig_t lim;
App_Limit_GetConfig(&lim);
lim.mode          = KNOB_LIMIT_MODE_DUAL;
lim.limit_min_deg = -180.0f;
lim.limit_max_deg =  180.0f;
App_Limit_SetConfig(&lim);
```

## app_limit.c 内部参数

以下参数在 `App/Src/app_limit.c` 顶部，仅在需要精细调整弹跳行为时修改：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `LIMIT_FORCE_FLOOR` | 20 | 限位地板力 (% 占空比) |
| `LIMIT_SETTLE_ANGLE` | 3 | 稳定判定角度 (°) |
| `LIMIT_SETTLE_VEL` | 0.3 | 稳定判定速度 (°/ms) |
| `LIMIT_SETTLE_MS` | 50 | 连续稳定 ms 后退出弹跳 |

## 已知限制 & 可优化项

| 项目 | 说明 |
|------|------|
| **编码器分辨率** | 2800 CPR → 每 count 0.13°，速度测量有同等级噪声。低于 ~130°/s 的慢速转动无法可靠检测方向 |
| **齿轮箱静摩擦** | 100:1 减速箱需要 ~12-14% 占空比才能克服静摩擦，限制了最小输出力 |
| **N20 力矩上限** | N20 电机力矩有限，较难实现 SmartKnob 原版那种"硬墙"限位手感 |
| **归中力振荡** | 归中力太高会冲过死区来回摆，太低推不动摩擦。当前 14% 地板 + 渐变是折中方案 |
| **无串口协议** | 参数目前只能改 `#define` 重编译，或通过代码调用运行时 API。没有串口命令解析器 |
| **单编码器无零点** | 增量式编码器上电从 0 开始计数，没有绝对位置参考。如需绝对零点需外加传感器或限位开关 |
| **限位弹跳退出条件** | 连续 50ms 在 ±3° 且速度 < 0.3°/ms 才退出。极少数情况下可能停不下来（需手动微调参数） |

## 调试输出

串口 115200，每秒输出一行：

```
Angle:   -45.23  Target:   -45.00  Err:  -0.23  Out:  +0.00  Det#:    -3
```

| 字段 | 含义 |
|------|------|
| Angle | 当前累计角度 (°) |
| Target | 最近卡位中心角度 |
| Err | 偏差 = Target - Angle |
| Out | 电机输出 (% 占空比，正=正转 负=反转) |
| Det# | 卡位编号 (限位开启时截断到边界内) |

## 与 SmartKnob 原版对比

| 功能 | SmartKnob 原版 | 本项目 |
|------|:---:|:---:|
| 虚拟卡位 | ✓ PID 弹簧 | ✓ EC11 棘轮 bump |
| 角度限位 | ✓ | ✓ 双向弹簧 + 阻尼 |
| 松手归中 | ✓ | ✓ 静止检测 |
| 卡位密度可调 | ✓ | ✓ |
| 限位范围可调 | ✓ | ✓ |
| 240×240 LCD | ✓ | ✗ (硬件不支持) |
| 按压检测 | ✓ | ✗ (无传感器) |
| RGB LED 灯环 | ✓ | ✗ (无灯珠) |
| WiFi / BLE | ✓ | ✗ (无无线模块) |
| BLDC FOC 控制 | ✓ | ✗ (N20 直流有刷) |
| 串口实时调参 | ✓ | 待实现 |
| 手感预设切换 | — | 待实现 |

## 构建

```bash
cmake --build build/Debug
```

使用 STM32CubeMX 生成的项目框架 + CMake 构建系统。
