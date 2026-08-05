namespace Haptic_Knob_Host
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            components = new System.ComponentModel.Container();
            btnOpen = new Button();
            lblPort = new Label();
            cmbPort = new ComboBox();
            btnRefresh = new Button();
            txtLog = new TextBox();
            lblAngle = new Label();
            pollTimer = new System.Windows.Forms.Timer(components);
            btnPreset0 = new Button();
            btnPreset1 = new Button();
            btnPreset2 = new Button();
            btnPreset3 = new Button();
            btnPreset4 = new Button();
            lblAck = new Label();
            trackBarVolume = new TrackBar();
            lblVolume = new Label();
            chkVolumeMode = new CheckBox();
            trackBarBrightness = new TrackBar();
            lblBrightness = new Label();
            chkBrightnessMode = new CheckBox();
            ((System.ComponentModel.ISupportInitialize)trackBarVolume).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBarBrightness).BeginInit();
            SuspendLayout();
            // 
            // btnOpen
            // 
            btnOpen.Location = new Point(256, 21);
            btnOpen.Name = "btnOpen";
            btnOpen.Size = new Size(75, 23);
            btnOpen.TabIndex = 0;
            btnOpen.Text = "打开串口";
            btnOpen.UseVisualStyleBackColor = true;
            btnOpen.Click += btnOpen_Click;
            // 
            // lblPort
            // 
            lblPort.AutoSize = true;
            lblPort.Location = new Point(12, 22);
            lblPort.Name = "lblPort";
            lblPort.Size = new Size(35, 17);
            lblPort.TabIndex = 1;
            lblPort.Text = "端口:";
            // 
            // cmbPort
            // 
            cmbPort.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbPort.FormattingEnabled = true;
            cmbPort.Location = new Point(55, 19);
            cmbPort.Name = "cmbPort";
            cmbPort.Size = new Size(110, 25);
            cmbPort.TabIndex = 2;
            // 
            // btnRefresh
            // 
            btnRefresh.Location = new Point(175, 19);
            btnRefresh.Name = "btnRefresh";
            btnRefresh.Size = new Size(75, 23);
            btnRefresh.TabIndex = 3;
            btnRefresh.Text = "刷新端口";
            btnRefresh.UseVisualStyleBackColor = true;
            btnRefresh.Click += btnRefresh_Click;
            // 
            // txtLog
            // 
            txtLog.Location = new Point(12, 264);
            txtLog.Multiline = true;
            txtLog.Name = "txtLog";
            txtLog.ReadOnly = true;
            txtLog.ScrollBars = ScrollBars.Vertical;
            txtLog.Size = new Size(776, 174);
            txtLog.TabIndex = 2;
            // 
            // lblAngle
            // 
            lblAngle.Font = new Font("Microsoft YaHei UI", 24F, FontStyle.Regular, GraphicsUnit.Point, 134);
            lblAngle.Location = new Point(258, 58);
            lblAngle.Name = "lblAngle";
            lblAngle.Size = new Size(133, 44);
            lblAngle.TabIndex = 3;
            lblAngle.Text = "0.0°";
            lblAngle.TextAlign = ContentAlignment.MiddleCenter;
            // 
            // pollTimer
            // 
            pollTimer.Interval = 50;
            pollTimer.Tick += pollTimer_Tick;
            // 
            // btnPreset0
            // 
            btnPreset0.Location = new Point(564, 46);
            btnPreset0.Name = "btnPreset0";
            btnPreset0.Size = new Size(90, 25);
            btnPreset0.TabIndex = 4;
            btnPreset0.Tag = "0";
            btnPreset0.Text = "COARSE_6";
            btnPreset0.UseVisualStyleBackColor = true;
            btnPreset0.Click += btnPreset_Click;
            // 
            // btnPreset1
            // 
            btnPreset1.Location = new Point(564, 77);
            btnPreset1.Name = "btnPreset1";
            btnPreset1.Size = new Size(90, 25);
            btnPreset1.TabIndex = 5;
            btnPreset1.Tag = "1";
            btnPreset1.Text = "NORMAL_12";
            btnPreset1.UseVisualStyleBackColor = true;
            btnPreset1.Click += btnPreset_Click;
            // 
            // btnPreset2
            // 
            btnPreset2.Location = new Point(564, 108);
            btnPreset2.Name = "btnPreset2";
            btnPreset2.Size = new Size(90, 25);
            btnPreset2.TabIndex = 6;
            btnPreset2.Tag = "2";
            btnPreset2.Text = "FINE_24";
            btnPreset2.UseVisualStyleBackColor = true;
            btnPreset2.Click += btnPreset_Click;
            // 
            // btnPreset3
            // 
            btnPreset3.Location = new Point(564, 139);
            btnPreset3.Name = "btnPreset3";
            btnPreset3.Size = new Size(90, 25);
            btnPreset3.TabIndex = 7;
            btnPreset3.Tag = "3";
            btnPreset3.Text = "DENSE_48";
            btnPreset3.UseVisualStyleBackColor = true;
            btnPreset3.Click += btnPreset_Click;
            // 
            // btnPreset4
            // 
            btnPreset4.Location = new Point(564, 170);
            btnPreset4.Name = "btnPreset4";
            btnPreset4.Size = new Size(90, 25);
            btnPreset4.TabIndex = 8;
            btnPreset4.Tag = "4";
            btnPreset4.Text = "SMOOTH";
            btnPreset4.UseVisualStyleBackColor = true;
            btnPreset4.Click += btnPreset_Click;
            // 
            // lblAck
            // 
            lblAck.AutoSize = true;
            lblAck.Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point, 134);
            lblAck.Location = new Point(12, 244);
            lblAck.Name = "lblAck";
            lblAck.Size = new Size(44, 17);
            lblAck.TabIndex = 9;
            lblAck.Text = "未设置";
            // 
            // trackBarVolume
            // 
            trackBarVolume.Location = new Point(159, 185);
            trackBarVolume.Maximum = 100;
            trackBarVolume.Name = "trackBarVolume";
            trackBarVolume.Size = new Size(104, 45);
            trackBarVolume.TabIndex = 10;
            trackBarVolume.Value = 50;
            trackBarVolume.Scroll += trackBarVolume_Scroll;
            // 
            // lblVolume
            // 
            lblVolume.AutoSize = true;
            lblVolume.Location = new Point(196, 150);
            lblVolume.Name = "lblVolume";
            lblVolume.Size = new Size(57, 17);
            lblVolume.TabIndex = 11;
            lblVolume.Text = "音量: 0%";
            // 
            // chkVolumeMode
            // 
            chkVolumeMode.AutoSize = true;
            chkVolumeMode.Location = new Point(174, 126);
            chkVolumeMode.Name = "chkVolumeMode";
            chkVolumeMode.Size = new Size(171, 21);
            chkVolumeMode.TabIndex = 12;
            chkVolumeMode.Text = "音量控制模式（无限旋转）";
            chkVolumeMode.UseVisualStyleBackColor = true;
            // 
            // trackBarBrightness
            // 
            trackBarBrightness.Location = new Point(358, 185);
            trackBarBrightness.Maximum = 100;
            trackBarBrightness.Name = "trackBarBrightness";
            trackBarBrightness.Size = new Size(104, 45);
            trackBarBrightness.TabIndex = 13;
            trackBarBrightness.Value = 50;
            trackBarBrightness.Scroll += trackBarBrightness_Scroll;
            // 
            // lblBrightness
            // 
            lblBrightness.AutoSize = true;
            lblBrightness.Location = new Point(395, 150);
            lblBrightness.Name = "lblBrightness";
            lblBrightness.Size = new Size(57, 17);
            lblBrightness.TabIndex = 14;
            lblBrightness.Text = "亮度: 0%";
            // 
            // chkBrightnessMode
            // 
            chkBrightnessMode.AutoSize = true;
            chkBrightnessMode.Location = new Point(373, 126);
            chkBrightnessMode.Name = "chkBrightnessMode";
            chkBrightnessMode.Size = new Size(171, 21);
            chkBrightnessMode.TabIndex = 15;
            chkBrightnessMode.Text = "亮度控制模式（无限旋转）";
            chkBrightnessMode.UseVisualStyleBackColor = true;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 17F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
            Controls.Add(chkBrightnessMode);
            Controls.Add(lblBrightness);
            Controls.Add(trackBarBrightness);
            Controls.Add(chkVolumeMode);
            Controls.Add(lblVolume);
            Controls.Add(trackBarVolume);
            Controls.Add(lblAck);
            Controls.Add(btnPreset4);
            Controls.Add(btnPreset3);
            Controls.Add(btnPreset2);
            Controls.Add(btnPreset1);
            Controls.Add(btnPreset0);
            Controls.Add(lblAngle);
            Controls.Add(txtLog);
            Controls.Add(btnOpen);
            Controls.Add(btnRefresh);
            Controls.Add(cmbPort);
            Controls.Add(lblPort);
            Name = "Form1";
            Text = "Form1";
            ((System.ComponentModel.ISupportInitialize)trackBarVolume).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBarBrightness).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button btnOpen;
        private Label lblPort;
        private ComboBox cmbPort;
        private Button btnRefresh;
        private TextBox txtLog;
        private Label lblAngle;
        private System.Windows.Forms.Timer pollTimer;
        private Button btnPreset0;
        private Button btnPreset1;
        private Button btnPreset2;
        private Button btnPreset3;
        private Button btnPreset4;
        private Label lblAck;
        private TrackBar trackBarVolume;
        private Label lblVolume;
        private CheckBox chkVolumeMode;
        private TrackBar trackBarBrightness;
        private Label lblBrightness;
        private CheckBox chkBrightnessMode;
    }
}
