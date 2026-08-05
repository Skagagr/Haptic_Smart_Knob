/**
 * @file    app_usb_protocol.c
 * @brief   USB-CDC 双向通信协议 — 解析状态机 + 命令分发
 * @details 帧格式： [0xAA][0x55][Type][Len][Payload][CRC8]
 *          接收状态机：WAIT_AA → WAIT_55 → TYPE → LEN → PAYLOAD → CRC → 分发
 *          所有解析均在 USB 中断上下文完成（帧短、CRC8 查表为微秒级操作）。
 * @version 1.0.0
 * @date    2026/8/4
 */
#include "app_usb_protocol.h"
#include "app_knob.h"
#include "app_knob_limit.h"
#include "app_mode.h"
#include "bsp_knob_encoder.h"
#include "usbd_cdc_if.h"
#include <string.h>

// =============== CRC8 查表（多项式 0x07，初值 0x00，MSB 先行） ===============
static const uint8_t s_crc8_table[256] = {
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
    0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
    0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
    0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
    0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
    0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
    0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
    0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
    0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3,
};

// ================================ 解析状态机  ================================
typedef enum
{
    PARSER_WAIT_AA,     ///< 等待同步头第 1 字节
    PARSER_WAIT_55,     ///< 等待同步头第 2 字节
    PARSER_TYPE,        ///< 已收类型
    PARSER_LEN,         ///< 已收长度
    PARSER_PAYLOAD,     ///< 接收载荷
    PARSER_CRC,         ///< 已收校验
} ParserState_t;

static ParserState_t s_state = PARSER_WAIT_AA;
static uint8_t s_rx_type;                           ///< 帧类型
static uint8_t s_rx_len;                            ///< 载荷长度
static uint8_t s_rx_payload[USB_PROTO_MAX_PAYLOAD]; ///< 载荷缓冲（跨包保持）
static uint8_t s_rx_index;                          ///< 载荷写入游标
static uint8_t s_rx_crc;                            ///< 收到的校验字节

// ================================= 发送缓冲  =================================
// CDC_Transmit_FS 是异步发送（指针交给 USB 端点），缓冲必须常驻且
// 在上一帧发送完成前不能被覆盖。busy 时直接丢弃当前帧。
static uint8_t s_tx_buf[USB_PROTO_MAX_PAYLOAD + 4]; ///< 同步头(2)+类型+长度+载荷+CRC

// =============================== 私有函数声明  ===============================
static uint8_t UsbProto_Crc8Update(uint8_t crc, const uint8_t *data, uint8_t len);
static void UsbProto_Dispatch(void);
static void UsbProto_HandleSetConfig(void);
static void UsbProto_HandleGetAngle(void);
static void UsbProto_HandleSetLimitMode(void);
static void UsbProto_HandleGetState(void);
static void UsbProto_HandleSetMode(void);
static void UsbProto_HandleSetBuzzer(void);
static void UsbProto_SendResponse(uint8_t cmd, uint8_t status,
                                  const uint8_t *data, uint8_t data_len);

// =================================== CRC8  ===================================

/**
 * @brief 增量计算 CRC8
 * @details CRC-8，多项式 0x07，初值 0x00，MSB 先行，不取反。
 *
 *          计算原理：
 *            crc 初始 = 0x00
 *            对每个字节：crc = crc ^ byte，再查表 table[crc] 得到新 crc
 *            等价于逐位运算：crc 最高位为 1 时左移一位并异或 0x07，
 *            否则只左移一位（8 次后对 crc 截断到 8 位）
 *
 *          帧内 CRC 覆盖范围：Type + Len + Payload（不含同步头）。
 *          发送端计算：crc = Crc8Update(0x00, [Type Len Payload...])
 *          接收端用同一算法重新计算，与帧尾校验字节比对，
 *          不一致说明数据被破坏，整帧丢弃。
 *
 *          示例（查询角度帧 AA 55 03 00 3F，帧尾 0x3F 即 CRC）：
 *            Crc8Update(0x00, {0x03, 0x00}) = 0x3F
 *
 *          支持分段计算：传入上次的结果继续累加，
 *          例如先算 Type 再接着算 Len、Payload。
 * @param crc  初始或上次计算的 CRC 值
 * @param data 数据指针
 * @param len  数据长度
 * @return 计算后的 CRC 值
 */
static uint8_t UsbProto_Crc8Update(uint8_t crc, const uint8_t *data, uint8_t len)
{
    while (len--)
    {
        crc = s_crc8_table[crc ^ *data++];
    }
    return crc;
}

// =================================== 接收  ===================================

