using System.Management;

namespace Haptic_Knob_Host.Volume
{
    /// <summary>
    /// 系统亮度控制器：通过 WMI 读写屏幕亮度。
    /// WMI 调用很慢（一次约 10~100ms），转得快时若在主线程同步调用会卡界面，
    /// 因此：
    ///   - 当前亮度缓存到内存，步进时不再查询 WMI
    ///   - 实际写入放到后台线程，按"取最新值 + 节流"执行
    /// </summary>
    public class BrightnessController : IDisposable
    {
        private const int WriteIntervalMs = 20;   // 两次 WMI 写入的最小间隔

        private readonly object _lock = new();
        private readonly CancellationTokenSource _cts = new();
        private int _cachedBrightness = -1;       // 缓存当前亮度（-1 = 尚未从 WMI 读取）
        private int _pendingBrightness = -1;      // 待写入的目标亮度（-1 = 无待写）
        private bool _workerRunning;

        /// <summary>当前亮度（0~100）；读取失败返回 -1</summary>
        public int BrightnessPercent
        {
            get
            {
                if (_cachedBrightness < 0) LoadFromWmi();
                return _cachedBrightness;
            }
        }

        /// <summary>设置亮度（0~100，自动钳制）；异步写入，不阻塞调用线程</summary>
        public void SetBrightnessPercent(int percent)
        {
            int p = Math.Clamp(percent, 0, 100);
            _cachedBrightness = p;               // 界面显示立即更新
            QueueWrite(p);
        }

        /// <summary>亮度步进（+1 = 亮度+1%）</summary>
        public void StepBrightness(int delta)
        {
            if (_cachedBrightness < 0) LoadFromWmi();
            if (_cachedBrightness < 0) return;
            SetBrightnessPercent(_cachedBrightness + delta);
        }

        public void Dispose() => _cts.Cancel();

        // ===== 内部实现 =====

        /// <summary>首次使用时从 WMI 读取当前亮度并缓存</summary>
        private void LoadFromWmi()
        {
            try
            {
                using var searcher = new ManagementObjectSearcher(
                    "root\\WMI", "SELECT CurrentBrightness FROM WmiMonitorBrightness");
                foreach (ManagementObject obj in searcher.Get())
                {
                    _cachedBrightness = Convert.ToInt32(obj["CurrentBrightness"]);
                    return;
                }
            }
            catch { /* 设备不支持亮度时忽略 */ }
        }

        /// <summary>登记一次写入：只保留最新目标值，由后台线程节流执行</summary>
        private void QueueWrite(int percent)
        {
            lock (_lock)
            {
                _pendingBrightness = percent;
            }
            if (!_workerRunning)
            {
                _workerRunning = true;
                Task.Run(WorkerLoop);
            }
        }

        /// <summary>后台写循环：取最新值写入，然后节流等待；无待写则退出</summary>
        private void WorkerLoop()
        {
            var token = _cts.Token;
            try
            {
                while (!token.IsCancellationRequested)
                {
                    int p;
                    lock (_lock)
                    {
                        if (_pendingBrightness < 0)
                        {
                            _workerRunning = false;
                            return;
                        }
                        p = _pendingBrightness;
                        _pendingBrightness = -1;
                    }
                    WriteBrightness(p);
                    Thread.Sleep(WriteIntervalMs);
                }
            }
            catch { }
            finally
            {
                lock (_lock) { _workerRunning = false; }
            }
        }

        private void WriteBrightness(int percent)
        {
            try
            {
                using var searcher = new ManagementObjectSearcher(
                    "root\\WMI", "SELECT * FROM WmiMonitorBrightnessMethods");
                foreach (ManagementObject obj in searcher.Get())
                {
                    obj.InvokeMethod("WmiSetBrightness", new object[] { 0, percent });
                    return;
                }
            }
            catch { /* 设备不支持亮度时忽略 */ }
        }
    }
}
