# 力反馈智能旋钮（Haptic Smart Knob）STM32

基于 STM32F103C8T6，参考 [scottbez1/smartknob](https://github.com/scottbez1/smartknob) 设计。

---

## 已实现功能

### 虚拟卡位（棘轮手感）
- **中点位能峰 bump 模型**：卡位之间电机自由（如同未通电），接近中点时逆向阻力线性爬升，翻过中点后阻力释放滑入下一卡位
- **松手归中**：连续静止 20ms 后自动归中到最近卡位
- **预设方案**：6/12/24/48 卡位 + 完全平滑（5 种预设，一键切换）
- **力度可调**：卡位阻力和归中力分别可调（1-10 档）
- **蜂鸣反馈**：跨过卡位时触发短促蜂鸣音效

### 角度限位
- **三种模式**：关闭 / 单边上限 / 双边上下限
- **物理弹簧模拟**：双向弹簧 + 速度阻尼，越过边界后产生真实弹簧式来回弹跳振荡
- **参数可调**：弹簧刚度、阻尼、最大力均可调
- **限位音效**：撞击限位边界时触发蜂鸣提示
- **与卡位共存**：限位可与卡位同时使用，也可独立使用

### 1kHz 闭环控制
- **TIM3 1kHz 中断**驱动控制循环
- **优先级管理**：限位优先级高于卡位（边界保护）
- **智能蜂鸣**：
  - 限位激活时卡位检测和 Det# 同步冻结，避免超出边界时误触发
  - 边界振荡时 100ms 去抖保护，防止连续响
  - 可在 `App/Src/app_knob_limit.c` 中修改 `LIMIT_DEBOUNCE_MS` 调整去抖时间
- **低延迟响应**：传感器读取 → 状态机 → 力输出 < 1ms

---

## 硬件

| 组件 | 型号 | 说明 |
|------|------|------|
| MCU | STM32F103C8T6 | Cortex-M3, 72MHz, 20KB RAM, 64KB Flash |
| 电机 | N20 直流减速电机 | 100:1 减速比，6V 供电 |
| 电机驱动 | TB6612FNG | 双 H 桥驱动芯片 |
| 编码器 | N20 自带增量式 | 7 PPR × 4 倍频 × 100 减速比 = 2800 CPR |
| 蜂鸣器 | 有源蜂鸣器 | 低电平触发，PB0 |

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
| AO1 | 电机**白线** (电机电源-) |
| AO2 | 电机**红线** (电机电源+) |
| BO2 | 空置不接 |
| BO1 | 空置不接 |
| GND | 接系统共地 |

**右排针:**

| 引脚名 | 接到哪里 |
|---|---|
| PWMA | STM32 **PB6** (TIM4_CH1) |
| AIN2 | STM32 **PB8** |
| AIN1 | STM32 **PB7** |
| STBY | STM32 **PB9** (必须拉高使能) |
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
| PB9 | TB6612 STBY 使能 | GPIO Output, **必须拉高使能** |
| PB0 | 有源蜂鸣器 | GPIO Output, 低电平触发 |
| USART1 | 调试串口 | 115200, 8N1, TX=PA9, RX=PA10(可不接) |

---

## 软件架构（v2.0 新架构）

### 分层设计

```
┌────────────────────────────────────────────────┐
│  app_knob_ctrl.c (控制协调层)                   │
│  - 1kHz 控制循环入口                            │
│  - 读取传感器，调度状态机                       │
│  - 协调卡位/限位/蜂鸣器                         │
└────────────┬───────────────────────────────────┘
             │
  ┌──────────┼──────────┬─────────────┐
  │          │          │             │
┌─▼──────┐ ┌─▼──────┐ ┌─▼──────┐ ┌───▼────┐
│physics │ │ state  │ │ limit  │ │ event  │
│卡位物理│ │ 状态机  │ │ 限位   │ │ 事件   │
│模型    │ │        │ │ 模块   │ │ 层     │
└────────┘ └────────┘ └────────┘ └────────┘
     │          │          │          │
     └──────────┴──────────┴──────────┘
                │
        ┌───────▼────────┐
        │  BSP 层        │
        │  硬件抽象层     │
        └────────────────┘
```

### 文件结构

```
App/
├── Inc/
│   ├── app_knob_types.h       # 公共类型定义（配置、状态、数据结构）
│   ├── app_knob_physics.h     # 卡位物理模型接口（纯函数）
│   ├── app_knob_state.h       # 状态机接口（显式状态转换）
│   ├── app_knob_limit.h       # 限位模块接口（事件驱动）
│   ├── app_knob_event.h       # 事件通知层（蜂鸣器管理）
│   ├── app_knob_ctrl.h        # 控制协调层（顶层入口）
│   └── app_isr.h              # HAL 回调入口
└── Src/
    ├── app_knob_physics.c     # 预设参数 + 力计算
    ├── app_knob_state.c       # 状态机实现（FREE/RETURNING/LIMIT_BOUNCE）
    ├── app_knob_limit.c       # 双向弹簧 + 阻尼
    ├── app_knob_event.c       # 卡位检测 + 蜂鸣触发
    ├── app_knob_ctrl.c        # 主控制循环
    └── app_isr.c              # TIM3 1kHz → App_Knob_Control

Drivers/BSP/
├── Knob_Motor/                # N20 + TB6612 电机驱动
├── Knob_Encoder/              # TIM2 编码器读取 + 角度换算
└── Knob_Buzzer/               # 有源蜂鸣器驱动（脉冲管理）
```

### 模块职责

| 模块 | 职责 |
|------|------|
| **physics** | 查找卡位、计算力输出（无状态纯函数） |
| **state** | 管理 FREE/RETURNING/LIMIT_BOUNCE 三种状态 |
| **limit** | 限位检测、弹簧力计算、事件回调 |
| **event** | 卡位切换检测、蜂鸣器触发 |
| **ctrl** | 协调所有模块、主控制循环 |
| **isr** | HAL 弱符号覆盖 |

---

## 快速开始

### 1. 修改配置

打开 `App/Src/app_knob_ctrl.c`，找到第 49-52 行：

```c
void App_Knob_Init(void)
{
    // 默认配置：48 卡位，中等力度
    s_config.preset = KNOB_PRESET_DENSE_48;  // ← 改这里
    s_config.detent_strength = 7;             // ← 改这里 (1-10)
    s_config.return_strength = 8;             // ← 改这里 (1-10)
```

**可选预设：**
```c
KNOB_PRESET_COARSE_6    // 6 卡位/圈，粗糙手感
KNOB_PRESET_NORMAL_12   // 12 卡位/圈，标准手感
KNOB_PRESET_FINE_24     // 24 卡位/圈，精细手感
KNOB_PRESET_DENSE_48    // 48 卡位/圈，密集手感
KNOB_PRESET_SMOOTH      // 完全平滑，无卡位
```

**力度参数：**
- `detent_strength (1-10)` - 卡位爬坡阻力，数值越大越难翻过卡位
- `return_strength (1-10)` - 松手归中力，数值越大归中速度越快

### 2. 修改限位配置

打开 `App/Inc/app_knob_limit.h`，修改第 15-20 行：

```c
#define KNOB_LIMIT_DEFAULT_MODE    KNOB_LIMIT_MODE_DUAL  // OFF/SINGLE/DUAL
#define KNOB_LIMIT_DEFAULT_MIN     -180.0f  // 下界角度
#define KNOB_LIMIT_DEFAULT_MAX     360.0f   // 上界角度
#define KNOB_LIMIT_SPRING_KP       4.0f     // 弹簧刚度（越大越硬）
#define KNOB_LIMIT_SPRING_KD       1.5f     // 阻尼（越大振荡衰减越快）
#define KNOB_LIMIT_MAX_FORCE_PCT   55       // 限位最大推力 (%)
```

---

## 配置示例

### 示例 1：强烈卡位手感

```c
s_config.preset = KNOB_PRESET_NORMAL_12;  // 12 卡位（大间距）
s_config.detent_strength = 9;              // 很大的阻力
s_config.return_strength = 10;             // 快速归中
```

### 示例 2：平滑精细调节

```c
s_config.preset = KNOB_PRESET_DENSE_48;   // 48 卡位（小间距）
s_config.detent_strength = 4;              // 轻微阻力
s_config.return_strength = 5;              // 温和归中
```

### 示例 3：仅限位无卡位

```c
s_config.preset = KNOB_PRESET_SMOOTH;     // 无卡位
// 限位配置保持默认 DUAL 模式
```

### 示例 4：运行时动态切换

```c
// 在 main.c 中随时调用
KnobConfig_t new_cfg = {
    .preset = KNOB_PRESET_NORMAL_12,
    .detent_strength = 5,
    .return_strength = 6,
};
App_Knob_SetConfig(&new_cfg);
```

---

## API 参考

### 控制层 API

```c
// 初始化（在 main.c 中调用一次）
void App_Knob_Init(void);

// 1kHz 控制循环（由 TIM3 ISR 自动调用）
void App_Knob_Control(void);

// 串口调试输出（在 main 循环中每秒调用）
void App_Knob_Debug(void);

// 运行时配置接口
void App_Knob_SetConfig(const KnobConfig_t *cfg);
void App_Knob_GetConfig(KnobConfig_t *cfg);
```

### 限位模块 API

```c
// 运行时修改限位配置
void KnobLimit_SetConfig(const Knob_LimitConfig_t *cfg);
void KnobLimit_GetConfig(Knob_LimitConfig_t *cfg);
```

**运行时修改限位示例：**

```c
Knob_LimitConfig_t lim;
KnobLimit_GetConfig(&lim);
lim.mode = KNOB_LIMIT_MODE_SINGLE;  // 改为单边限位
lim.limit_max_deg = 180.0f;         // 上限 180°
KnobLimit_SetConfig(&lim);
```

---

## 调试输出

串口 115200 baud，每秒输出一行：

```
Angle:   47.25  Target:   45.00  Err:  -2.25  Out:  15.00  Det#:  6  State: 0
```

| 字段 | 含义 |
|------|------|
| **Angle** | 当前累计角度 (°) |
| **Target** | 最近卡位中心角度 |
| **Err** | 偏差 = Target - Angle |
| **Out** | 电机输出占空比 (%) |
| **Det#** | 卡位编号 |
| **State** | 状态（0=FREE, 1=RETURNING, 2=LIMIT_BOUNCE） |

---

## 状态机说明

旋钮有三种工作状态：

| 状态 | 说明 | 转换条件 |
|------|------|----------|
| **FREE** | 正常转动，有卡位手感 | 默认状态 |
| **RETURNING** | 松手后自动归中 | 连续静止 20ms → RETURNING |
| **LIMIT_BOUNCE** | 限位边界弹跳 | 越过限位边界 → LIMIT_BOUNCE |

**状态转换图：**

```
┌──────────┐  连续静止20ms   ┌────────────┐
│   FREE   │───────────────→│ RETURNING  │
│          │←───────────────│            │
└─────┬────┘  进入死区/反向  └────────────┘
      │         拧动
      │ 越界
      ▼
┌──────────┐  振荡稳定
│  LIMIT   │──────────────→ 回到 FREE
│  BOUNCE  │
└──────────┘
```

---

## 已知限制 & 优化建议

| 项目 | 说明 | 建议 |
|------|------|------|
| **编码器分辨率** | 2800 CPR → 每 count 0.13°，速度测量有同等级噪声 | 升级到更高分辨率编码器（如 AS5600 磁编码器） |
| **齿轮箱静摩擦** | 100:1 减速箱需要 12-14% 占空比才能克服静摩擦 | 使用低摩擦齿轮箱或无刷电机 |
| **N20 力矩上限** | N20 电机力矩有限，无法实现"硬墙"限位手感 | 升级到 BLDC 无刷电机 + FOC 控制 |
| **归中力振荡** | 归中力太高会冲过死区来回摆 | 当前 14% 地板 + 渐变是折中方案，可微调 `return_strength` |
| **无绝对零点** | 增量式编码器上电从 0 开始计数 | 添加霍尔传感器或磁编码器实现绝对位置 |

---

## 与 SmartKnob 原版对比

| 功能 | SmartKnob 原版 |   本项目 (v2.0)   |
|------|:---:|:--------------:|
| 虚拟卡位 | ✓ PID 弹簧 | ✓ EC11 棘轮 bump |
| 角度限位 | ✓ |  ✓ 双向弹簧 + 阻尼   |
| 松手归中 | ✓ |     ✓ 静止检测     |
| 卡位密度可调 | ✓ |  ✓ 预设方案 (5 种)  |
| 限位范围可调 | ✓ |   ✓ 运行时 API    |
| 蜂鸣反馈 | ✗ | ✓ 卡位切换 + 限位撞击  |
| 配置简化 | — | ✓ 3 参数替代 22 宏  |
| 模块化架构 | — | ✓ 分层设计 + 事件驱动  |
| 240×240 LCD | ✓ |     ✗ (暂无)     |
| 按压检测 | ✓ |    ✗ (无传感器)    |
| RGB LED 灯环 | ✓ |    ✗ (无灯珠)     |
| WiFi / BLE | ✓ |   ✗ (无无线模块)    |
| BLDC FOC 控制 | ✓ |  ✗ (N20 直流有刷)  |

---

## 更新日志

### v2.0.0 (2026-08-02)
- 重大架构重构：完全重写 App 层
- 简化配置系统：3 参数替代 22 宏（-86%）
- 模块化设计：分层架构，职责单一
- 显式状态机：清晰的状态转换逻辑
- 事件驱动：限位模块解耦
- 智能蜂鸣：
  - 限位激活时卡位检测和 Det# 同步冻结（避免超出边界误触发）
  - 边界振荡 100ms 去抖（防止连续响）

### v1.1.0 (2026-07-31)
- 增加限位点蜂鸣器音效
- 优化手感参数
- 代码分层和去冗余

### v1.0.0 (2026-07-30)
- 初始版本
- 实现基本卡位和限位功能
