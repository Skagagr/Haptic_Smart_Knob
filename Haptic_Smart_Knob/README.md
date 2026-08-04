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

### USB 双向通信协议
- **帧格式**：`[0xAA][0x55][Type][Len][Payload][CRC8]`，双字节同步头 + 1 字节长度 + CRC8 校验
- **命令-应答模式**：PC 主导，MCU 应答，带状态码
- **查询角度**：PC 发 `GET_ANGLE` 命令，MCU 回传当前角度（float，小端）
- **设置预设**：PC 发 `SET_CONFIG` 命令，运行中切换 5 种卡位预设

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

### USB 通讯（micro-USB 口）接线与标准初始流程

| micro-USB 信号 | 接到哪里 | 说明 |
|---|---|---|
| D- | STM32 **PA11** (USB_DM) | 板上自带 1.5k 上拉 |
| D+ | STM32 **PA12** (USB_DP) | 板上自带 1.5k 上拉 |
| VBUS (5V) | 板载 LDO → **3.3V** | 仅供 MCU，电机 **6V 独立供电** |
| GND | 接系统共地 | |

**标准流程：** CubeMX 使能 USB-FS Device + CDC 中间件后，在生成代码的
`USB_DEVICE/App/usbd_cdc_if.c` 中加一行转发，业务逻辑放 App 层：

`usbd_cdc_if.c` 的 `CDC_Receive_FS`：
```c
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
    Usb_OnReceive(Buf, *Len);  // USB 收包入口，在此转发调用。USB 收包是事件回调，主循环收不到
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}
```

`App/Src/app_usb.c`（传输层桥，转发给协议层）：
```c
#include "app_usb.h"
#include "app_usb_protocol.h"

void Usb_OnReceive(uint8_t *buf, uint32_t len)
{
    UsbProto_HandleRx(buf, len);
}
```

> USB 收包是中断/事件驱动的，必须经过 `CDC_Receive_FS` 回调，主循环无法轮询。
> 业务逻辑不放生成文件，只在 USER CODE 区留一行转发，处理都放 App 层。
> 收到字节后交由 `app_usb_protocol` 协议层做帧解析与命令分发。

### USB 双向通信协议（帧头 + 长度 + 校验）

帧格式（定义在 `App/Inc/app_usb_protocol.h`）：

```
[0xAA][0x55][Type][Len][Payload 0~255B][CRC8]
```

- **同步头**：`0xAA 0x55` 双字节，互反码抗干扰
- **Len**：载荷字节数 (0~255)，一帧总长 = 4 + Len（2 同步头 + 1 类型 + 1 长度 + Len 载荷 + 1 CRC）
- **CRC8**：多项式 0x07，覆盖 Type+Len+Payload（不含同步头）
- **响应类型** = 命令类型 | 0x80，响应载荷首字节为状态码
- **字节序**：小端（STM32 与 x86 PC 均为小端）

**已实现命令（PC → MCU）：**

| 命令 | 类型 | 载荷 | 说明 |
|------|------|------|------|
| `SET_CONFIG` | 0x02 | 1B = preset (0~4) | 设置预设，调用 `App_Knob_SetConfig` |
| `GET_ANGLE` | 0x03 | 空 | 查询当前角度，返回 4B float（小端） |

**状态码（响应载荷首字节）：**

| 状态码 | 值 | 含义 |
|--------|----|------|
| `OK` | 0x00 | 成功 |
| `ERR_LEN` | 0x01 | 长度字段不匹配 |
| `ERR_PARAM` | 0x02 | 参数非法（preset 越界） |
| `ERR_UNKNOWN` | 0x03 | 未知命令 |

**PC 端测试帧（十六进制）：**

```
查询角度:  AA 55 03 00 3F
          → 回 AA 55 83 05 00 <角度4B> <CRC>   // 00=OK, 后 4 字节小端 float

设置预设:  AA 55 02 01 03 CA                   // DENSE_48 (3)
          → 回 AA 55 82 01 00 <CRC>           // ACK
```

