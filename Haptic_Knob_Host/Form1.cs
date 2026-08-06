using System.IO.Ports;
using Haptic_Knob_Host.Protocol;
using Haptic_Knob_Host.Volume;

namespace Haptic_Knob_Host
{
    public partial class Form1 : Form
    {
        private SerialPort serialPort = new SerialPort();   // 端口名在打开时从下拉框设置
        private KnobFrameParser parser = new KnobFrameParser();

        private VolumeController volume = new VolumeController();
        private BrightnessController brightness = new BrightnessController();
        private KnobControlMode controlMode = KnobControlMode.None;  // 当前控制模式（协议层枚举）
        private bool _syncingMode;              // 正在从固件同步模式到 UI，跳过互斥回调
        private bool hasLastAngle;          // 第一帧只记录基准，不产生转动
        private float lastAngle;            // 上一次角度
        private float angleAccumulator;     // 角度差累加器
        private float detentAngle = 15f;    // 每个卡位对应的角度，随预设变化（默认 FINE_24）
        private DateTime _lastRxTime;       // 最近一次收到有效响应的时间（失联看门狗）

        public Form1()
        {
            InitializeComponent();
            serialPort.DataReceived += SerialPort_DataReceived;
            chkVolumeMode.CheckedChanged += chkVolumeMode_CheckedChanged;
            chkBrightnessMode.CheckedChanged += chkBrightnessMode_CheckedChanged;
            chkBuzzer.CheckedChanged += chkBuzzer_CheckedChanged;
            pollTimer.Start();
            FormClosing += Form1_FormClosing;
            RefreshPorts();                 // 启动时枚举可用串口
            UpdateVolumeUI();
            UpdateBrightnessUI();
        }

        #region 串口开关

        /// <summary>刷新可用串口列表到下拉框（去重）</summary>
        private void RefreshPorts()
        {
            try
            {
                string previous = cmbPort.SelectedItem as string ?? "";
                cmbPort.Items.Clear();
                string[] ports = SerialPort.GetPortNames().Distinct().ToArray();
                cmbPort.Items.AddRange(ports);
                if (cmbPort.Items.Contains(previous))
                {
                    cmbPort.SelectedItem = previous;      // 保持之前选中的端口
                }
                else if (cmbPort.Items.Count > 0)
                {
                    cmbPort.SelectedIndex = 0;
                }
            }
            catch (Exception ex)
            {
                Log($"枚举串口失败：{ex.Message}");
            }
        }

        /// <summary>点击刷新端口按钮：重新枚举可用串口</summary>
        private void btnRefresh_Click(object? sender, EventArgs e)
        {
            RefreshPorts();
        }