/**
 * @brief USB 接收入口，逐字节喂给解析状态机
 * @param buf 收到的数据缓冲
 * @param len 收到的字节数
 */
void UsbProto_HandleRx(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        const uint8_t b = buf[i];

        switch (s_state)
        {
            case PARSER_WAIT_AA:
                if (b == USB_PROTO_SYNC_AA)
                {
                    s_state = PARSER_WAIT_55;
                }
                break;

            case PARSER_WAIT_55:
                if (b == USB_PROTO_SYNC_55)
                {
                    s_state = PARSER_TYPE;
                }
                else if (b == USB_PROTO_SYNC_AA)
                {
                    s_state = PARSER_WAIT_55;   // 连续 AA 时留在等 55
                }
                else
                {
                    s_state = PARSER_WAIT_AA;
                }
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
                if (s_rx_index >= s_rx_len)
                {
                    s_state = PARSER_CRC;
                }
                break;

            case PARSER_CRC:
            {
                s_rx_crc = b;

                // CRC 校验覆盖 Type+Len+Payload
                uint8_t crc = UsbProto_Crc8Update(0x00, &s_rx_type, 1);
                crc = UsbProto_Crc8Update(crc, &s_rx_len, 1);
                crc = UsbProto_Crc8Update(crc, s_rx_payload, s_rx_len);

                if (crc == s_rx_crc)
                {
                    UsbProto_Dispatch();
                }
                // CRC 错误：整帧丢弃
                s_state = PARSER_WAIT_AA;
                break;
            }

            default:
                s_state = PARSER_WAIT_AA;
                break;
        }
    }
}

// ================================= 命令分发  =================================

/**
 * @brief 分发已校验通过的完整帧
 */
static void UsbProto_Dispatch(void)
{
    switch (s_rx_type)
    {
        case USB_PROTO_CMD_SET_CONFIG:
            UsbProto_HandleSetConfig();
            break;

        case USB_PROTO_CMD_GET_ANGLE:
            UsbProto_HandleGetAngle();
            break;

        case USB_PROTO_CMD_SET_LIMIT_MODE:
            UsbProto_HandleSetLimitMode();
            break;

        case USB_PROTO_CMD_GET_STATE:
            UsbProto_HandleGetState();
            break;

        case USB_PROTO_CMD_SET_MODE:
            UsbProto_HandleSetMode();
            break;

        case USB_PROTO_CMD_SET_BUZZER:
            UsbProto_HandleSetBuzzer();
            break;

        default:
            UsbProto_SendResponse(s_rx_type, USB_PROTO_STATUS_ERR_UNKNOWN, NULL, 0);
            break;
    }
}

/**
 * @brief 设置命令：payload[0] = preset (0~4)，调用 App_Knob_SetConfig
 */
static void UsbProto_HandleSetConfig(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;

    if (s_rx_len != 1)
    {
        status = USB_PROTO_STATUS_ERR_LEN;
    }
    else if (s_rx_payload[0] > (uint8_t)KNOB_PRESET_SMOOTH)
    {
        status = USB_PROTO_STATUS_ERR_PARAM;
    }
    else
    {
        KnobConfig_t cfg;
        App_Knob_GetConfig(&cfg);
        cfg.preset = (KnobPreset_t)s_rx_payload[0];
        App_Knob_SetConfig(&cfg);
    }

    UsbProto_SendResponse(USB_PROTO_CMD_SET_CONFIG, status, NULL, 0);
}

/**
 * @brief 查询命令：回传当前角度（4B float，小端）
 */
static void UsbProto_HandleGetAngle(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;

    if (s_rx_len != 0)
    {
        status = USB_PROTO_STATUS_ERR_LEN;
        UsbProto_SendResponse(USB_PROTO_CMD_GET_ANGLE, status, NULL, 0);
        return;
    }

    float angle = BSP_KnobEncoder_GetAngle();
    UsbProto_SendResponse(USB_PROTO_CMD_GET_ANGLE, status,
                          (const uint8_t *)&angle, sizeof(angle));
}

/**
 * @brief 设置限位模式：payload[0] = 0关闭/1单边/2双边
 * @details 复用 KnobLimit_GetConfig/SetConfig 运行时切换限位模式，
 *          用于上位机"音量控制"等需要无限旋转的场景（关闭限位）。
 */
