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
            btnOpen = new Button();
            btnQuery = new Button();
            txtLog = new TextBox();
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
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 17F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
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
    }
}
