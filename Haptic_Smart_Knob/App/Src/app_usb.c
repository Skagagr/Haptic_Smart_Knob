/**
 * @file    app_usb.c
 * @brief   USB-CDC 应用层 — 收包转发
 * @details 作为 USB 传输层与协议解析层之间的桥：把 CDC_Receive_FS
 *          收到的字节原样转给协议模块 UsbProto_HandleRx，由协议层
 *          完成帧解析、校验和命令分发。
 * @version 2.0.0
 * @date    2026/8/4
 */
#include "app_usb.h"
#include "app_usb_protocol.h"

void Usb_OnReceive(uint8_t *buf, uint32_t len)
{
    UsbProto_HandleRx(buf, len);
}