**新增一条命令**只需改 3 处（以 `CMD_GET_STATE = 0x04` 为例）：
1. `app_usb_protocol.h` 枚举加 `USB_PROTO_CMD_GET_STATE = 0x04`
2. `app_usb_protocol.c` 的 `UsbProto_Dispatch()` 加分发分支
3. 实现处理函数（参照 `UsbProto_HandleGetAngle`，响应载荷首字节放状态码）
   CRC 可交给 `UsbProto_SendFrame` 自动计算，无需手算。

> 发送为异步：`CDC_Transmit_FS` 只是把缓冲指针交给 USB 端点，帧缓冲须常驻，
> 发送忙时（返回 `USBD_BUSY`）当前帧会被丢弃（PC 端轮询慢时不会触发）。


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
| USB | USB-FS Device (CDC 虚拟串口) | CDC (VCP), 48MHz = PLLCLK/1.5, PA11=USB_DM, PA12=USB_DP |

---

## 软件架构 v3.2

### 分层设计

```
┌──────────────────────────────────────────┐
│  app_knob.c (应用层入口)                 │
│  - 1kHz 控制循环 + 状态机 + ISR          │
│  - 蜂鸣器事件 + 卡位检测                 │
│  - 协调 physics / limit 子模块           │
└────────────┬─────────────────────────────┘
             │
     ┌───────┼───────┐
     │               │
┌────▼─────┐   ┌─────▼────┐
│ physics  │   │  limit   │
│ 卡位物理 │   │  限位    │
│ 纯计算   │   │  模块    │
└──────────┘   └──────────┘
     │               │
     └───────┬───────┘
             │
     ┌───────▼────────┐
     │  BSP 层        │
     │  硬件抽象层    │
     └────────────────┘
```

### 文件结构

```
App/
├── Inc/
│   ├── app_knob.h              # 类型定义 + 对外 API
│   ├── app_knob_physics.h      # 卡位物理模型接口（纯函数）
│   ├── app_knob_limit.h        # 限位模块接口
│   ├── app_usb.h               # USB 收包转发入口
│   └── app_usb_protocol.h      # USB 协议定义（帧格式/命令/状态码）
└── Src/
    ├── app_knob.c              # 控制循环 + 状态机 + ISR + 蜂鸣器事件
    ├── app_knob_physics.c      # 预设参数 + 力计算
    ├── app_knob_limit.c        # 双向弹簧 + 阻尼
    ├── app_usb.c               # USB 传输层 → 协议层桥
    └── app_usb_protocol.c      # 帧解析状态机 + CRC8 + 命令分发

Drivers/BSP/
├── Knob_Motor/                 # N20 + TB6612 电机驱动
├── Knob_Encoder/               # TIM2 编码器读取 + 角度换算
└── Knob_Buzzer/                # 有源蜂鸣器驱动（脉冲管理）

USB_DEVICE/App/                 # CubeMX 生成（一般不修改）
└── usbd_cdc_if.c               # USB 收发回调，USER CODE 区转发到 App
```

### 模块职责

| 模块 | 职责 |
|------|------|
| **app_knob** | 控制循环、状态机、ISR 回调、蜂鸣器事件、配置管理 |
| **physics** | 查找卡位、计算力输出（无状态纯函数） |
| **limit** | 限位检测、弹簧力计算、振荡稳定检测 |
| **app_usb** | USB 收包转发（传输层 → 协议层桥） |
| **app_usb_protocol** | 帧解析状态机、CRC8 校验、命令分发、响应组帧 |

**USB 收包数据流（中断上下文）：**

```
USB OUT 中断
  └→ usbd_cdc_if.c: CDC_Receive_FS
       └→ app_usb.c: Usb_OnReceive(buf, len)
            └→ app_usb_protocol.c: UsbProto_HandleRx
                 └→ 解析状态机（跨 USB 包重组）→ CRC8 校验 → 命令分发
                      ├→ SET_CONFIG → App_Knob_SetConfig()
                      └→ GET_ANGLE  → BSP_KnobEncoder_GetAngle()
                  响应帧 → UsbProto_SendFrame() → CDC_Transmit_FS()
```

