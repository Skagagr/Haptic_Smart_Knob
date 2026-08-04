# 力反馈旋钮上位机（Haptic_Knob_Host）

基于 **C# WinForms (.NET 8)** 的 PC 端上位机，通过 USB-CDC 虚拟串口与 STM32 固件通信，
验证与调试固件的双向通信协议（帧头 + 长度 + CRC8 校验）。

---

## 已实现功能

### 串口通信（USB-CDC）
- 打开 / 关闭串口（串口号当前硬编码为 COM7）

### 查询角度
- 手动点击发送 `GET_ANGLE` 命令帧（AA 55 03 00 3F）
- 接收并显示原始字节（hex 格式）

### 收发日志
- 时间戳 + hex 帧显示（发送帧与接收帧）

---

## 软件架构

### 分层设计

```
┌────────────────────────────────────────────┐
│  Form1.cs          界面层 + 串口逻辑       │
│  - 打开/关闭串口 / 手动发送查询帧           │
│  - 收包 hex 日志显示                       │
└────────────────────────────────────────────┘
```

### 文件结构

```
Haptic_Knob_Host/
├── Form1.cs                  # 主窗口（界面 + 事件逻辑）
├── Form1.Designer.cs         # 设计器布局（VS 自动生成）
├── Form1.resx
├── Program.cs                # 程序入口
└── Haptic_Knob_Host.csproj   # 工程文件（.NET 8 + System.IO.Ports）
```

### 协议说明

帧格式与固件 `app_usb_protocol.h` 一致：

```
[0xAA][0x55][Type][Len][Payload 0~255B][CRC8]
```

- 同步头：`0xAA 0x55` 双字节
- CRC8：多项式 0x07，覆盖 Type+Len+Payload
- 响应类型 = 命令类型 | 0x80，响应载荷首字节为状态码

---

## 快速开始

1. Visual Studio 打开 `Haptic_Knob_Host.sln`（.NET 8 桌面开发工作负载）
2. 旋钮 USB 插入电脑，在设备管理器确认虚拟串口号（如 COM7）
3. 若串口号不是 COM7，修改 `Form1.cs` 中 `new SerialPort("COM7", 115200)`
4. F5 运行 → 打开串口 → 查询角度 → 日志区查看响应帧 hex

> 依赖 NuGet 包：`System.IO.Ports`

---

## 更新日志

### v0.1.0 (2026-08-04)
- 初始版本：基础串口收发（打开/关闭串口 + 手动查询 + hex 日志）
