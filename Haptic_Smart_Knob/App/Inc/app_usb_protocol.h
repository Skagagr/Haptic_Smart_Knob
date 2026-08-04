/**
 * @file    app_usb_protocol.h
 * @brief   USB-CDC 双向通信协议 — 帧定义与对外接口
 * @details 帧格式： [0xAA][0x55][Type][Len][Payload 0~255B][CRC8]
 *          - 同步头：0xAA 0x55 双字节，互反码抗干扰
 *          - Len：载荷字节数 (0~255)，一帧总长 = 4 + Len
 *            （2 同步头 + 1 类型 + 1 长度 + Len 载荷 + 1 CRC）
 *          - CRC8：多项式 0x07，覆盖 Type+Len+Payload
 *          - 响应类型 = 命令类型 | 0x80，响应载荷首字节为状态码
 *          - 字节序：小端（STM32 与 x86 PC 均为小端）
 *
 * ============ 命令参考（PC 发完整帧，含已算好的 CRC）============
 *
 * 查询角度（载荷为空）
 *   发: AA 55 03 00 3F
 *   收: AA 55 83 05 00 [角度4B] [CRC]     // 00=OK，后4字节小端 float
 *
 * 设置预设（载荷 1B = preset 0~4）
 *   发 COARSE_6 (0):  AA 55 02 01 00 C3
 *   发 NORMAL_12 (1): AA 55 02 01 01 C4
 *   发 FINE_24   (2): AA 55 02 01 02 CD
 *   发 DENSE_48 (3):  AA 55 02 01 03 CA
 *   发 SMOOTH   (4):  AA 55 02 01 04 DF
 *   收: AA 55 82 01 [状态] [CRC]          // 00=OK, 02=preset越界
 *
 * ============ 如何新增一条命令 ============
 * 以新增"查询当前状态"命令 CMD_GET_STATE = 0x04 为例，需要改 3 处：
 *
 * ① 本文件枚举里加命令类型：
 *    USB_PROTO_CMD_GET_STATE = 0x04,      ///< 查询状态
 *
 * ② app_usb_protocol.c 的 UsbProto_Dispatch() 里加分发分支：
 *    case USB_PROTO_CMD_GET_STATE:
 *        UsbProto_HandleGetState();
 *        break;
 *
 * ③ 实现处理函数（参照 UsbProto_HandleGetAngle，它同样位于
 *    app_usb_protocol.c）：
 *    static void UsbProto_HandleGetState(void)
 *    {
 *        uint8_t status = USB_PROTO_STATUS_OK;
 *        // 校验载荷长度（若命令无载荷则要求 len==0）
 *        if (s_rx_len != 0) { ... 回 ERR_LEN ... return; }
 *        // 组响应：[状态][数据...]，状态字节放最前
 *        UsbProto_SendResponse(USB_PROTO_CMD_GET_STATE, status,
 *                              data, data_len);
 *    }
 *
 * 帧 CRC 可用任意在线 CRC-8/MAXIM 工具计算（多项式 0x07），
 * 或直接调 UsbProto_SendFrame 由固件自动生成，无需手算。
 *
 * @version 1.1.0
 * @date    2026/8/4
 */
#ifndef APP_USB_PROTOCOL_H
#define APP_USB_PROTOCOL_H

#include <stdint.h>

// ================================ 帧格式常量  ================================
#define USB_PROTO_SYNC_AA      0xAAu   ///< 同步头第 1 字节
#define USB_PROTO_SYNC_55      0x55u   ///< 同步头第 2 字节
#define USB_PROTO_MAX_PAYLOAD  255u    ///< 最大载荷长度（1 字节长度字段）
#define USB_PROTO_RESP_FLAG    0x80u   ///< 响应标志：类型 | 0x80

// =========================== 命令类型（PC → MCU） ===========================
typedef enum
{
    USB_PROTO_CMD_SET_CONFIG = 0x02,    ///< 设置预设：载荷 1B = preset (0~4)
    USB_PROTO_CMD_GET_ANGLE  = 0x03,    ///< 查询角度：载荷为空
} UsbProtoCmd_t;

// ========================= 状态码（响应载荷首字节）  =========================
typedef enum
{
    USB_PROTO_STATUS_OK          = 0x00,    ///< 成功
    USB_PROTO_STATUS_ERR_LEN     = 0x01,    ///< 长度字段不匹配
    USB_PROTO_STATUS_ERR_PARAM   = 0x02,    ///< 参数非法（preset 越界等）
    USB_PROTO_STATUS_ERR_UNKNOWN = 0x03,    ///< 未知命令
} UsbProtoStatus_t;

/**
 * @brief USB 接收入口（由 app_usb.c 的 Usb_OnReceive 转发调用）
 * @param buf 收到的数据缓冲
 * @param len 收到的字节数
 * @details 逐字节喂给解析状态机，状态跨 USB 包保持。
 *          完整帧解析 + CRC 校验通过后执行命令并回发响应。
 */
void UsbProto_HandleRx(const uint8_t *buf, uint32_t len);

/**
 * @brief 按帧格式发送一帧数据
 * @param type    帧类型（命令或响应）
 * @param payload 载荷指针，可为 NULL（len=0 时）
 * @param len     载荷长度 (0~255)
 * @return 1 = 已递交 USB 发送，0 = 发送忙或参数非法（帧被丢弃）
 * @details 内部用持久静态缓冲，异步发送期间不会被复用。
 */
uint8_t UsbProto_SendFrame(uint8_t type, const uint8_t *payload, uint8_t len);

#endif
