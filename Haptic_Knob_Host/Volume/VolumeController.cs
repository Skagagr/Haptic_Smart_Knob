using NAudio.CoreAudioApi;

namespace Haptic_Knob_Host.Volume
{
    /// <summary>系统音量控制器：读写 Windows 主音量</summary>
    public class VolumeController : IDisposable
    {
        // 必须保存字段引用，防止 COM 对象被垃圾回收：
        // MMDevice 是 RCW 包装的 COM 对象，若只当局部变量用，
        // 函数返回后会被 GC 释放，导致 _volume 报 InvalidComObjectException
        private readonly MMDeviceEnumerator _enumerator;
        private readonly MMDevice _device;
        private readonly AudioEndpointVolume _volume;

        public VolumeController()
        {
            _enumerator = new MMDeviceEnumerator();
            _device = _enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            _volume = _device.AudioEndpointVolume;
        }

        /// <summary>当前音量（0~100）</summary>
        public int VolumePercent =>
            (int)Math.Round(_volume.MasterVolumeLevelScalar * 100);

        /// <summary>设置音量（0~100，自动钳制）</summary>
        public void SetVolumePercent(int percent)
        {
            _volume.MasterVolumeLevelScalar = Math.Clamp(percent, 0, 100) / 100f;
        }

        /// <summary>音量步进（+1 = 音量+1%）</summary>
        public void StepVolume(int delta) => SetVolumePercent(VolumePercent + delta);

        public void Dispose()
        {
            _volume.Dispose();
            _device.Dispose();
            _enumerator.Dispose();
        }
    }
}
