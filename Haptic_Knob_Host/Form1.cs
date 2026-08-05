using System.IO.Ports;
using Haptic_Knob_Host.Protocol;
using Haptic_Knob_Host.Volume;

namespace Haptic_Knob_Host
{
    /// <summary>旋钮控制模式：旋钮转动时步进调节的目标（可扩展）</summary>
    public enum KnobControlMode
    {
        None = 0,       // 普通旋钮：仅跟随角度，不调节任何系统参数
        Volume,         // 音量控制
        Brightness,     // 亮度控制
    }

    public partial class Form1 : Form
    {
        private SerialPort serialPort = new SerialPort("COM7", 115200);
        private KnobFrameParser parser = new KnobFrameParser();

        private VolumeController volume = new VolumeController();
        private BrightnessController brightness = new BrightnessController();
        private KnobControlMode controlMode = KnobControlMode.None;  // 当前控制模式
        private bool hasLastAngle;          // 第一帧只记录基准，不产生转动
        private float lastAngle;            // 上一次角度
        private float angleAccumulator;     // 角度差累加器
        private float detentAngle = 15f;    // 每个卡位对应的角度，随预设变化（默认 FINE_24）

        public Form1()
        {
            InitializeComponent();
            serialPort.DataReceived += SerialPort_DataReceived;
            chkVolumeMode.CheckedChanged += chkVolumeMode_CheckedChanged;
            chkBrightnessMode.CheckedChanged += chkBrightnessMode_CheckedChanged;
            pollTimer.Start();
            FormClosing += Form1_FormClosing;
            UpdateVolumeUI();
            UpdateBrightnessUI();
        }

        #region 串口开关

        /// <summary>打开/关闭串口，按钮文字同步切换</summary>
        private void btnOpen_Click(object? sender, EventArgs e)
        {
            if (!serialPort.IsOpen)
            {
                serialPort.Open();
                btnOpen.Text = "关闭串口";
                ApplyDefaultPreset();       // 连接后切到默认预设 FINE_24
            }
            else
            {
                serialPort.Close();
                btnOpen.Text = "打开串口";
            }
        }

        /// <summary>默认预设 FINE_24（2）：通知固件并同步卡位角度</summary>
        private void ApplyDefaultPreset()
        {
            Send(KnobProtocol.BuildSetConfig(2), "设置默认预设 FINE_24");
            detentAngle = DetentAngleFor(2);
        }

        #endregion

        #region 定时轮询

        /// <summary>周期查询角度（Timer 在主线程触发，可直接操作控件）；轮询不记日志避免刷屏</summary>
        private void pollTimer_Tick(object? sender, EventArgs e)
        {
            SendSilent(KnobProtocol.BuildGetAngle());
        }

        #endregion

        #region 命令发送

        /// <summary>发送命令帧到串口并记日志（未打开时忽略）</summary>
        private void Send(byte[] frame, string desc)
        {
            if (!serialPort.IsOpen) return;
            serialPort.Write(frame, 0, frame.Length);
            Log($"--> {desc}: {BitConverter.ToString(frame)}");
        }

        /// <summary>静默发送命令帧（不记日志，用于高频轮询）</summary>
        private void SendSilent(byte[] frame)
        {
            if (!serialPort.IsOpen) return;
            serialPort.Write(frame, 0, frame.Length);
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
                HandleAngleResponse(frame);     // 角度已在 UI 显示，不记日志
            }
            else if (type == (byte)((byte)KnobCmd.SetConfig | KnobProtocol.RespFlag))
            {
                HandleConfigResponse(frame);
            }
            else if (type == (byte)((byte)KnobCmd.SetLimitMode | KnobProtocol.RespFlag))
            {
                HandleLimitModeResponse(frame);
            }
        }

        /// <summary>设置限位模式响应：显示 ACK 结果</summary>
        private void HandleLimitModeResponse(byte[] frame)
        {
            lblAck.Text = (KnobStatus)frame[1] == KnobStatus.Ok ? "限位模式设置成功" : $"失败 (状态 0x{frame[1]:X2})";
        }

        /// <summary>查询角度响应：解析 float 并显示</summary>
        private void HandleAngleResponse(byte[] frame)
        {
            if ((KnobStatus)frame[1] != KnobStatus.Ok || frame.Length < 6) return;
            float angle = BitConverter.ToSingle(frame, 2);
            lblAngle.Text = $"{angle:F1}°";
            DetectRotation(angle);
        }

        /// <summary>按角度差检测转动；仅控制模式开启时累加步进</summary>
        private void DetectRotation(float angle)
        {
            if (!hasLastAngle)               // 第一帧只建立基准
            {
                hasLastAngle = true;
                lastAngle = angle;
                return;
            }
            float delta = angle - lastAngle;
            lastAngle = angle;
            if (controlMode == KnobControlMode.None) return;  // 普通旋钮模式：只跟随角度
            if (detentAngle <= 0f) return;   // 预设异常保护（正常预设均为正角度）

            angleAccumulator += delta;
            while (angleAccumulator >= detentAngle)
            {
                StepControl(1);
                angleAccumulator -= detentAngle;
            }
            while (angleAccumulator <= -detentAngle)
            {
                StepControl(-1);
                angleAccumulator += detentAngle;
            }
            UpdateVolumeUI();
            UpdateBrightnessUI();
        }

