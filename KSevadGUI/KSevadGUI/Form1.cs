using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using System.Security.Principal; // Add this at the top if not present

namespace KSevad
{
    public partial class Form1 : Form
    {
        // Map of driver name -> loaded status
        Dictionary<string, bool> loaded = new Dictionary<string, bool>();

        // List of driver keys for layout and lookup
        List<string> driverKeys = new List<string> { "reg", "proc", "files", "pci", "devices" };

        // For button controls
        Dictionary<string, Button> driverButtons = new Dictionary<string, Button>();
        Label adminLabel;

        public Form1()
        {
            //var names = System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceNames();
            //MessageBox.Show(string.Join("\n", names));
            InitializeComponent();
            this.Text = "KSevad Driver Loader";
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.ShowIcon = false;
            this.ClientSize = new Size(440, 350);
            this.BackColor = Color.Black;
            this.Font = new Font("Segoe UI", 10);



            // Logo (ICO as image)
            PictureBox picLogo = new PictureBox();
            picLogo.Size = new Size(80, 80);
            picLogo.Location = new Point((this.ClientSize.Width - 80) / 2, 60);
            picLogo.SizeMode = PictureBoxSizeMode.Zoom;

            // If your project's root namespace is KSevad and the file is at the root, this is correct.
            string resourceName = "KSevadGUI.Logo.png";
            var asm = Assembly.GetExecutingAssembly();
            using (Stream s = asm.GetManifestResourceStream(resourceName))
            {
                if (s != null)
                {
                    picLogo.Image = Image.FromStream(s);
                }
                else
                {
                    // fallback image if not found
                    Bitmap bmp = new Bitmap(124, 124);
                    using (Graphics g = Graphics.FromImage(bmp))
                    {
                        g.Clear(Color.White);
                        using (Pen p = new Pen(Color.Black, 3))
                            g.DrawEllipse(p, 5, 10, 70, 50);
                        using (Font font = new Font("Segoe UI", 18, FontStyle.Regular))
                            g.DrawString("ico", font, Brushes.Black, 16, 26);
                    }
                    picLogo.Image = bmp;
                }
            }
            this.Controls.Add(picLogo);

            /*
            // Separator under logo
            Panel sep = new Panel();
            sep.BackColor = Color.Black;
            sep.Size = new Size(28, 2);
            sep.Location = new Point((this.ClientSize.Width - sep.Width) / 2, picLogo.Bottom + 5);
            this.Controls.Add(sep);
            */

            // Button grid area
            int btnWidth = 120, btnHeight = 38, btnMarginX = 35, btnMarginY = 12;
            int gridStartY = picLogo.Bottom + 35;
            int gridStartX = (this.ClientSize.Width - (2 * btnWidth) - btnMarginX) / 2;

            // Row 1
            string[] row1 = { "reg", "proc" };
            for (int i = 0; i < row1.Length; ++i)
            {
                Button btn = CreateDriverButton(row1[i]);
                btn.Location = new Point(gridStartX + i * (btnWidth + btnMarginX), gridStartY);
                this.Controls.Add(btn);
            }

            // Row 2
            string[] row2 = { "files", "pci" };
            for (int i = 0; i < row2.Length; ++i)
            {
                Button btn = CreateDriverButton(row2[i]);
                btn.Location = new Point(gridStartX + i * (btnWidth + btnMarginX), gridStartY + btnHeight + btnMarginY);
                this.Controls.Add(btn);
            }

            // Bottom centered button
            Button btnDevices = CreateDriverButton("devices");
            btnDevices.Location = new Point((this.ClientSize.Width - btnWidth) / 2, gridStartY + 2 * (btnHeight + btnMarginY));
            this.Controls.Add(btnDevices);

            // Admin/conflict warning label at bottom
            adminLabel = new Label();
            adminLabel.Text = "Run this program as Administrator!";
            adminLabel.ForeColor = Color.Red;
            adminLabel.Font = new Font("Segoe UI", 10, FontStyle.Bold);
            adminLabel.AutoSize = false;
            adminLabel.TextAlign = ContentAlignment.MiddleCenter;
            adminLabel.Dock = DockStyle.Bottom;
            adminLabel.Height = 32;
            adminLabel.Visible = !IsRunningAsAdmin();
            this.Controls.Add(adminLabel);
        }

        public static bool IsRunningAsAdmin()
        {
            using (WindowsIdentity identity = WindowsIdentity.GetCurrent())
            {
                WindowsPrincipal principal = new WindowsPrincipal(identity);
                return principal.IsInRole(WindowsBuiltInRole.Administrator);
            }
        }

        private Button CreateDriverButton(string key)
        {
            var info = Program.driverTable[key];
            var btn = new AnimatedButton(); // <- Use AnimatedButton
            btn.Text = info.ServiceName;
            btn.Name = "btn_" + key;
            btn.Size = new Size(120, 38);
            btn.FlatStyle = FlatStyle.Flat;
            btn.FlatAppearance.BorderSize = 2;
            btn.Font = new Font("Segoe UI", 10, FontStyle.Regular);
            btn.BackColor = Color.FromArgb(30, 30, 30);
            btn.ForeColor = Color.White;

            loaded[key] = false;
            driverButtons[key] = btn;

            btn.Click += (s, e) => {
                ToggleDriver(key);
            };

            UpdateButtonState(key);

            return btn;
        }

        private void UpdateButtonState(string key)
        {
            var btn = driverButtons[key] as AnimatedButton;
            if (loaded[key])
            {
                btn.FlatAppearance.BorderColor = ColorTranslator.FromHtml("#A01506"); // Red border
                btn?.StartAnimation();
            }
            else
            {
               btn.ForeColor = Color.White;
               btn.FlatAppearance.BorderColor = Color.White;
               btn?.ResetAnimation();
            }
        }

        private void ToggleDriver(string key)
        {

            var info = Program.driverTable[key];
            string driverPath = Path.Combine(Program.GetExeDirectory(), "Drivers", info.DriverPath);

            if (!loaded[key])
            {
                if (Program.LoadDriver(info.ServiceName, driverPath))
                    loaded[key] = true;
            }
            else
            {
                if (Program.UnloadDriver(info.ServiceName))
                    loaded[key] = false;
            }
            UpdateButtonState(key);

            adminLabel.Visible = !IsRunningAsAdmin();
            if (adminLabel.Visible)
                adminLabel.Text = "Run this program as Administrator!";
        }
        /*
        private void UpdateButtonColor(string key)
        {
            var btn = driverButtons[key];
            if (loaded[key])
            {
                btn.FlatAppearance.BorderColor = Color.LimeGreen;
            }
            else
            {
                btn.FlatAppearance.BorderColor = Color.Black;
            }
        }
        */
        private void Form1_Load(object sender, EventArgs e)
        {
            // You can leave this empty or add initialization code if needed
        }
    }
}
