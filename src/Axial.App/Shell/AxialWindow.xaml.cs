using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace Axial.App.Shell;

/// <summary>
/// Axial-Hauptfenster: Mica-Hintergrund, randlose Titelleiste.
/// </summary>
public sealed partial class AxialWindow : Window
{
    public AxialWindow()
    {
        InitializeComponent();

        // Liquid-Glass-Basis: Mica (mit Fallback, sollte es fehlen)
        try
        {
            SystemBackdrop = new MicaBackdrop { Kind = Microsoft.UI.Composition.SystemBackdrops.MicaKind.BaseAlt };
        }
        catch
        {
            SystemBackdrop = new DesktopAcrylicBackdrop();
        }

        ExtendsContentIntoTitleBar = true;
        AppWindow.TitleBar.PreferredHeightOption = TitleBarHeightOption.Tall;

        var scale = GetDpiScale();
        int w = (int)(1180 * scale), h = (int)(800 * scale);
        AppWindow.Resize(new SizeInt32(w, h));
        CenterOnScreen();
    }

    private double GetDpiScale()
    {
        try
        {
            var displayArea = DisplayArea.GetFromPoint(
                new PointInt32(0, 0), DisplayAreaFallback.Nearest);
            return displayArea is null ? 1.0 : 1.0;  // DPI-Skalierung via XAML automatisch
        }
        catch
        {
            return 1.0;
        }
    }

    private void CenterOnScreen()
    {
        try
        {
            var area = DisplayArea.GetFromPoint(
                new PointInt32(0, 0), DisplayAreaFallback.Primary);
            if (area is null) return;
            var size = AppWindow.Size;
            int x = Math.Max(area.WorkArea.X, (area.WorkArea.Width - size.Width) / 2);
            int y = Math.Max(area.WorkArea.Y, (area.WorkArea.Height - size.Height) / 2);
            AppWindow.Move(new PointInt32(x, y));
        }
        catch
        {
            // Zentrierung ist Kosmetik — nie ein Fehler
        }
    }
}
