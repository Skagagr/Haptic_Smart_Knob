/**
 * @file    app_usb.c
 * @brief   USB-CDC 应用层 — 收包处理
 * @details 当前为 echo 测试：收到什么回显什么，验证双向通信链路。
 *          后续块2 协议解析将在此实现（替换纯 echo）。
 * @version 1.0.0
 * @date    2026/8/3
 */
#include "app_usb.h"
#include "usbd_cdc_if.h"

void Usb_OnReceive(uint8_t *buf, uint32_t len)
{
    CDC_Transmit_FS(buf, len);
}