static void UsbProto_HandleSetLimitMode(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;

    if (s_rx_len != 1)
    {
        status = USB_PROTO_STATUS_ERR_LEN;
    }
    else if (s_rx_payload[0] > KNOB_LIMIT_MODE_DUAL)
    {
        status = USB_PROTO_STATUS_ERR_PARAM;
    }
    else
    {
        Knob_LimitConfig_t lim;
        KnobLimit_GetConfig(&lim);
        lim.mode = (Knob_LimitMode_t)s_rx_payload[0];
        KnobLimit_SetConfig(&lim);
    }

    UsbProto_SendResponse(USB_PROTO_CMD_SET_LIMIT_MODE, status, NULL, 0);
}

/**
 * @brief 查询状态：返回 [模式1B][角度4B float 小端]
 * @details 上位机 50ms 轮询一次即可同时拿到控制模式和角度，
 *          替代分别发 GET_ANGLE + GET_MODE 的两次往返。
 */
static void UsbProto_HandleGetState(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;

    if (s_rx_len != 0)
    {
        status = USB_PROTO_STATUS_ERR_LEN;
        UsbProto_SendResponse(USB_PROTO_CMD_GET_STATE, status, NULL, 0);
        return;
    }

    // 载荷 = [模式1B][角度4B float]
    uint8_t payload[5];
    payload[0] = (uint8_t)AppMode_GetMode();
    float angle = BSP_KnobEncoder_GetAngle();
    memcpy(&payload[1], &angle, sizeof(angle));

    UsbProto_SendResponse(USB_PROTO_CMD_GET_STATE, status, payload, sizeof(payload));
}

/**
 * @brief 设置控制模式：payload[0] = 0空闲/1音量/2亮度
 * @details 上位机 UI 勾选音量/亮度时同步到下位机，
 *          下位机切换模式并点亮对应 LED。
 */
static void UsbProto_HandleSetMode(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;

    if (s_rx_len != 1)
    {
        status = USB_PROTO_STATUS_ERR_LEN;
    }
    else if (s_rx_payload[0] > KNOB_STATE_MODE_BRIGHTNESS)
    {
        status = USB_PROTO_STATUS_ERR_PARAM;
    }
    else
    {
        AppMode_SetMode((KnobStateMode_t)s_rx_payload[0]);
    }

    UsbProto_SendResponse(USB_PROTO_CMD_SET_MODE, status, NULL, 0);
}

/**
 * @brief 设置蜂鸣器开关：payload[0] = 0关/1开
 * @details 由上位机勾选框控制；关闭时卡位/限位蜂鸣均静音
 */
static void UsbProto_HandleSetBuzzer(void)
{
    uint8_t status = USB_PROTO_STATUS_OK;

    if (s_rx_len != 1)
    {
        status = USB_PROTO_STATUS_ERR_LEN;
    }
    else if (s_rx_payload[0] > 1)
    {
        status = USB_PROTO_STATUS_ERR_PARAM;
    }
    else
    {
        App_Knob_SetBuzzerEnabled((int)s_rx_payload[0]);
    }

    UsbProto_SendResponse(USB_PROTO_CMD_SET_BUZZER, status, NULL, 0);
}

// =================================== 发送  ===================================

/**
 * @brief 组响应帧：[status][data...]
 * @param cmd      原命令类型（内部会自动 | 0x80）
 * @param status   状态码
 * @param data     附加数据（如角度 float），可 NULL
 * @param data_len 附加数据长度
 */
static void UsbProto_SendResponse(uint8_t cmd, uint8_t status,
                                  const uint8_t *data, uint8_t data_len)
{
    uint8_t resp_len = 1 + data_len;

    s_rx_payload[0] = status;
    if (data_len > 0)
    {
        memcpy(&s_rx_payload[1], data, data_len);
    }
    UsbProto_SendFrame(cmd | USB_PROTO_RESP_FLAG, s_rx_payload, resp_len);
}

/**
 * @brief 按帧格式发送一帧数据
 * @param type    帧类型
 * @param payload 载荷指针，可为 NULL（len=0 时）
 * @param len     载荷长度 (0~255)
 * @return 1 = 已递交发送，0 = 发送忙或参数非法
 */
uint8_t UsbProto_SendFrame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (len > USB_PROTO_MAX_PAYLOAD)
    {
        return 0;
    }

    s_tx_buf[0] = USB_PROTO_SYNC_AA;
    s_tx_buf[1] = USB_PROTO_SYNC_55;
    s_tx_buf[2] = type;
    s_tx_buf[3] = len;
    if (len > 0)
    {
        memcpy(&s_tx_buf[4], payload, len);
    }
    s_tx_buf[4 + len] = UsbProto_Crc8Update(0x00, &s_tx_buf[2], 2 + len);

    return CDC_Transmit_FS(s_tx_buf, 5 + len) == USBD_OK;
}