---

## 快速开始

### 1. 修改配置

打开 `App/Src/app_knob.c`，找到第 71-73 行：

```c
void App_Knob_Init(void)
{
    // 默认配置：24 卡位，中等力度
    s_config.preset = KNOB_PRESET_FINE_24;   // ← 改这里
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

### 应用层 API（app_knob.h）

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
KnobLimit_GetConfig(&lim);          // 获取本地副本
lim.mode = KNOB_LIMIT_MODE_SINGLE;  // 改为单边限位
lim.limit_max_deg = 180.0f;         // 上限 180°
KnobLimit_SetConfig(&lim);          // 提交修改
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

| 功能 | SmartKnob 原版 |   本项目 (v3.2)   |
|------|:---:|:--------------:|
| 虚拟卡位 | ✓ PID 弹簧 | ✓ EC11 棘轮 bump |
| 角度限位 | ✓ |  ✓ 双向弹簧 + 阻尼   |
| 松手归中 | ✓ |     ✓ 静止检测     |
| 卡位密度可调 | ✓ |  ✓ 预设方案 (5 种)  |
| 限位范围可调 | ✓ |   ✓ 运行时 API    |
| 蜂鸣反馈 | ✗ | ✓ 卡位切换 + 限位撞击  |
| 240×240 LCD | ✓ |     ✗ (暂无)     |
| 按压检测 | ✓ |    ✗ (无传感器)    |
| RGB LED 灯环 | ✓ |    ✗ (无灯珠)     |
| WiFi / BLE | ✓ |   ✗ (无无线模块)    |
| BLDC FOC 控制 | ✓ |  ✗ (N20 直流有刷)  |

---

## 功能路线图（开发中）

| 阶段   | 内容 | 状态 |
|------|------|------|
| 初始工程 | USB-CDC 通讯（CubeMX 配置 + 电脑识别 COM 口） | 已完成 |
| 基础通信 | 双向通信协议（帧头 + 长度 + 校验 + 查询/设置命令） | 已完成 |
| 旋钮控制 | 旋钮伺服跟随（闭环位置环 + 人手接管检测） | 待做 |
| 最终交互 | C# 上位机（CoreAudio 音量 / WMI 亮度 / 媒体键 + 状态回读） | 待做 |

---

## 更新日志

### v3.2.0 (2026-08-04)
- 新增 USB 双向通信协议（`app_usb_protocol.h/c`）
  - 帧格式：`0xAA 0x55` 双字节同步头 + 1 字节长度 + CRC8 校验（多项式 0x07）
  - 接收状态机：跨 USB 包重组 + 校验 + 重同步
  - 命令-应答：`GET_ANGLE` 查询角度（float 小端回传）、`SET_CONFIG` 运行中切换预设
  - 状态码：OK / ERR_LEN / ERR_PARAM / ERR_UNKNOWN
- `app_usb.c` 由 echo 改为转发协议层解析

### v3.1.0 (2026-08-03)
- 新增 USB-CDC 虚拟串口通讯（硬件接线 + CubeMX 配置 + echo 收发验证通过）

### v3.0.0 (2026-08-03)
- 精简架构：13 文件 → 6 文件
- 合并 ctrl/state/event/isr/types 为单一 app_knob 模块
- 删除回调模式：limit 模块直接调用，减少间接层
- 删除未使用的枚举和类型（KnobEventType_t、LimitEventCallback_t）
- 对外 API 和运行时配置接口不变

### v2.0.0 (2026-08-02)
- 重大架构重构：完全重写 App 层
- 简化配置系统：3 参数替代 22 宏
- 模块化设计：分层架构，职责单一
- 显式状态机：清晰的状态转换逻辑
- 事件驱动：限位模块解耦
- 智能蜂鸣：限位激活时卡位检测和 Det# 同步冻结，边界振荡 100ms 去抖

### v1.1.0 (2026-07-31)
- 增加限位点蜂鸣器音效
- 优化手感参数
- 代码分层和去冗余

### v1.0.0 (2026-07-30)
- 初始版本
- 实现基本卡位和限位功能
