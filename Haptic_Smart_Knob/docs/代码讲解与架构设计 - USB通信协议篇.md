# 代码讲解与架构设计 - USB 通信协议篇

> 本篇讲解**下位机（STM32）**的 USB-CDC 通信协议设计与实现：
> 帧格式、CRC8 校验、逐字节解析状态机、命令集、收发数据流。
> 配套源码：`App/Src/app_usb_protocol.c`、`App/Inc/app_usb_protocol.h`、
> `App/Src/app_usb.c`、`App/Inc/app_usb.h`

## 目录

1. [通信链路概述](#1-通信链路概述)
2. [帧格式设计](#2-帧格式设计)
3. [CRC8 校验](#3-crc8-校验)
4. [逐字节解析状态机](#4-逐字节解析状态机)
5. [命令集与响应](#5-命令集与响应)
6. [收发数据流](#6-收发数据流)
7. [可靠性设计](#7-可靠性设计)
8. [如何新增一条命令](#8-如何新增一条命令)

---

## 1. 通信链路概述

### 1.1 为什么需要自定义协议

STM32 的 USB-CDC 在 PC 上表现为一个**虚拟串口**。它传输的是"字节流"，
**本身没有帧边界**——PC 发一帧 10 字节，底层 USB 可能拆成 2 段、3 段到达，
也可能多帧数据粘在一起到达。

所以我们需要在字节流之上定义一层**帧协议**：
- 明确"一帧从哪开始、到哪结束"
- 校验数据完整性（传输中是否有噪声）
- 约定命令编号与响应格式

### 1.2 通信方向约定

```
PC（上位机）                STM32（下位机）
    │   ── 命令帧 ──→        │
    │                        │  解析、执行
    │   ←── 响应帧 ────      │  回 ACK / 数据
```

- **命令-应答模式**：PC 主导，发一条命令，MCU 回一条响应
- MCU **不主动**上报（除非未来扩展事件推送）

---

## 2. 帧格式设计

### 2.1 一帧的结构

```
┌──────┬──────┬──────┬──────┬────────────┬──────┐
│ 0xAA │ 0x55 │ Type │ Len  │ Payload    │ CRC8 │
│ 同步头(2B)  │ 类型 │ 长度 │ 载荷(可变)  │ 校验 │
└──────┴──────┴──────┴──────┴────────────┴──────┘
```

| 字段 | 长度 | 说明 |
|------|------|------|
| **同步头** | 2 B | `0xAA 0x55` 固定，标记帧开始 |
| **Type** | 1 B | 命令类型（0x02~0x09） |
| **Len** | 1 B | 载荷字节数（0~255） |
| **Payload** | 0~255 B | 命令数据（可空） |
| **CRC8** | 1 B | 校验 Type+Len+Payload |

**一帧总长** = 4 + Len（2 同步头 + 1 类型 + 1 长度 + Len 载荷 + 1 CRC）。

### 2.2 为什么用双字节同步头 `0xAA 0x55`

- `0xAA` = `10101010`，`0x55` = `01010101`，二进制**互反码**
- 数据线受干扰时，两个字节同时翻成这种形态的概率极低
- 比单字节同步头抗干扰能力强，且代码里重同步逻辑简单

### 2.3 同步头出现在载荷里怎么办

不会误判。解析状态机在 `PAYLOAD` 阶段**靠长度字段决定何时结束**，
不关心内容是什么——只有回到"等待同步头"状态时才检查 `0xAA`。
（详见第 4 节状态机）

### 2.4 字节序

**小端**（Little-Endian）。STM32（Cortex-M3）和 x86 PC 都是小端，
float 等多字节数据直接 `memcpy` 即可，两端都不用翻转。

---

## 3. CRC8 校验

### 3.1 校验范围

```
CRC8 覆盖：Type + Len + Payload   （不含同步头）
```

### 3.2 算法参数

| 参数 | 值 |
|------|-----|
| 多项式 | 0x07 |
| 初始值 | 0x00 |
| 位序 | MSB 先行（不反转） |
| 结果取反 | 否 |

这是标准的 **CRC-8/MAXIM** 变体（多项式 0x07），和 CRC-8/SMBUS 一致。

### 3.3 查表实现

```c
// 256 字节预计算表（多项式 0x07）
static const uint8_t s_crc8_table[256] = { ... };

// 增量计算：crc = 上次结果，data = 数据，len = 长度
static uint8_t UsbProto_Crc8Update(uint8_t crc, const uint8_t *data, uint8_t len)
{
    while (len--)
    {
        crc = s_crc8_table[crc ^ *data++];
    }
    return crc;
}
```

**为什么不逐位算？** 查表把"8 次移位异或"变成"1 次数组下标访问"，
快 8 倍，对 ISR 上下文友好。

### 3.4 校验流程

- **发送端**：`crc = Crc8Update(0x00, [Type Len Payload...])` → 结果放帧尾
- **接收端**：用同一算法对 Type+Len+Payload 重算，与收到的帧尾比对
  - 相等 → 数据完整 → 执行命令
  - 不等 → 数据被破坏 → **整帧丢弃**

---

## 4. 逐字节解析状态机

### 4.1 为什么需要状态机

USB-CDC 是流，一帧可能被拆成多段到达。状态机**跨多次接收调用保持状态**，
逐字节喂入，攒够一帧再处理。

### 4.2 六个状态

```
        收到 0xAA        收到 0x55        读到 Type
IDLE ────────────→ WAIT55 ──────────→ TYPE ────────→ LEN
                                                   │
                                     Len=0         │ Len>0
                                        ↓          ↓
                                   CRC ←──────── PAYLOAD
```

| 状态 | 含义 |
|------|------|
| `WAIT_AA` | 等待同步头第 1 字节 `0xAA` |
| `WAIT_55` | 等待同步头第 2 字节 `0x55` |
| `TYPE` | 已收 Type，等 Len |
| `LEN` | 已收 Len，进入载荷或校验 |
| `PAYLOAD` | 攒 Len 个载荷字节 |
| `CRC` | 收到校验字节，比对后回到 `WAIT_AA` |

### 4.3 核心代码

```c
void UsbProto_HandleRx(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        const uint8_t b = buf[i];

        switch (s_state)
        {
            case PARSER_WAIT_AA:
                if (b == USB_PROTO_SYNC_AA) s_state = PARSER_WAIT_55;
                break;

            case PARSER_WAIT_55:
                if (b == USB_PROTO_SYNC_55) s_state = PARSER_TYPE;
                else if (b == USB_PROTO_SYNC_AA) s_state = PARSER_WAIT_55;  // 连续 AA
                else s_state = PARSER_WAIT_AA;
                break;

            case PARSER_TYPE:
                s_rx_type = b;
                s_state = PARSER_LEN;
                break;

            case PARSER_LEN:
                s_rx_len = b;
                s_rx_index = 0;
                s_state = (s_rx_len > 0) ? PARSER_PAYLOAD : PARSER_CRC;
                break;

            case PARSER_PAYLOAD:
                s_rx_payload[s_rx_index++] = b;
                if (s_rx_index >= s_rx_len) s_state = PARSER_CRC;
                break;

            case PARSER_CRC:
            {
                s_rx_crc = b;
                uint8_t crc = UsbProto_Crc8Update(0x00, &s_rx_type, 1);
                crc = UsbProto_Crc8Update(crc, &s_rx_len, 1);
                crc = UsbProto_Crc8Update(crc, s_rx_payload, s_rx_len);
                if (crc == s_rx_crc) UsbProto_Dispatch();   // 通过才执行
                s_state = PARSER_WAIT_AA;                   // 无论如何回 IDLE
                break;
            }
        }
    }
}
```

### 4.4 关键设计点

- **状态跨调用保持**：`s_state`、`s_rx_payload[]` 都是 static，多次 `HandleRx`
  调用之间不丢状态
- **CRC 错误 → 整帧丢弃**，直接回 `WAIT_AA`，不影响下一帧
- **连续 `0xAA`**：`WAIT_55` 里再遇 `0xAA` 留在 `WAIT_55`（可能是两帧粘连），
  不误回 IDLE

---

## 5. 命令集与响应

### 5.1 命令类型（PC → MCU）

| 命令 | Type | 载荷 | 说明 |
|------|------|------|------|
| `SET_CONFIG` | 0x02 | 1B = preset (0~4) | 设置卡位预设 |
| `GET_ANGLE` | 0x03 | 空 | 查询当前角度 |
| `SET_LIMIT_MODE` | 0x04 | 1B = mode (0关/1单边/2双边) | 设置限位模式 |
| `GET_STATE` | 0x07 | 空 | 查询状态（模式+角度合一） |
| `SET_MODE` | 0x08 | 1B = mode (0空闲/1音量/2亮度) | 设置控制模式 |
| `SET_BUZZER` | 0x09 | 1B = 0关/1开 | 蜂鸣器开关 |

### 5.2 响应格式

**响应类型 = 命令类型 | 0x80**：

```
命令 Type 0x03  →  响应 Type 0x83
```

**响应载荷首字节 = 状态码**，其后才是数据：

```
[0xAA][0x55][Type|0x80][Len][状态码][数据...][CRC8]
```

### 5.3 状态码

| 状态码 | 值 | 含义 |
|--------|----|------|
| `OK` | 0x00 | 成功 |
| `ERR_LEN` | 0x01 | 载荷长度不匹配 |
| `ERR_PARAM` | 0x02 | 参数非法（越界） |
| `ERR_UNKNOWN` | 0x03 | 未知命令 |

### 5.4 典型交互示例

```
查询状态（一次拿模式 + 角度）:
  发: AA 55 07 00 [CRC]
  收: AA 55 87 06 00 [模式1B] [角度4B float] [CRC]

设置预设 FINE_24:
  发: AA 55 02 01 02 [CRC]
  收: AA 55 82 01 00 [CRC]        // ACK
```

---

## 6. 收发数据流

### 6.1 接收（USB 中断上下文）

```
USB OUT 中断
  └→ usbd_cdc_if.c: CDC_Receive_FS(buf, len)
       └→ app_usb.c: Usb_OnReceive(buf, len)
            └→ app_usb_protocol.c: UsbProto_HandleRx
                 ├→ 逐字节喂状态机 → CRC 校验
                 └→ UsbProto_Dispatch() → 按命令执行
                      ├→ SET_CONFIG     → App_Knob_SetConfig()
                      ├→ GET_ANGLE      → BSP_KnobEncoder_GetAngle()
                      ├→ SET_LIMIT_MODE → KnobLimit_SetConfig()
                      ├→ GET_STATE      → AppMode_GetMode() + 编码器角度
                      ├→ SET_MODE       → AppMode_SetMode()
                      └→ SET_BUZZER     → App_Knob_SetBuzzerEnabled()
                  响应帧 → UsbProto_SendFrame() → CDC_Transmit_FS()
```

> 所有解析在 USB 中断上下文完成：帧短（≤260B）、CRC8 查表为微秒级，
> 无阻塞操作，可接受。

### 6.2 发送（异步注意事项）

```c
uint8_t UsbProto_SendFrame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    s_tx_buf[0] = USB_PROTO_SYNC_AA;
    s_tx_buf[1] = USB_PROTO_SYNC_55;
    s_tx_buf[2] = type;
    s_tx_buf[3] = len;
    if (len > 0) memcpy(&s_tx_buf[4], payload, len);
    s_tx_buf[4 + len] = UsbProto_Crc8Update(0x00, &s_tx_buf[2], 2 + len);
    return CDC_Transmit_FS(s_tx_buf, 5 + len) == USBD_OK;
}
```

**关键**：`CDC_Transmit_FS` 是**异步发送**——只把缓冲指针交给 USB 端点，
硬件稍后慢慢发完。所以：
- 发送缓冲 `s_tx_buf[]` 必须是**持久 static**，不能用局部数组（函数返回就没了）
- 上一帧没发完（返回 `USBD_BUSY`）时，**当前帧被丢弃**（PC 轮询慢不会触发）

---

## 7. 可靠性设计

| 机制 | 说明 |
|------|------|
| **跨包重组** | 状态机 static 保持，USB 分包/粘包都能正确解析 |
| **CRC8 校验** | 检测传输噪声，错误帧整帧丢弃 |
| **重同步** | 收到垃圾字节后，靠 `0xAA 0x55` 重新定位帧头 |
| **状态码** | 命令执行结果（长度错/参数越界/未知命令）明确上报 |
| **命令-应答** | PC 发命令必回响应，天然带 ACK 语义 |

---

## 8. 如何新增一条命令

新增一条命令只需改 **3 处**（以新增 `CMD_GET_TEMPERATURE = 0x0B` 为例）：

**① 枚举加命令类型**（`app_usb_protocol.h`）：

```c
    USB_PROTO_CMD_GET_TEMPERATURE = 0x0B,  ///< 查询温度：载荷为空
```

**② 分发 switch 加分支**（`app_usb_protocol.c` 的 `UsbProto_Dispatch()`）：

```c
        case USB_PROTO_CMD_GET_TEMPERATURE:
            UsbProto_HandleGetTemperature();
            break;
```

**③ 实现处理函数**（仿照 `UsbProto_HandleGetAngle`）：

```c
static void UsbProto_HandleGetTemperature(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;
    if (s_rx_len != 0)   // 校验载荷长度（无载荷命令要求 len==0）
    {
        status = USB_PROTO_STATUS_ERR_LEN;
        UsbProto_SendResponse(USB_PROTO_CMD_GET_TEMPERATURE, status, NULL, 0);
        return;
    }
    float temp = ReadTemperature();   // 业务函数
    UsbProto_SendResponse(USB_PROTO_CMD_GET_TEMPERATURE, status,
                          (const uint8_t *)&temp, sizeof(temp));
}
```

**约定**：
- 命令 Type 用 0x02~0x7F（0x00/0x01 保留，0x80+ 是响应标志位）
- 响应载荷首字节**必须**是状态码
- CRC 交给 `UsbProto_SendFrame` 自动计算，无需手算

---

## 附录：完整调用链

```
系统上电
  └→ MX_USB_DEVICE_Init()        ← CubeMX 初始化 USB-CDC

--- PC 发命令：中断上下文 ---
USB OUT 中断
  └→ CDC_Receive_FS(buf, len)
       └→ Usb_OnReceive(buf, len)          (app_usb.c)
            └→ UsbProto_HandleRx(buf, len) (app_usb_protocol.c)
                 ├→ 状态机逐字节解析
                 ├→ CRC8 校验
                 ├→ UsbProto_Dispatch()
                 │    └→ 业务处理函数 → App 层 API
                 └→ UsbProto_SendResponse() → SendFrame → CDC_Transmit_FS

--- 帧格式回顾 ---
[0xAA][0x55][Type][Len][Payload 0~255B][CRC8]
```
