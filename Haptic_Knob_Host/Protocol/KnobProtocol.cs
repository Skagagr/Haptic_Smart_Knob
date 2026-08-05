// ============================================================
// 协议层：CRC8 校验 + 帧构建 + 帧解析
// 帧格式：[AA][55][Type][Len][Payload 0~255B][CRC8]
// CRC8 多项式 0x07，覆盖 Type+Len+Payload
// ============================================================
namespace Haptic_Knob_Host.Protocol
{
    #region 命令类型与状态码

    /// <summary>命令类型（与固件 app_usb_protocol.h 对齐）</summary>
    public enum KnobCmd : byte
    {
        SetConfig     = 0x02,   // 设置预设
        GetAngle      = 0x03,   // 查询角度
        SetLimitMode  = 0x04,   // 设置限位模式
    }

    /// <summary>限位模式（与固件 Knob_LimitMode_t 对齐）</summary>
    public enum KnobLimitMode : byte
    {
        Off    = 0,   // 关闭限位（无限旋转）
        Single = 1,   // 单边限位
        Dual   = 2,   // 双边限位
    }

    /// <summary>状态码（响应载荷首字节）</summary>
    public enum KnobStatus : byte
    {
        Ok         = 0x00,
        ErrLen     = 0x01,
        ErrParam   = 0x02,
        ErrUnknown = 0x03,
    }

    #endregion

    #region 协议编解码

    public static class KnobProtocol
    {
        /// <summary>响应类型标志：响应 = 命令 | 0x80</summary>
        public const byte RespFlag = 0x80;

        #region CRC8 校验

        /// <summary>增量计算 CRC8（多项式 0x07，与固件算法一致）</summary>
        public static byte Crc8Update(byte crc, byte[] data)
        {
            foreach (byte b in data)
            {
                crc ^= b;
                for (int i = 0; i < 8; i++)
                {
                    crc = (byte)((crc & 0x80) != 0 ? (crc << 1) ^ 0x07 : (crc << 1));
                }
            }
            return crc;
        }

        #endregion

        #region 帧构建

        /// <summary>组装一帧：[AA][55][Type][Len][Payload][CRC]</summary>
        public static byte[] BuildFrame(byte type, byte[] payload)
        {
            byte[] frame = new byte[4 + payload.Length + 1];
            frame[0] = 0xAA;
            frame[1] = 0x55;
            frame[2] = type;
            frame[3] = (byte)payload.Length;
            Array.Copy(payload, 0, frame, 4, payload.Length);
            frame[4 + payload.Length] = Crc8Update(0, HeaderPlusPayload(type, payload));
            return frame;
        }

        /// <summary>拼出 CRC 计算范围（Type+Len+Payload，不含同步头）</summary>
        private static byte[] HeaderPlusPayload(byte type, byte[] payload)
        {
            byte[] range = new byte[2 + payload.Length];
            range[0] = type;
            range[1] = (byte)payload.Length;
            Array.Copy(payload, 0, range, 2, payload.Length);
            return range;
        }

        #endregion

        #region 具体命令

        /// <summary>查询角度命令帧</summary>
        public static byte[] BuildGetAngle() => BuildFrame((byte)KnobCmd.GetAngle, Array.Empty<byte>());

        /// <summary>设置预设命令帧（preset 0~4）</summary>
        public static byte[] BuildSetConfig(byte preset) => BuildFrame((byte)KnobCmd.SetConfig, new[] { preset });

        /// <summary>设置限位模式命令帧（0关闭/1单边/2双边）</summary>
        public static byte[] BuildSetLimitMode(KnobLimitMode mode) => BuildFrame((byte)KnobCmd.SetLimitMode, new[] { (byte)mode });

        #endregion
    }

    #endregion

    #region 帧解析状态机

    /// <summary>逐字节重组完整帧，CRC 通过才返回（与固件解析逻辑一致）</summary>
    public class KnobFrameParser
    {
        private enum State { WaitAA, Wait55, Type, Len, Payload, Crc }

        private State _state = State.WaitAA;
        private byte _type;
        private byte _len;
        private readonly byte[] _payload = new byte[255];
        private int _index;

        /// <summary>喂入一个字节；攒完整帧返回 [类型, 载荷...]，否则 null</summary>
        public byte[]? Feed(byte b)
        {
            switch (_state)
            {
                case State.WaitAA:
                    if (b == 0xAA) _state = State.Wait55;
                    break;

                case State.Wait55:
                    _state = b == 0x55 ? State.Type : (b == 0xAA ? State.Wait55 : State.WaitAA);
                    break;

                case State.Type:
                    _type = b;
                    _state = State.Len;
                    break;

                case State.Len:
                    _len = b;
                    _index = 0;
                    _state = _len > 0 ? State.Payload : State.Crc;
                    break;

                case State.Payload:
                    _payload[_index++] = b;
                    if (_index >= _len) _state = State.Crc;
                    break;

                case State.Crc:
                    _state = State.WaitAA;
                    return CrcMatches(b) ? BuildResult() : null;
            }
            return null;
        }

        /// <summary>重算 Type+Len+Payload 的 CRC，与收到的校验字节比对</summary>
        private bool CrcMatches(byte received)
        {
            byte[] range = new byte[2 + _len];
            range[0] = _type;
            range[1] = _len;
            Array.Copy(_payload, 0, range, 2, _len);
            return KnobProtocol.Crc8Update(0, range) == received;
        }

        /// <summary>组装完整帧 [类型, 载荷...]</summary>
        private byte[] BuildResult()
        {
            byte[] frame = new byte[1 + _len];
            frame[0] = _type;
            Array.Copy(_payload, 0, frame, 1, _len);
            return frame;
        }
    }

    #endregion
}
