/**
 * @file    app_usb.h
 * @brief   USB-CDC 应用层 — 对外接口
 * @details 定义 USB 收包处理入口。由 USB_DEVICE/App/usbd_cdc_if.c 的
 *          CDC_Receive_FS 回调转发调用，业务逻辑全部放这里，
 *          保持 CubeMX 生成代码干净。
 * @version 2.0.0
 * @date    2026/8/4
 */
#ifndef APP_USB_H
#define APP_USB_H

#include <stdint.h>

/**
 * @brief USB 收包入口（由 usbd_cdc_if.c 的 CDC_Receive_FS 转发调用）
 * @param buf 收到的数据缓冲（USB 静态缓冲，处理完即被下一包覆盖）
 * @param len 收到的字节数
 * @details USB 收包是中断/事件驱动的，主循环轮询不到，必须经过 CDC_Receive_FS 回调。
 *          收到字节后转交协议层 UsbProto_HandleRx 做帧解析，保持生成文件干净。
 */
void Usb_OnReceive(uint8_t *buf, uint32_t len);

#endif