        /// <summary>打开/关闭串口，按钮文字同步切换</summary>
        private void btnOpen_Click(object? sender, EventArgs e)
        {
            if (!serialPort.IsOpen)
            {
                if (cmbPort.SelectedItem is not string port || port.Length == 0)
                {
                    MessageBox.Show("请先选择串口，或点击「刷新端口」重新扫描",
                                    "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }
                try
                {
                    serialPort.PortName = port;
                    serialPort.BaudRate = 115200;
                    serialPort.Open();
                }
                catch (Exception ex) when (ex is OperationCanceledException or
                                               IOException or
                                               UnauthorizedAccessException or
                                               InvalidOperationException)
                {
                    MessageBox.Show($"打开串口失败：{ex.Message}",
                                    "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    Log($"打开 {port} 失败：{ex.Message}");
                    return;
                }
                serialPort.DataReceived += SerialPort_DataReceived;   // 断开后重新连接需重新注册
                _lastRxTime = DateTime.Now;    // 初始化看门狗时间戳，避免连接后立即误判失联
                pollTimer.Start();             // 失联断开时轮询被停止，重连需恢复
                btnOpen.Text = "关闭串口";
                ApplyDefaultPreset();       // 连接后切到默认预设 FINE_24
            }
            else
            {
                CloseSerial();                 // 手动关闭（设备可能已失效，需保护）
                btnOpen.Text = "打开串口";
            }
        }

        /// <summary>安全关闭串口并重置运行状态（设备失效/复位时 Close 可能抛异常）</summary>
        private void CloseSerial()
        {
            serialPort.DataReceived -= SerialPort_DataReceived;   // 先注销事件，防止后台回调
            if (serialPort.IsOpen)
            {
                try { serialPort.Close(); }
                catch { /* 设备已失效，忽略关闭异常 */ }
            }

            btnOpen.Text = "打开串口";            // 恢复按钮文字
            pollTimer.Stop();                     // 停止轮询，避免继续 Write 抛异常

            hasLastAngle = false;                 // 重置角度基准，避免重连后首帧误判转动
            angleAccumulator = 0f;
            _lastRxTime = DateTime.MinValue;      // 重置心跳，避免重连前误判
        }

        /// <summary>默认预设 FINE_24（2）：通知固件并同步卡位角度</summary>
        private void ApplyDefaultPreset()
        {
            Send(KnobProtocol.BuildSetConfig(2), "设置默认预设 FINE_24");
            detentAngle = DetentAngleFor(2);
        }

        #endregion

        #region 定时轮询

        /// <summary>周期查询状态（Timer 在主线程触发，可直接操作控件）；轮询不记日志避免刷屏</summary>
        private void pollTimer_Tick(object? sender, EventArgs e)
        {
            // 失联检测：超过阈值没收到任何响应（如 STM32 复位、USB 重枚举），判定连接已失效
            if (serialPort.IsOpen && (DateTime.Now - _lastRxTime).TotalMilliseconds > RxTimeoutMs)
            {
                HandleSerialDisconnect(new IOException("未收到固件响应，判定失联"));
                return;
            }
            SendSilent(KnobProtocol.BuildGetState());
        }

        /// <summary>失联超时阈值 (ms)：轮询 50ms，连续 ~10 次无响应才判定</summary>
        private const double RxTimeoutMs = 500;

        #endregion

        #region 命令发送

        /// <summary>发送命令帧到串口并记日志（未打开时忽略）</summary>
        private void Send(byte[] frame, string desc)
        {
            if (!serialPort.IsOpen) return;
            if (!TryWrite(frame))
            {
                return;   // 串口已断开，状态已由 TryWrite 清理
            }
            Log($"--> {desc}: {BitConverter.ToString(frame)}");
        }

        /// <summary>静默发送命令帧（不记日志，用于高频轮询）</summary>
        private void SendSilent(byte[] frame)
        {
            if (!serialPort.IsOpen) return;
            TryWrite(frame);   // 断开时忽略，不刷日志
        }

        /// <summary>安全写入串口；串口断开/异常时清理连接状态并返回 false</summary>
        private bool TryWrite(byte[] frame)
        {
            try
            {
                serialPort.Write(frame, 0, frame.Length);
                return true;
            }
            catch (Exception ex) when (ex is OperationCanceledException or
                                           IOException or
                                           InvalidOperationException or
                                           UnauthorizedAccessException)
            {
                HandleSerialDisconnect(ex);
                return false;
            }
        }

        /// <summary>串口异常断开/失联：统一清理连接状态</summary>
        private void HandleSerialDisconnect(Exception ex)
        {
            CloseSerial();   // 注销事件 + 关串口 + 停轮询 + 重置角度基准
            Log($"串口连接已断开：{ex.Message}");
        }

        #endregion

        #region 接收与解析

        /// <summary>串口收包回调（后台线程），解析后切回主线程更新界面</summary>
        private void SerialPort_DataReceived(object? sender, SerialDataReceivedEventArgs e)
        {
            BeginInvoke(new Action(() =>
            {
                try
                {
                    int count = serialPort.BytesToRead;
                    byte[] buffer = new byte[count];
                    serialPort.Read(buffer, 0, count);

                    foreach (byte b in buffer)
                    {
                        byte[]? frame = parser.Feed(b);
                        if (frame != null) HandleFrame(frame);
                    }
                }
                catch (Exception ex) when (ex is OperationCanceledException or
                                               IOException or
                                               InvalidOperationException or
                                               UnauthorizedAccessException)
                {
                    HandleSerialDisconnect(ex);   // 串口断开：清理连接状态
                }
            }));
        }

        /// <summary>按帧类型分发到对应处理</summary>
        private void HandleFrame(byte[] frame)
        {
            byte type = frame[0];
            if (type == (byte)((byte)KnobCmd.GetState | KnobProtocol.RespFlag))
            {
                HandleStateResponse(frame);     // 轮询主用：模式 + 角度合一
            }
            else if (type == (byte)((byte)KnobCmd.GetAngle | KnobProtocol.RespFlag))
            {
                HandleAngleResponse(frame);     // 兼容单独的查询角度
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

        /// <summary>状态响应：解析 [模式][角度]，更新界面并同步控制模式</summary>
        private void HandleStateResponse(byte[] frame)
        {
            // frame = [type][status][mode][角度4B float]
            if ((KnobStatus)frame[1] != KnobStatus.Ok || frame.Length < 7) return;
            _lastRxTime = DateTime.Now;        // 只有收到有效状态响应才刷新心跳
            byte mode = frame[2];
            float angle = BitConverter.ToSingle(frame, 3);

            lblAngle.Text = $"{angle:F1}°";
            DetectRotation(angle);
            SyncModeFromFirmware((KnobControlMode)mode);
        }

        /// <summary>将固件当前模式同步到 UI 复选框（避免互斥回调死循环）</summary>
        private void SyncModeFromFirmware(KnobControlMode mode)
        {
            if (mode == controlMode) return;   // 模式没变，无需更新

            controlMode = mode;                          // 直接设字段，不触发回调
            bool isVolume = mode == KnobControlMode.Volume;
            bool isBright = mode == KnobControlMode.Brightness;

            // 通过 _syncingMode 标志防止 CheckedChanged 回调互相触发
            _syncingMode = true;
            chkVolumeMode.Checked = isVolume;
            chkBrightnessMode.Checked = isBright;
            _syncingMode = false;

            UpdateLimitMode();                           // 同步固件限位状态（模式变化才发）
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
            if (_syncingMode) return;          // 固件同步中，不重复处理

            if (chkVolumeMode.Checked)
            {
                chkBrightnessMode.Checked = false;        // 互斥：切音量时取消亮度
                controlMode = KnobControlMode.Volume;
            }
            else if (controlMode == KnobControlMode.Volume)
            {
                controlMode = KnobControlMode.None;
            }
            ApplyModeChange();                 // 用户操作：同步到下位机
        }

        /// <summary>亮度控制模式：勾选 = 无限旋转 + 步进亮度；与音量模式互斥</summary>
        private void chkBrightnessMode_CheckedChanged(object? sender, EventArgs e)
        {
            if (_syncingMode) return;          // 固件同步中，不重复处理

            if (chkBrightnessMode.Checked)
            {
                chkVolumeMode.Checked = false;            // 互斥：切亮度时取消音量
                controlMode = KnobControlMode.Brightness;
            }
            else if (controlMode == KnobControlMode.Brightness)
            {
                controlMode = KnobControlMode.None;
            }
            ApplyModeChange();                 // 用户操作：同步到下位机
        }

        /// <summary>用户手动改模式：通知下位机切换模式 + 更新限位</summary>
        private void ApplyModeChange()
        {
            Send(KnobProtocol.BuildSetMode(controlMode),
                 $"设置控制模式 {controlMode}");
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

        /// <summary>蜂鸣器开关：勾选/取消 → 同步下位机蜂鸣器状态</summary>
        private void chkBuzzer_CheckedChanged(object? sender, EventArgs e)
        {
            Send(KnobProtocol.BuildSetBuzzer(chkBuzzer.Checked),
                 chkBuzzer.Checked ? "蜂鸣器开启" : "蜂鸣器关闭");
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

        /// <summary>关闭窗口时：清理串口、释放控制器</summary>
        private void Form1_FormClosing(object? sender, FormClosingEventArgs e)
        {
            CloseSerial();   // 注销事件 + 关串口 + 停轮询 + 重置角度基准
            volume.Dispose();
            brightness.Dispose();
        }

        #endregion
    }
}
