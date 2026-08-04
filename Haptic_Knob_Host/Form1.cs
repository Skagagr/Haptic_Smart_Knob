using System.IO.Ports;

namespace Haptic_Knob_Host
{
    public partial class Form1 : Form
    {
        private SerialPort serialPort = new SerialPort("COM7", 115200);     // 创建串口对象

        public Form1()
        {
            // 调用设计器生成的代码：创建窗口上所有控件
            InitializeComponent();

            // 注册回调：串口一收到数据，系统就自动调用 SerialPort_DataReceived 函数
            serialPort.DataReceived += SerialPort_DataReceived;

        }


        /// <summary> 点击"打开串口"按钮时执行 </summary>
        private void btnOpen_Click(object sender, EventArgs e)
        {
            if (!serialPort.IsOpen)      // IsOpen = 串口当前是否已打开
            {
                serialPort.Open();       // 真正打开串口，占用这个端口
                btnOpen.Text = "关闭串口"; // 改按钮文字：提示下次点击是"关闭"
            }
            else
            {
                serialPort.Close();      // 关闭串口，释放端口
                btnOpen.Text = "打开串口";
            }
        }

        /// <summary> 点击"查询角度"按钮时执行：发送一帧查询命令给旋钮 </summary>
        private void btnQuery_Click(object sender, EventArgs e)
        {
            // 查询角度命令帧（5 字节，CRC 已算好写死）：
            //   AA 55 = 同步头（帧开始标志）
            //   03    = 命令类型（0x03 = 查询角度）
            //   00    = 载荷长度（此命令没有数据，长度 0）
            //   3F    = CRC8 校验
            byte[] frame = { 0xAA, 0x55, 0x03, 0x00, 0x3F };
            serialPort.Write(frame, 0, frame.Length);     // 把 5 字节发出去
            txtLog.Text += $"[{DateTime.Now:HH:mm:ss.fff}] ---> 发送: AA 55 03 00 3F" + Environment.NewLine;
        }

        /// <summary>
        /// 串口收到数据时自动执行
        /// 它在"后台线程"运行（系统自动唤醒），而控件只能被"主线程"修改
        /// 所以不能直接碰 txtLog —— 必须用 BeginInvoke 把活扔回主线程做
        /// 相当于固件里"中断中不能乱动主循环共享数据，要置标志等主循环来取"
        /// </summary>
        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            // BeginInvoke(任务) = 把下面这段"任务"排队扔给主线程执行，然后立即返回
            BeginInvoke(new Action(() =>
            {

                int count = serialPort.BytesToRead;        // 缓冲区里现在有几字节
                byte[] buffer = new byte[count];           // 建一个同样大小的数组
                serialPort.Read(buffer, 0, count);         // 把数据读进数组
                // BitConverter.ToString = 字节数组转成 "AA-55-83-..." 格式方便看
                txtLog.Text += $"[{DateTime.Now:HH:mm:ss.fff}] <--- 接收: " + BitConverter.ToString(buffer) + Environment.NewLine;
            }));
        }
    }
}
