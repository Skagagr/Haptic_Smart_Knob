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
            btnQuery = new Button();
            txtLog = new TextBox();
            lblAngle = new Label();
            pollTimer = new System.Windows.Forms.Timer(components);
            btnPreset0 = new Button();
            btnPreset1 = new Button();
            btnPreset2 = new Button();
            btnPreset3 = new Button();
            btnPreset4 = new Button();
            lblAck = new Label();
            SuspendLayout();
            // 
            // btnOpen
            // 
            btnOpen.Location = new Point(12, 48);
            btnOpen.Name = "btnOpen";
            btnOpen.Size = new Size(75, 23);
            btnOpen.TabIndex = 0;
            btnOpen.Text = "打开串口";
            btnOpen.UseVisualStyleBackColor = true;
            btnOpen.Click += btnOpen_Click;
            // 
            // btnQuery
            // 
            btnQuery.Location = new Point(93, 48);
            btnQuery.Name = "btnQuery";
            btnQuery.Size = new Size(75, 23);
            btnQuery.TabIndex = 1;
            btnQuery.Text = "查询角度";
            btnQuery.UseVisualStyleBackColor = true;
            btnQuery.Click += btnQuery_Click;
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
            lblAngle.Location = new Point(237, 48);
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
            lblAck.Location = new Point(587, 26);
            lblAck.Name = "lblAck";
            lblAck.Size = new Size(44, 17);
            lblAck.TabIndex = 9;
            lblAck.Text = "未设置";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 17F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
            Controls.Add(lblAck);
            Controls.Add(btnPreset4);
            Controls.Add(btnPreset3);
            Controls.Add(btnPreset2);
            Controls.Add(btnPreset1);
            Controls.Add(btnPreset0);
            Controls.Add(lblAngle);
            Controls.Add(txtLog);
            Controls.Add(btnQuery);
            Controls.Add(btnOpen);
            Name = "Form1";
            Text = "Form1";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button btnOpen;
        private Button btnQuery;
        private TextBox txtLog;
        private Label lblAngle;
        private System.Windows.Forms.Timer pollTimer;
        private Button btnPreset0;
        private Button btnPreset1;
        private Button btnPreset2;
        private Button btnPreset3;
        private Button btnPreset4;
        private Label lblAck;
    }
}