        /// <summary>按当前控制模式步进目标参数（新增模式时在此扩展）</summary>
        private void StepControl(int delta)
        {
            switch (controlMode)
            {
                case KnobControlMode.Volume:
                    volume.StepVolume(delta);
                    break;
                case KnobControlMode.Brightness:
                    brightness.StepBrightness(delta);
                    break;
            }
        }

        /// <summary>刷新音量滑块和数值显示</summary>
        private void UpdateVolumeUI()
        {
            int v = volume.VolumePercent;
            if (v < 0) return;
            trackBarVolume.Value = v;
            lblVolume.Text = $"音量: {v}%";
        }

        /// <summary>刷新亮度滑块和数值显示</summary>
        private void UpdateBrightnessUI()
        {
            int v = brightness.BrightnessPercent;
            if (v < 0) return;
            trackBarBrightness.Value = v;
            lblBrightness.Text = $"亮度: {v}%";
        }

        /// <summary>拖动音量滑块时</summary>
        private void trackBarVolume_Scroll(object? sender, EventArgs e)
        {
            volume.SetVolumePercent(trackBarVolume.Value);
            lblVolume.Text = $"音量: {trackBarVolume.Value}%";
        }

        /// <summary>拖动亮度滑块时</summary>
        private void trackBarBrightness_Scroll(object? sender, EventArgs e)
        {
            brightness.SetBrightnessPercent(trackBarBrightness.Value);
            lblBrightness.Text = $"亮度: {trackBarBrightness.Value}%";
        }

        /// <summary>预设编号 → 每卡位角度（SMOOTH 无卡位，用固定 3.6° = 360/100，一圈调完）</summary>
        private static float DetentAngleFor(int preset) => preset switch
        {
            0 => 60f,     // COARSE_6  6卡位/圈
            1 => 30f,     // NORMAL_12
            2 => 15f,     // FINE_24
            3 => 7.5f,    // DENSE_48
            _ => 3.6f,    // SMOOTH（无卡位，按 3.6° 步进）
        };

        /// <summary>设置预设响应：显示 ACK 结果</summary>
        private void HandleConfigResponse(byte[] frame)
        {
            lblAck.Text = (KnobStatus)frame[1] == KnobStatus.Ok ? "设置成功" : $"失败 (状态 0x{frame[1]:X2})";
        }

        #endregion

        #region 手动命令

        /// <summary>预设切换（5 按钮共用，靠 Tag 区分编号）</summary>
        private void btnPreset_Click(object? sender, EventArgs e)
        {
            if (sender is not Button btn) return;
            int preset = Convert.ToInt32(btn.Tag);
            Send(KnobProtocol.BuildSetConfig((byte)preset), $"设置预设 {preset}");
            detentAngle = DetentAngleFor(preset);
        }

        /// <summary>音量控制模式：勾选 = 无限旋转 + 步进音量；与亮度模式互斥</summary>
        private void chkVolumeMode_CheckedChanged(object? sender, EventArgs e)
        {
            if (chkVolumeMode.Checked)
            {
                chkBrightnessMode.Checked = false;        // 互斥：切音量时取消亮度
                controlMode = KnobControlMode.Volume;
            }
            else if (controlMode == KnobControlMode.Volume)
            {
                controlMode = KnobControlMode.None;
            }
            UpdateLimitMode();
        }

        /// <summary>亮度控制模式：勾选 = 无限旋转 + 步进亮度；与音量模式互斥</summary>
        private void chkBrightnessMode_CheckedChanged(object? sender, EventArgs e)
        {
            if (chkBrightnessMode.Checked)
            {
                chkVolumeMode.Checked = false;            // 互斥：切亮度时取消音量
                controlMode = KnobControlMode.Brightness;
            }
            else if (controlMode == KnobControlMode.Brightness)
            {
                controlMode = KnobControlMode.None;
            }
            UpdateLimitMode();
        }

        /// <summary>任一控制模式开启 → 关闭限位（无限旋转）；全关 → 恢复双边限位</summary>
        private void UpdateLimitMode()
        {
            bool anyMode = controlMode != KnobControlMode.None;
            var mode = anyMode ? KnobLimitMode.Off : KnobLimitMode.Dual;
            Send(KnobProtocol.BuildSetLimitMode(mode),
                 anyMode ? "进入控制模式（无限旋转）" : "退出控制模式（恢复限位）");
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

        #region 关闭清理

        /// <summary>关闭窗口时释放音量/亮度控制器（COM 对象）</summary>
        private void Form1_FormClosing(object? sender, FormClosingEventArgs e)
        {
            volume.Dispose();
            brightness.Dispose();
        }

        #endregion
    }
}
