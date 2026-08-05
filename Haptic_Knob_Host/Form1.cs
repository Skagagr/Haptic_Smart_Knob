using System.IO.Ports;
using Haptic_Knob_Host.Protocol;

namespace Haptic_Knob_Host
{
    public partial class Form1 : Form
    {
        private SerialPort serialPort = new SerialPort("COM7", 115200);
        private KnobFrameParser parser = new KnobFrameParser();

        public Form1()
        {
            InitializeComponent();
            serialPort.DataReceived += SerialPort_DataReceived;
            pollTimer.Start();
        }

        #region 串口开关

        /// <summary>打开/关闭串口，按钮文字同步切换</summary>
        private void btnOpen_Click(object? sender, EventArgs e)
        {
            if (!serialPort.IsOpen)
            {
                serialPort.Open();
                btnOpen.Text = "关闭串口";
            }
            else
            {
                serialPort.Close();
                btnOpen.Text = "打开串口";
            }
        }

        #endregion

        #region 定时轮询

        /// <summary>周期查询角度（Timer 在主线程触发，可直接操作控件）</summary>
        private void pollTimer_Tick(object? sender, EventArgs e)
        {
            Send(KnobProtocol.BuildGetAngle());
        }

        #endregion

        #region 命令发送

        /// <summary>发送一帧命令到串口（未打开时忽略）</summary>
        private void Send(byte[] frame)
        {
            if (!serialPort.IsOpen) return;
            serialPort.Write(frame, 0, frame.Length);
            Log($"--> 发送: {BitConverter.ToString(frame)}");
        }

        /// <summary>发送一帧并附带说明文字</summary>
        private void Send(byte[] frame, string desc)
        {
            if (!serialPort.IsOpen) return;
            serialPort.Write(frame, 0, frame.Length);
            Log($"--> {desc}: {BitConverter.ToString(frame)}");
        }

        #endregion

        #region 接收与解析

        /// <summary>串口收包回调（后台线程），解析后切回主线程更新界面</summary>
        private void SerialPort_DataReceived(object? sender, SerialDataReceivedEventArgs e)
        {
            BeginInvoke(new Action(() =>
            {
                int count = serialPort.BytesToRead;
                byte[] buffer = new byte[count];
                serialPort.Read(buffer, 0, count);
                Log($"<-- 接收: {BitConverter.ToString(buffer)}");

                foreach (byte b in buffer)
                {
                    byte[]? frame = parser.Feed(b);
                    if (frame != null) HandleFrame(frame);
                }
            }));
        }

        /// <summary>按帧类型分发到对应处理</summary>
        private void HandleFrame(byte[] frame)
        {
            byte type = frame[0];
            if (type == (byte)((byte)KnobCmd.GetAngle | KnobProtocol.RespFlag))
            {
                HandleAngleResponse(frame);
            }
            else if (type == (byte)((byte)KnobCmd.SetConfig | KnobProtocol.RespFlag))
            {
                HandleConfigResponse(frame);
            }
        }

        /// <summary>查询角度响应：解析 float 并显示</summary>
        private void HandleAngleResponse(byte[] frame)
        {
            if ((KnobStatus)frame[1] != KnobStatus.Ok || frame.Length < 6) return;
            float angle = BitConverter.ToSingle(frame, 2);
            lblAngle.Text = $"{angle:F1}°";
        }

        /// <summary>设置预设响应：显示 ACK 结果</summary>
        private void HandleConfigResponse(byte[] frame)
        {
            lblAck.Text = (KnobStatus)frame[1] == KnobStatus.Ok ? "设置成功" : $"失败 (状态 0x{frame[1]:X2})";
        }

        #endregion

        #region 手动命令

        /// <summary>手动查询一次角度</summary>
        private void btnQuery_Click(object? sender, EventArgs e)
        {
            Send(KnobProtocol.BuildGetAngle());
        }

        /// <summary>预设切换（5 按钮共用，靠 Tag 区分编号）</summary>
        private void btnPreset_Click(object? sender, EventArgs e)
        {
            if (sender is not Button btn) return;
            int preset = Convert.ToInt32(btn.Tag);
            Send(KnobProtocol.BuildSetConfig((byte)preset), $"设置预设 {preset}");
        }

        #endregion

        #region 日志

        /// <summary>带时间戳追加一行日志，并滚动到底部</summary>
        private void Log(string line)
        {
            txtLog.Text += $"[{DateTime.Now:HH:mm:ss.fff}] {line}" + Environment.NewLine;
            txtLog.SelectionStart = txtLog.Text.Length;
            txtLog.ScrollToCaret();
        }

        #endregion
    }
}
