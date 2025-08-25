using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;

public class AnimatedButton : Button
{
    private Timer animTimer;
    private static Timer shineTimer;
    private static int shineAlpha = 80;
    private static int shineDirection = 1;

    private float progress = 0;
    private bool animating = false;

    // Track all shining buttons
    private static HashSet<AnimatedButton> shiningButtons = new HashSet<AnimatedButton>();

    // Track all AnimatedButtons (for invalidate)
    private static List<AnimatedButton> AllAnimatedButtons = new List<AnimatedButton>();

    public AnimatedButton()
    {
        this.DoubleBuffered = true;
        animTimer = new Timer();
        animTimer.Interval = 12;
        animTimer.Tick += (s, e) =>
        {
            if (progress < 1f)
            {
                progress += 0.022f;
                this.Invalidate();
            }
            else
            {
                animTimer.Stop();
                animating = false;
                StartShine();
            }
        };

        if (shineTimer == null)
        {
            shineTimer = new Timer();
            shineTimer.Interval = 100;
            shineTimer.Tick += (s, e) =>
            {
                shineAlpha += shineDirection * 3;
                if (shineAlpha >= 160) { shineAlpha = 160; shineDirection = -1; }
                if (shineAlpha <= 60) { shineAlpha = 60; shineDirection = 1; }
                // Invalidate all AnimatedButtons
                foreach (var btn in AllAnimatedButtons)
                    btn.Invalidate();
            };
        }
        lock (AllAnimatedButtons)
        {
            AllAnimatedButtons.Add(this);
        }
    }

    public void StartAnimation()
    {
        progress = 0;
        animating = true;
        shiningButtons.Remove(this); // Ensure not in shine set yet
        this.Invalidate();
        animTimer.Start();
    }

    public void ResetAnimation()
    {
        animTimer.Stop();
        progress = 0;
        animating = false;
        shiningButtons.Remove(this); // Remove from shine set
        this.Invalidate();

        // If none left, stop the timer
        if (shiningButtons.Count == 0)
            shineTimer.Stop();
    }

    private void StartShine()
    {
        shiningButtons.Add(this);
        if (!shineTimer.Enabled)
        {
            shineAlpha = 80;
            shineDirection = 1;
            shineTimer.Start();
        }
    }

    protected override void OnPaint(PaintEventArgs pevent)
    {
        base.OnPaint(pevent);

        // Animate green charging effect while animating,
        // then show shining if in the shiningButtons set.
        if (animating)
        {
            int w = (int)(this.Width * progress);
            using (Brush b = new SolidBrush(Color.FromArgb(120, 0, 255, 0)))
            {
                pevent.Graphics.FillRectangle(b, 0, 0, w, this.Height);
            }
        }
        else if (shiningButtons.Contains(this))
        {
            // Show blinking/shine effect
            using (Brush b = new SolidBrush(Color.FromArgb(shineAlpha, 0, 255, 0)))
            {
                pevent.Graphics.FillRectangle(b, 0, 0, this.Width, this.Height);
            }
        }

        TextRenderer.DrawText(
            pevent.Graphics,
            this.Text,
            this.Font,
            this.ClientRectangle,
            this.ForeColor,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter
        );
    }

    // Clean up on disposal
    protected override void Dispose(bool disposing)
    {
        base.Dispose(disposing);
        lock (AllAnimatedButtons)
        {
            AllAnimatedButtons.Remove(this);
        }
        shiningButtons.Remove(this);
    }
}
